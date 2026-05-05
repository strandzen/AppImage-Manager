// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#pragma once
#include <QObject>
#include <QSet>
#include <QTimer>
#include "appimagemanager_qml_export.h"

class QFileSystemWatcher;
class QNetworkAccessManager;

class APPIMAGEMANAGER_EXPORT UpdateDaemon : public QObject
{
    Q_OBJECT
public:
    explicit UpdateDaemon(QObject *parent = nullptr);
    void start();

private:
    void checkUpdates();
    void watchDownloads();
    void onDownloadDirChanged();

    QTimer               *m_timer;
    QNetworkAccessManager *m_networkManager;
    QFileSystemWatcher   *m_downloadWatcher = nullptr;
    QSet<QString>         m_knownDownloads;
};
