// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#include "appimagebackend.h"
#include "appimageiconprovider.h"
#include "corpsemodel.h"
#include "dashboardwindow.h"
#include "logging.h"
#include "../core/appimagemanager.h"
#include "../core/appimagereader.h"
#include "../core/appsettings.h"
#include "../core/checkresults.h"
#include "../core/launchenvironment.h"
#include "../core/updatesource.h"

#include <KFormat>
#include <KJob>
#include <KIO/CopyJob>
#include <KIO/RestoreJob>
#include <KLocalizedString>
#include <KNotification>

#include <QDir>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QIcon>
#include <QProcess>
#include <QtConcurrent/QtConcurrent>
#include <QUrl>

#ifdef HAVE_CANBERRA
#include <canberra.h>
#endif

AppImageBackend::AppImageBackend(const QString &appImagePath,
                                 AppImageIconProvider *iconProvider,
                                 QObject *parent)
    : QObject(parent)
    , m_appImagePath(appImagePath)
    , m_iconProvider(iconProvider)
    , m_corpseModel(new CorpseModel(this))
{
    qRegisterMetaType<AppImageInfo>();

    m_info.originalName = QFileInfo(appImagePath).fileName();

    const QString applicationsDir = AppSettings::instance()->applicationsPath();
    m_isInstalled = (QFileInfo(appImagePath).absolutePath() == applicationsDir);

    startMetadataLoad();
}

void AppImageBackend::startMetadataLoad()
{
    auto *watcher = new QFutureWatcher<AppImageInfo>(this);
    connect(watcher, &QFutureWatcher<AppImageInfo>::finished, this, [this, watcher]() {
        onMetadataReady(watcher->result());
        watcher->deleteLater();
    });
    watcher->setFuture(QtConcurrent::run([path = m_appImagePath]() {
        return AppImageReader(path).read();
    }));
}

void AppImageBackend::onMetadataReady(const AppImageInfo &info)
{
    m_info = info;
    m_info.originalName = QFileInfo(m_appImagePath).fileName();

    if (!info.iconData.isEmpty())
        m_iconProvider->setIconData(info.iconData, info.iconExt);

    m_hasDesktopLink = AppImageManager::isDesktopLinkEnabled(m_appImagePath, m_info);

    if (!info.appId.isEmpty() && QIcon::hasThemeIcon(info.appId))
        m_cachedIconSource = QLatin1String("image://icon/") + info.appId;
    else if (!info.iconData.isEmpty())
        m_cachedIconSource = QStringLiteral("image://appimage/icon");

    m_metadataLoaded = true;

    Q_EMIT infoChanged();
    Q_EMIT desktopLinkChanged();
    Q_EMIT metadataLoadedChanged();

    if (!m_isInstalled)
        findInstalledCopy();
}

namespace {
struct InstalledCopy {
    QString path;
    AppImageInfo info;
};
}

// The copy of this app already in Applications: a file with the same name (installing would
// overwrite it), else the newest copy with the same app id (or, without ids, the same name).
// Blocking; the reader is cache-backed, so this is quick for a library the dashboard has seen.
static InstalledCopy installedCopy(const QString &dir, const QString &selfPath, const AppImageInfo &self)
{
    const QString ownName = QFileInfo(selfPath).fileName();
    InstalledCopy best;
    const QFileInfoList files = QDir(dir).entryInfoList(kAppImageFilters(), QDir::Files);
    for (const QFileInfo &file : files) {
        const QString path = file.absoluteFilePath();
        if (path == QFileInfo(selfPath).absoluteFilePath())
            continue;
        const AppImageInfo info = AppImageReader(path).read();
        if (file.fileName() == ownName)
            return { path, info };
        if (!info.isValid)
            continue;
        const bool sameApp = (!self.appId.isEmpty() && !info.appId.isEmpty())
            ? info.appId == self.appId
            : !self.appName.isEmpty() && info.appName.compare(self.appName, Qt::CaseInsensitive) == 0;
        if (sameApp && (best.path.isEmpty()
                        || isNewerVersion(normalizeVersion(info.version), normalizeVersion(best.info.version))))
            best = { path, info };
    }
    return best;
}

void AppImageBackend::findInstalledCopy()
{
    auto *watcher = new QFutureWatcher<InstalledCopy>(this);
    connect(watcher, &QFutureWatcher<InstalledCopy>::finished, this, [this, watcher]() {
        watcher->deleteLater();
        const InstalledCopy copy = watcher->result();
        if (copy.path.isEmpty())
            return;
        m_existingPath = copy.path;
        m_existingInfo = copy.info;
        m_existingSameFile = QFileInfo(copy.path).fileName() == QFileInfo(m_appImagePath).fileName();

        const QString mine = normalizeVersion(m_info.version);
        const QString theirs = normalizeVersion(copy.info.version);
        if (mine.isEmpty() || theirs.isEmpty())
            m_existingState = VersionUnknown;
        else if (mine == theirs)
            m_existingState = SameVersion;
        else if (isNewerVersion(mine, theirs))
            m_existingState = ThisNewer;
        else if (isNewerVersion(theirs, mine))
            m_existingState = ThisOlder;
        else
            m_existingState = VersionUnknown;
        Q_EMIT existingChanged();
    });
    watcher->setFuture(QtConcurrent::run([dir = AppSettings::instance()->applicationsPath(),
                                          path = m_appImagePath, info = m_info]() {
        return installedCopy(dir, path, info);
    }));
}

// ──────────────────────────────────────────────────────────────────────────────
// Q_INVOKABLE
// ──────────────────────────────────────────────────────────────────────────────

void AppImageBackend::openDashboard()
{
    DashboardWindow::open();
}

QString AppImageBackend::formatBytes(qint64 bytes) const
{
    static const KFormat fmt;
    return fmt.formatByteSize(static_cast<double>(bytes));
}

// ──────────────────────────────────────────────────────────────────────────────
// Slots called from QML
// ──────────────────────────────────────────────────────────────────────────────

void AppImageBackend::launchAppImage()
{
    if (!launchWithEnvironment(m_appImagePath))
        Q_EMIT errorOccurred(i18n("Could not launch the application."));
}

void AppImageBackend::beginInstall(bool replace)
{
    m_replace = replace && m_existingState != NotInstalled;

    if (!AppSettings::instance()->verifySignatures()) {
        // Verification disabled — skip straight to install.
        doInstall();
        return;
    }

    auto *watcher = new QFutureWatcher<int>(this);
    connect(watcher, &QFutureWatcher<int>::finished, this, [this, watcher]() {
        watcher->deleteLater();
        Q_EMIT signatureCheckReady(watcher->result());
    });
    watcher->setFuture(QtConcurrent::run([path = m_appImagePath]() {
        return static_cast<int>(AppImageReader::verifySignature(path));
    }));
}

void AppImageBackend::doInstall()
{
    if (m_replace && m_existingSameFile) {
        // The installed copy has this name: it goes to the Trash (with its menu entry) first.
        m_oldHadLink = AppImageManager::isDesktopLinkEnabled(m_existingPath, m_existingInfo);
        KJob *job = AppImageManager::removeItems(QUrl::fromLocalFile(m_existingPath), m_existingInfo, {});
        connect(job, &KJob::result, this, [this](KJob *j) {
            if (j->error()) {
                qCWarning(AIM_LOG) << "Replace failed:" << j->errorString();
                Q_EMIT errorOccurred(j->errorString());
                return;
            }
            startInstallJob();
        });
        job->start();
        return;
    }
    startInstallJob();
}

void AppImageBackend::startInstallJob()
{
    auto *job = AppImageManager::installAppImage(QUrl::fromLocalFile(m_appImagePath),
                                                AppSettings::instance()->applicationsPath());
    connect(job, &KJob::result, this, &AppImageBackend::onInstallJobFinished);
    job->start();
}

void AppImageBackend::toggleDesktopLink(bool enable)
{
    const bool ok = enable
        ? AppImageManager::createDesktopLink(m_appImagePath, m_info)
        : AppImageManager::removeDesktopLink(m_appImagePath, m_info);

    if (!ok) {
        Q_EMIT errorOccurred(enable
            ? i18n("Could not create launcher entry.")
            : i18n("Could not remove launcher entry."));
        return;
    }

    m_hasDesktopLink = enable;
    Q_EMIT desktopLinkChanged();
}

void AppImageBackend::findCorpses()
{
    m_isFindingCorpses = true;
    m_corpseModel->clear();
    Q_EMIT busyChanged();

    auto *watcher = new QFutureWatcher<QList<AppImageManager::CorpseEntry>>(this);
    connect(watcher, &QFutureWatcher<QList<AppImageManager::CorpseEntry>>::finished,
            this, [this, watcher]() {
        m_corpseModel->setCorpses(watcher->result());
        m_isFindingCorpses = false;
        watcher->deleteLater();
        Q_EMIT busyChanged();
    });
    watcher->setFuture(QtConcurrent::run([info = m_info]() {
        return AppImageManager::findCorpses(info);
    }));
}

void AppImageBackend::removeAppImageAndCorpses(const QStringList &corpsePaths, bool includeAppImage)
{
    m_isRemovingItems = true;
    Q_EMIT busyChanged();

    QList<QUrl> corpseUrls;
    for (const QString &p : corpsePaths)
        corpseUrls << QUrl::fromLocalFile(p);

    KJob *job = nullptr;
    if (includeAppImage) {
        job = AppImageManager::removeItems(QUrl::fromLocalFile(m_appImagePath), m_info, corpseUrls);
    } else if (!corpseUrls.isEmpty()) {
        job = KIO::trash(corpseUrls);
    } else {
        m_isRemovingItems = false;
        Q_EMIT busyChanged();
        Q_EMIT uninstallFinished();
        return;
    }

    // Per-job collection of trash:/ destinations so the "Restore" action can
    // target this remove only; sharing one member across in-flight jobs raced.
    auto trashedUrls = QSharedPointer<QList<QUrl>>::create();
    if (auto *copyJob = qobject_cast<KIO::CopyJob *>(job)) {
        connect(copyJob, &KIO::CopyJob::copyingDone, this,
                [trashedUrls](KIO::Job *, const QUrl &, const QUrl &to, const QDateTime &, bool, bool) {
                    *trashedUrls << to;
                });
    }

    connect(job, &KJob::result, this, [this, trashedUrls](KJob *j) {
        onRemoveJobFinished(j, *trashedUrls);
    });
    job->start();
}

// ──────────────────────────────────────────────────────────────────────────────
// KJob result handlers
// ──────────────────────────────────────────────────────────────────────────────

void AppImageBackend::onInstallJobFinished(KJob *job)
{
    if (job->error()) {
        qCWarning(AIM_LOG) << "Install failed:" << job->errorString();
        Q_EMIT errorOccurred(job->errorString());
        return;
    }

    m_appImagePath = AppSettings::instance()->applicationsPath()
                   + QLatin1Char('/')
                   + QFileInfo(m_appImagePath).fileName();
    m_isInstalled = true;
    const bool link = m_replace ? finishReplace() : true;
    m_hasDesktopLink = link && AppImageManager::createDesktopLink(m_appImagePath, m_info);
    Q_EMIT installedChanged();
    Q_EMIT desktopLinkChanged();

    if (AppSettings::instance()->showNotifications()) {
#ifdef HAVE_CANBERRA
        static ca_context *s_caCtx = nullptr;
        if (!s_caCtx)
            ca_context_create(&s_caCtx);
        if (s_caCtx)
            ca_context_play(s_caCtx, 0, CA_PROP_EVENT_ID, "outcome-success", nullptr);
#endif
        KNotification::event(QStringLiteral("installed"),
                             i18n("AppImage installed"),
                             i18n("%1 was moved to Applications.", m_info.appName),
                             QStringLiteral("application-x-executable"),
                             KNotification::CloseOnTimeout);
    }
}

// After installing over another copy: its settings (keyed by file name) move to this file and
// it goes to the Trash. Returns whether it was in the app menu, so this one is too.
bool AppImageBackend::finishReplace()
{
    const QString oldName = QFileInfo(m_existingPath).fileName();
    saveCheckResult(oldName, {});  // the result was for the replaced version

    if (m_existingSameFile) {
        saveInstalledTag(m_appImagePath, {});  // belonged to the replaced build
        return m_oldHadLink;
    }

    const UpdateSource source = loadUpdateSource(m_existingPath);
    saveUpdateSource(m_appImagePath, source);  // the installed tag is not carried
    saveUpdateSource(m_existingPath, {});
    saveEnvironment(m_appImagePath, loadEnvironment(m_existingPath));
    saveEnvironment(m_existingPath, {});

    // removeItems() drops the old menu entry; it must go before ours is written, as both
    // can be the same app-id file.
    const bool hadLink = AppImageManager::isDesktopLinkEnabled(m_existingPath, m_existingInfo);
    KJob *job = AppImageManager::removeItems(QUrl::fromLocalFile(m_existingPath), m_existingInfo, {});
    connect(job, &KJob::result, this, [this](KJob *j) {
        if (j->error()) {
            qCWarning(AIM_LOG) << "Could not trash the replaced copy:" << j->errorString();
            Q_EMIT errorOccurred(j->errorString());
        }
    });
    job->start();
    return hadLink;
}

void AppImageBackend::onRemoveJobFinished(KJob *job, const QList<QUrl> &trashedUrls)
{
    m_isRemovingItems = false;
    Q_EMIT busyChanged();

    if (job->error()) {
        qCWarning(AIM_LOG) << "Remove failed:" << job->errorString();
        Q_EMIT errorOccurred(job->errorString());
        return;
    }

    if (AppSettings::instance()->showNotifications()) {
        auto *notification = new KNotification(QStringLiteral("removed"),
                                               KNotification::CloseOnTimeout);
        notification->setTitle(i18n("AppImage removed"));
        notification->setText(i18n("%1 was moved to Trash.", m_info.appName));
        notification->setIconName(QStringLiteral("edit-delete"));

        if (!trashedUrls.isEmpty()) {
            auto *restoreAction = notification->addAction(i18n("Restore"));
            connect(restoreAction, &KNotificationAction::activated, this, [trashedUrls]() {
                KIO::restoreFromTrash(trashedUrls)->start();
            });
        }

        notification->sendEvent();
    }

    Q_EMIT uninstallFinished();
}

// ──────────────────────────────────────────────────────────────────────────────
// Property helpers
// ──────────────────────────────────────────────────────────────────────────────

QString AppImageBackend::displayName() const
{
    if (!m_info.appName.isEmpty() && m_info.appName != m_info.cleanName)
        return m_info.appName;
    return m_info.cleanName.isEmpty()
           ? QFileInfo(m_appImagePath).fileName()
           : m_info.cleanName;
}

QString AppImageBackend::formattedSize() const
{
    return formatBytes(m_info.fileSize);
}

