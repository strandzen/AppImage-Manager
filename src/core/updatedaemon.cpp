// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#include "updatedaemon.h"
#include "appimageinfo.h"
#include "appsettings.h"
#include "appimagereader.h"
#include "githubreleasechecker.h"
#include <QDBusConnection>
#include <QDir>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QFutureWatcher>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QStandardPaths>
#include <KLocalizedString>
#include <KNotification>
#include <QProcess>
#include <QtConcurrent/QtConcurrent>

#ifdef HAVE_KSTATUSNOTIFIERITEM
#include <KStatusNotifierItem>
#endif

// freq values: 0 = Never, 1 = Daily, 2 = Weekly, 3 = Monthly, 4 = Custom (customDays)
static std::chrono::hours intervalForFrequency(int freq, int customDays)
{
    switch (freq) {
    case 1: return std::chrono::hours(24);
    case 2: return std::chrono::hours(24 * 7);
    case 3: return std::chrono::hours(24 * 30);
    case 4: return std::chrono::hours(24 * customDays);
    default: return std::chrono::hours(24);
    }
}


UpdateDaemon::UpdateDaemon(QObject *parent)
    : QObject(parent)
    , m_timer(new QTimer(this))
    , m_networkManager(new QNetworkAccessManager(this))
{
    connect(m_timer, &QTimer::timeout, this, &UpdateDaemon::checkUpdates);
    connect(AppSettings::instance(), &AppSettings::updateFrequencyChanged, this, [this]() {
        const int freq = AppSettings::instance()->updateFrequency();
        m_timer->setInterval(intervalForFrequency(freq, AppSettings::instance()->customUpdateDays()));
        if (m_timer->isActive())
            m_timer->start();
    });
    watchDownloads();
}

void UpdateDaemon::start()
{
    QDBusConnection::sessionBus().registerService(QStringLiteral("io.github.appimagemanager.Daemon"));
    const int freq = AppSettings::instance()->updateFrequency();
    m_timer->setInterval(intervalForFrequency(freq, AppSettings::instance()->customUpdateDays()));
    checkUpdates();
    m_timer->start();
}

void UpdateDaemon::watchDownloads()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (dir.isEmpty())
        return;

    const QStringList &filters = kAppImageFilters();
    for (const QFileInfo &fi : QDir(dir).entryInfoList(filters, QDir::Files))
        m_knownDownloads.insert(fi.absoluteFilePath());

    m_downloadWatcher = new QFileSystemWatcher(this);
    m_downloadWatcher->addPath(dir);
    connect(m_downloadWatcher, &QFileSystemWatcher::directoryChanged,
            this, &UpdateDaemon::onDownloadDirChanged);
}

void UpdateDaemon::onDownloadDirChanged()
{
    if (!AppSettings::instance()->watchDownloads())
        return;

    const QString dir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    const QStringList &filters = kAppImageFilters();
    const QFileInfoList entries = QDir(dir).entryInfoList(filters, QDir::Files);

    for (const QFileInfo &fi : entries) {
        const QString path = fi.absoluteFilePath();
        if (m_knownDownloads.contains(path))
            continue;
        m_knownDownloads.insert(path);

        QString displayName = AppImageReader::cleanName(fi.fileName());
        if (displayName.endsWith(QLatin1String(".AppImage"), Qt::CaseInsensitive))
            displayName.chop(9);
        // Strip trailing version number (e.g. "Krita 5.3.1" → "Krita")
        displayName.remove(QRegularExpression(QStringLiteral(R"(\s+\d[\d.]*$)")));
        displayName = displayName.trimmed();

        auto *n = new KNotification(QStringLiteral("downloaded"), KNotification::Persistent, this);
        n->setTitle(i18n("%1 downloaded", displayName));
        n->setIconName(QStringLiteral("application-x-executable"));
        auto *action = n->addAction(i18n("Manage"));
        connect(action, &KNotificationAction::activated, this, [path]() {
            QProcess::startDetached(QStringLiteral("appimagemanager"), { path });
        });
        n->sendEvent();
    }

    QSet<QString> current;
    for (const QFileInfo &fi : entries)
        current.insert(fi.absoluteFilePath());
    m_knownDownloads.intersect(current);
}

void UpdateDaemon::checkUpdates()
{
    if (AppSettings::instance()->updateFrequency() == 0)
        return;

    const QString dir = AppSettings::instance()->applicationsPath();
    const QStringList &filters = kAppImageFilters();
    const QFileInfoList files = QDir(dir).entryInfoList(filters, QDir::Files);
    if (files.isEmpty())
        return;

    auto *watcher = new QFutureWatcher<QList<AppImageInfo>>(this);
    connect(watcher, &QFutureWatcher<QList<AppImageInfo>>::finished, this, [this, watcher]() {
        watcher->deleteLater();

        // Reset counters for this check cycle
        m_updateCount = 0;
        m_pendingChecks = 0;

        const QList<AppImageInfo> infos = watcher->result();
        for (const AppImageInfo &info : infos) {
            if (info.updateInfo.startsWith(QStringLiteral("gh-releases-zsync|")))
                ++m_pendingChecks;
        }

        if (m_pendingChecks == 0) {
            updateTrayStatus();
            return;
        }

        for (const AppImageInfo &info : infos) {
            if (!info.updateInfo.startsWith(QStringLiteral("gh-releases-zsync|")))
                continue;

            auto *checker = new GitHubReleaseChecker(m_networkManager, this);
            connect(checker, &GitHubReleaseChecker::updateAvailable, this,
                    [this, info, checker](const QString &remoteVer, const QString &/*zsyncUrl*/) {
                checker->deleteLater();

                ++m_updateCount;
                auto *notification = new KNotification(QStringLiteral("updateAvailable"),
                                                       KNotification::Persistent, this);
                notification->setTitle(i18n("Update Available"));
                notification->setText(i18n("An update is available for %1 (%2 → %3).",
                    info.cleanName.isEmpty() ? info.originalName : info.cleanName,
                    normalizeVersion(info.version), remoteVer));
                notification->setIconName(info.appId.isEmpty()
                    ? QStringLiteral("application-x-executable") : info.appId);
                auto *action = notification->addDefaultAction(i18n("Open Manager"));
                connect(action, &KNotificationAction::activated, this, []() {
                    QProcess::startDetached(QStringLiteral("appimagemanager"),
                                            {QStringLiteral("--dashboard")});
                });
                notification->sendEvent();

                if (--m_pendingChecks == 0)
                    updateTrayStatus();
            });

            auto handleFailure = [this, checker]() {
                checker->deleteLater();
                if (--m_pendingChecks == 0)
                    updateTrayStatus();
            };
            connect(checker, &GitHubReleaseChecker::upToDate, this, handleFailure);
            connect(checker, &GitHubReleaseChecker::networkFailed, this, handleFailure);
            connect(checker, &GitHubReleaseChecker::failed, this, handleFailure);

            checker->check(info.updateInfo, info.version);
        }
    });

    watcher->setFuture(QtConcurrent::run([files]() {
        QList<AppImageInfo> results;
        results.reserve(files.size());
        for (const QFileInfo &fi : files)
            results.append(AppImageReader(fi.absoluteFilePath()).read());
        return results;
    }));
}

void UpdateDaemon::updateTrayStatus()
{
#ifdef HAVE_KSTATUSNOTIFIERITEM
    if (m_updateCount > 0) {
        if (!m_trayIcon) {
            m_trayIcon = new KStatusNotifierItem(QStringLiteral("appimagemanager-updates"), this);
            m_trayIcon->setCategory(KStatusNotifierItem::ApplicationStatus);
            m_trayIcon->setIconByName(QStringLiteral("software-update-available"));
            connect(m_trayIcon, &KStatusNotifierItem::activateRequested,
                    this, [](bool, const QPoint &) {
                QProcess::startDetached(QStringLiteral("appimagemanager"),
                                        {QStringLiteral("--dashboard")});
            });
        }
        m_trayIcon->setStatus(KStatusNotifierItem::Active);
        m_trayIcon->setToolTip(
            QStringLiteral("software-update-available"),
            i18n("AppImage Updates"),
            i18np("%1 update available", "%1 updates available", m_updateCount));
    } else if (m_trayIcon) {
        m_trayIcon->setStatus(KStatusNotifierItem::Passive);
    }
#endif
}
