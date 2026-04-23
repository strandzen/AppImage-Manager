// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#pragma once

#include "appimagemanager_qml_export.h"

#include <QHash>
#include <QObject>
#include <QString>

class AppImageBackend;
class AppImageIconProvider;
class QQmlApplicationEngine;
class QQuickWindow;

// Manages the lifecycle of one QQuickWindow per AppImage path.
// Open the same path twice → the existing window is raised instead.
class APPIMAGEMANAGER_EXPORT AppImageWindow : public QObject
{
    Q_OBJECT
public:
    static void open(const QString &appImagePath);

private:
    static QHash<QString, AppImageWindow *> s_instances;

    explicit AppImageWindow(const QString &path, QObject *parent = nullptr);

    void setupAndShow();

    QString m_path;
    QQmlApplicationEngine *m_engine = nullptr;
    QQuickWindow *m_window = nullptr;
    AppImageBackend *m_backend = nullptr;
    AppImageIconProvider *m_iconProvider = nullptr;
};
