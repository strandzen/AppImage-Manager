// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#include "appimagebackend.h"
#include "appimageiconprovider.h"
#include "corpsemodel.h"
#include "logging.h"
#include "../core/appimagemanager.h"
#include "../core/appimagereader.h"
#include "../core/appsettings.h"

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
}

// ──────────────────────────────────────────────────────────────────────────────
// Q_INVOKABLE
// ──────────────────────────────────────────────────────────────────────────────

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
    QProcess::startDetached(m_appImagePath, {});
}

void AppImageBackend::installAppImage()
{
    auto *job = AppImageManager::installAppImage(QUrl::fromLocalFile(m_appImagePath),
                                                AppSettings::instance()->applicationsPath());
    connect(job, &KJob::result, this, &AppImageBackend::onInstallJobFinished);
    job->start();
}

void AppImageBackend::toggleDesktopLink(bool enable)
{
    if (enable)
        AppImageManager::createDesktopLink(m_appImagePath, m_info);
    else
        AppImageManager::removeDesktopLink(m_appImagePath, m_info);

    m_hasDesktopLink = enable;
    Q_EMIT desktopLinkChanged();
}

void AppImageBackend::findCorpses()
{
    m_isFindingCorpses = true;
    m_corpseModel->clear();
    Q_EMIT busyChanged();

    auto *watcher = new QFutureWatcher<QList<QPair<QString, qint64>>>(this);
    connect(watcher, &QFutureWatcher<QList<QPair<QString, qint64>>>::finished,
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

    // Collect trash:/ destination URLs so the notification can offer "Restore"
    m_lastTrashedUrls.clear();
    if (auto *copyJob = qobject_cast<KIO::CopyJob *>(job)) {
        connect(copyJob, &KIO::CopyJob::copyingDone, this,
                [this](KIO::Job *, const QUrl &, const QUrl &to, const QDateTime &, bool, bool) {
                    m_lastTrashedUrls << to;
                });
    }

    connect(job, &KJob::result, this, &AppImageBackend::onRemoveJobFinished);
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
    Q_EMIT installedChanged();

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

void AppImageBackend::onRemoveJobFinished(KJob *job)
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

        if (!m_lastTrashedUrls.isEmpty()) {
            const QList<QUrl> urls = m_lastTrashedUrls;
            auto *restoreAction = notification->addAction(i18n("Restore"));
            connect(restoreAction, &KNotificationAction::activated, this, [urls]() {
                KIO::restoreFromTrash(urls)->start();
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

