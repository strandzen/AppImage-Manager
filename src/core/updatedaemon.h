// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#pragma once
#include <QObject>
#include <QTimer>
#include "appimagemanager_qml_export.h"

class QNetworkAccessManager;
class KStatusNotifierItem;
class DownloadWatcher;

class APPIMAGEMANAGER_EXPORT UpdateDaemon : public QObject
{
    Q_OBJECT
public:
    explicit UpdateDaemon(QObject *parent = nullptr);
    void start();

private:
    void checkUpdates();
    void onDownloadAppeared(const QString &path, const QString &displayName);
    void updateTrayStatus();

    QTimer               *m_timer;
    QNetworkAccessManager *m_networkManager;
    DownloadWatcher      *m_downloadWatcher;

    int m_updateCount   = 0;
    int m_pendingChecks = 0;

    KStatusNotifierItem *m_trayIcon = nullptr;
};
