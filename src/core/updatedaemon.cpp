// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#include "updatedaemon.h"
#include "appimageinfo.h"
#include "logging.h"
#include "appsettings.h"
#include "appimagereader.h"
#include "githubreleasechecker.h"
#include "../gui/downloadwatcher.h"

#include <QDBusConnection>
#include <QDir>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QNetworkAccessManager>
#include <QMutex>
#include <QMutexLocker>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QAction>
#include <QMenu>
#include <QStandardPaths>
#include <KLocalizedString>
#include <KNotification>
#include <QProcess>
#include <QtConcurrent/QtConcurrent>

#include <KStatusNotifierItem>

// freq values: 0 = Never, 1 = Daily, 2 = Weekly, 3 = Monthly, 4 = Custom (customDays)
// start() skips m_timer->start() when freq == 0, so case 0 is never used at
// runtime; it exists so the switch has no UB on unexpected values.
static std::chrono::hours intervalForFrequency(int freq, int customDays)
{
    switch (freq) {
    case 0: return std::chrono::hours(0); // Never — timer not started by caller
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
    , m_downloadWatcher(new DownloadWatcher(this))
{
    m_readerPool.setMaxThreadCount(qBound(1, QThread::idealThreadCount() / 2, 4));
    connect(m_timer, &QTimer::timeout, this, &UpdateDaemon::checkUpdates);
    connect(AppSettings::instance(), &AppSettings::updateFrequencyChanged, this, [this]() {
        const int freq = AppSettings::instance()->updateFrequency();
        if (freq == 0) {
            m_timer->stop();
        } else {
            m_timer->setInterval(intervalForFrequency(freq, AppSettings::instance()->customUpdateDays()));
            m_timer->start();
        }
    });

    // Single source of truth for download detection. Mirror the user's
    // watchDownloads pref into the watcher; the dashboard's own watcher
    // skips notification when the daemon's D-Bus name is registered.
    m_downloadWatcher->setEnabled(AppSettings::instance()->watchDownloads());
    connect(AppSettings::instance(), &AppSettings::watchDownloadsChanged, this, [this]() {
        m_downloadWatcher->setEnabled(AppSettings::instance()->watchDownloads());
    });
    connect(m_downloadWatcher, &DownloadWatcher::appImageFound,
            this, &UpdateDaemon::onDownloadAppeared);
}

void UpdateDaemon::start()
{
    if (!QDBusConnection::sessionBus().registerService(QStringLiteral("io.github.appimagemanager.Daemon")))
        qCWarning(AIM_LOG) << "Failed to register D-Bus service io.github.appimagemanager.Daemon";

    // Persistent tray entry — Passive when idle so the user can still trigger
    // a manual check or open the dashboard at any time. Status flips to Active
    // when updateTrayStatus sees a pending update.
    m_trayIcon = new KStatusNotifierItem(QStringLiteral("appimagemanager-updates"), this);
    m_trayIcon->setCategory(KStatusNotifierItem::ApplicationStatus);
    m_trayIcon->setIconByName(QStringLiteral("application-x-executable"));
    m_trayIcon->setTitle(i18n("AppImage Manager"));
    m_trayIcon->setToolTip(QStringLiteral("application-x-executable"),
                           i18n("AppImage Manager"),
                           i18n("No pending updates"));
    m_trayIcon->setStatus(KStatusNotifierItem::Passive);

    connect(m_trayIcon, &KStatusNotifierItem::activateRequested,
            this, [](bool, const QPoint &) {
        if (!QProcess::startDetached(QStringLiteral("appimagemanager"),
                                     {QStringLiteral("--dashboard")}))
            qCWarning(AIM_LOG) << "Failed to launch appimagemanager --dashboard";
    });

    if (QMenu *menu = m_trayIcon->contextMenu()) {
        auto *checkAction = menu->addAction(QIcon::fromTheme(QStringLiteral("view-refresh")),
                                            i18n("Check for Updates Now"));
        connect(checkAction, &QAction::triggered, this, &UpdateDaemon::checkUpdates);

        auto *openAction = menu->addAction(QIcon::fromTheme(QStringLiteral("system-software-install")),
                                           i18n("Open Dashboard"));
        connect(openAction, &QAction::triggered, this, []() {
            if (!QProcess::startDetached(QStringLiteral("appimagemanager"),
                                         {QStringLiteral("--dashboard")}))
                qCWarning(AIM_LOG) << "Failed to launch appimagemanager --dashboard";
        });
    }

    const int freq = AppSettings::instance()->updateFrequency();
    m_timer->setInterval(intervalForFrequency(freq, AppSettings::instance()->customUpdateDays()));
    checkUpdates();
    if (freq != 0)
        m_timer->start();
}

void UpdateDaemon::onDownloadAppeared(const QString &path, const QString &displayName)
{
    auto *n = new KNotification(QStringLiteral("downloaded"), KNotification::Persistent, this);
    n->setTitle(i18n("%1 downloaded", displayName));
    n->setIconName(QStringLiteral("application-x-executable"));
    auto *action = n->addAction(i18n("Manage"));
    connect(action, &KNotificationAction::activated, this, [path]() {
        if (!QProcess::startDetached(QStringLiteral("appimagemanager"), { path }))
            qCWarning(AIM_LOG) << "Failed to launch appimagemanager for" << path;
    });
    n->sendEvent();
}

void UpdateDaemon::checkUpdates()
{
    if (AppSettings::instance()->updateFrequency() == 0)
        return;
    // Guard against re-entrancy: readers still in flight, or GitHub checker
    // chain still processing the previous batch. Dropping the duplicate call
    // is correct — the in-flight check will finish and updateTrayStatus().
    if (m_pendingChecks > 0 || !m_updateQueue.isEmpty())
        return;

    const QString dir = AppSettings::instance()->applicationsPath();
    const QStringList &filters = kAppImageFilters();
    const QFileInfoList files = QDir(dir).entryInfoList(filters, QDir::Files);
    if (files.isEmpty())
        return;

    auto results = QSharedPointer<QList<AppImageInfo>>::create();
    auto mutex = QSharedPointer<QMutex>::create();
    m_pendingChecks = files.size();

    for (const QFileInfo &fi : files) {
        const QString path = fi.absoluteFilePath();
        auto *watcher = new QFutureWatcher<AppImageInfo>(this);
        connect(watcher, &QFutureWatcher<AppImageInfo>::finished, this, [this, watcher, results, mutex]() {
            watcher->deleteLater();
            {
                QMutexLocker lock(mutex.data());
                results->append(watcher->result());
            }
            if (--m_pendingChecks == 0) {
                // All files read — build the GitHub checker queue.
                m_updateQueue.clear();
                m_updateCount = 0;

                for (const AppImageInfo &info : *results) {
                    if (info.updateInfo.startsWith(QStringLiteral("gh-releases-zsync|"))) {
                        m_updateQueue.enqueue(info);
                    }
                }

                checkNextUpdate();
            }
        });
        watcher->setFuture(QtConcurrent::run(&m_readerPool, [path]() {
            return AppImageReader(path).read();
        }));
    }
}

void UpdateDaemon::checkNextUpdate()
{
    if (m_updateQueue.isEmpty()) {
        updateTrayStatus();
        return;
    }

    const AppImageInfo info = m_updateQueue.dequeue();

    auto *checker = new GitHubReleaseChecker(m_networkManager, this);
    connect(checker, &GitHubReleaseChecker::checkFinished, this,
            [this, info, checker](GitHubReleaseChecker::Result result,
                                  const QString &remoteVer, const QString &/*zsyncUrl*/) {
        checker->deleteLater();

        if (result == GitHubReleaseChecker::Result::UpdateAvailable) {
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
                if (!QProcess::startDetached(QStringLiteral("appimagemanager"),
                                             {QStringLiteral("--dashboard")}))
                    qCWarning(AIM_LOG) << "Failed to launch appimagemanager --dashboard";
            });
            notification->sendEvent();
        }

        // Check next AppImage in queue sequentially
        checkNextUpdate();
    });

    checker->check(info.updateInfo, info.version);
}

void UpdateDaemon::updateTrayStatus()
{
    if (!m_trayIcon)
        return;  // start() not called yet

    if (m_updateCount > 0) {
        m_trayIcon->setIconByName(QStringLiteral("software-update-available"));
        m_trayIcon->setStatus(KStatusNotifierItem::Active);
        m_trayIcon->setToolTip(
            QStringLiteral("software-update-available"),
            i18n("AppImage Updates"),
            i18np("%1 update available", "%1 updates available", m_updateCount));
    } else {
        m_trayIcon->setIconByName(QStringLiteral("application-x-executable"));
        m_trayIcon->setStatus(KStatusNotifierItem::Passive);
        m_trayIcon->setToolTip(QStringLiteral("application-x-executable"),
                               i18n("AppImage Manager"),
                               i18n("No pending updates"));
    }
}
