// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#pragma once
#include <QObject>
#include <QTimer>
#include "appimagemanager_qml_export.h"

class QNetworkAccessManager;

class APPIMAGEMANAGER_EXPORT UpdateDaemon : public QObject
{
    Q_OBJECT
public:
    explicit UpdateDaemon(QObject *parent = nullptr);
    void start();

private:
    void checkUpdates();
    
    QTimer *m_timer;
    QNetworkAccessManager *m_networkManager;
};
