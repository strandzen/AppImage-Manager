// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#pragma once
#include <QDBusContext>
#include <QDateTime>
#include <QObject>
#include <QPoint>
#include <QSet>
#include <QStringList>
#include <QThreadPool>
#include "appimageinfo.h"
#include "appimagemanager_qml_export.h"

class QDBusServiceWatcher;
class QNetworkAccessManager;
class QQmlApplicationEngine;
class QQuickWindow;
class QTimer;
class KDirWatch;
class KStatusNotifierItem;
class DownloadWatcher;
class AppImageUpdateManager;
class TrayModel;

// Background part of AppImage Manager (`appimagemanager --daemon`, started at login):
// scheduled update checks, notifications for new updates and for AppImages appearing in
// Downloads, and the tray. On Plasma the tray entry is our system tray widget
// (data/plasmoid), a view of State() over D-Bus that registers itself with RegisterView();
// elsewhere, or while no widget is registered, a KStatusNotifierItem with TrayWindow.qml.
// D-Bus: io.github.appimagemanager.Daemon, object /Daemon, interface ...Daemon1.
class APPIMAGEMANAGER_EXPORT UpdateDaemon : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "io.github.appimagemanager.Daemon1")
public:
    explicit UpdateDaemon(QObject *parent = nullptr);
    ~UpdateDaemon() override;
    void start();

    // Schedule (pure, tested). frequency: 0 Never, 1 Daily, 2 Weekly, 3 Monthly, 4 Custom.
    static int intervalDays(int frequency, int customDays);
    // When the next scheduled check is due; invalid for Never. Never checked = due now.
    static QDateTime nextCheck(const QDateTime &now, const QDateTime &lastCheck, int frequency, int customDays);
    static bool isDue(const QDateTime &now, const QDateTime &lastCheck, int frequency, int customDays);

    // Tray window (QML context property `daemon`).
    Q_INVOKABLE void checkNow();
    Q_INVOKABLE void updateAll();
    Q_INVOKABLE void openManager(const QString &selectPath = {});
    Q_INVOKABLE void openSettings();
    Q_INVOKABLE void hideTrayWindow();

public Q_SLOTS:
    // D-Bus: everything the tray widget shows, as JSON: updateCount, updatingCount,
    // checking, schedule, lastChecked, items[] (path, name, icon, version, state, newVersion, progress,
    // reason), updates first and apps without an update source last.
    Q_SCRIPTABLE QString State() const;
    Q_SCRIPTABLE void CheckNow() { checkNow(); }
    Q_SCRIPTABLE void UpdateAll() { updateAll(); }
    Q_SCRIPTABLE void OpenManager(const QString &selectPath) { openManager(selectPath); }
    Q_SCRIPTABLE void OpenSettings() { openSettings(); }
    // The tray widget is showing us: our own tray icon goes away until it unregisters
    // (or its process, plasmashell, leaves the bus).
    Q_SCRIPTABLE void RegisterView();
    // The main window wrote new check results (checkresults.h): show them.
    Q_SCRIPTABLE void ReloadState();
    Q_SCRIPTABLE void UnregisterView();

Q_SIGNALS:
    Q_SCRIPTABLE void UpdatesChanged();
    Q_SCRIPTABLE void StateChanged();

private Q_SLOTS:
    void tick();
    void reloadSettings();

private:
    void setupTrayIcon();
    void removeTrayIcon();
    void viewsChanged();
    QString iconFileFor(const QString &path, const QString &ext) const;
    void toggleTrayWindow(const QPoint &pos);
    bool createTrayWindow();
    void updateTray();
    void updateSchedule();

    void watchFolder();
    void scanFolder();
    void readItem(const QString &path);
    void applyMetadata(const QString &path, const AppImageInfo &info);
    void runCheck(const QStringList &paths, bool scheduled);
    void onCheckFinished(int updatesFound, int networkFailures);
    void onDownloadFinished(const QString &path, bool success);
    void notifyNewUpdates();
    void saveResults() const;
    bool isOnline() const;

    void onDownloadAppeared(const QString &path, const QString &displayName);

    QNetworkAccessManager *m_networkManager;
    DownloadWatcher       *m_downloadWatcher;
    AppImageUpdateManager *m_updates;
    TrayModel             *m_model;
    QTimer                *m_tick;
    QTimer                *m_readTimer;
    KDirWatch             *m_dirWatch;
    QString                m_watchedDir;
    QSet<QString>          m_pendingReads;   // changed files, read after a short settle
    QSet<QString>          m_recheck;        // files to check again (changed while an update was pending)
    QThreadPool            m_readerPool;

    QDateTime m_lastCheck;
    QDateTime m_retryAt;            // after a failed scheduled check, not before this
    bool      m_fullCheck = false;  // the running check covers everything (updates LastCheck)
    int       m_checkedCount = 0;   // items with an update source in the running check
    QStringList m_updatedNames;     // for the summary when Update All finishes

    QTimer                *m_stateTimer;     // coalesces StateChanged (progress is chatty)
    QDBusServiceWatcher   *m_viewWatcher;
    QSet<QString>          m_views;          // bus names of registered tray widgets
    bool                   m_started = false;

    KStatusNotifierItem   *m_trayIcon = nullptr;
    QQmlApplicationEngine *m_engine = nullptr;
    QQuickWindow          *m_trayWindow = nullptr;
};
