// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#pragma once

#include "appimagemanager_qml_export.h"

#include <QObject>

class AppImageBackend;
class AppImageIconProvider;
class AppImageListModel;
class AppImageSortFilterModel;
class QQmlApplicationEngine;
class QQuickWindow;

class APPIMAGEMANAGER_EXPORT DashboardWindow : public QObject
{
    Q_OBJECT
public:
    static void open();

    Q_INVOKABLE QObject *createUninstallBackend(const QString &filePath);
    Q_INVOKABLE QObject *createStorageBackend(const QString &filePath);
    Q_INVOKABLE void     openInFileManager(const QString &path);

private:
    explicit DashboardWindow(QObject *parent = nullptr);
    void setupAndShow();
    AppImageBackend *createBackend(const QString &filePath, bool withCorpses = false);

    AppImageIconProvider      *m_iconProvider       = nullptr;
    AppImageListModel         *m_listModel          = nullptr;
    AppImageSortFilterModel   *m_proxyModel         = nullptr;
    AppImageBackend       *m_uninstallBackend   = nullptr;
    AppImageBackend       *m_storageBackend     = nullptr;
    QQmlApplicationEngine *m_engine             = nullptr;
    QQuickWindow          *m_window             = nullptr;

    static DashboardWindow *s_instance;
};
