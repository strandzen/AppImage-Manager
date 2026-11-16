// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#pragma once

#include "appimagemanager_qml_export.h"

#include <QObject>
#include <QUrl>

class AppImageBackend;
class AppImageIconProvider;
class AppImageListModel;
class AppImageSortFilterModel;
class AMStoreModel;
class QQmlApplicationEngine;
class QQuickWindow;

class APPIMAGEMANAGER_EXPORT DashboardWindow : public QObject
{
    Q_OBJECT
public:
    // page: "" (as it is) or "settings"; selectPath: select that AppImage on the Installed tab.
    static void open(const QString &page = {}, const QString &selectPath = {});

    Q_INVOKABLE QObject *createUninstallBackend(const QString &filePath);
    Q_INVOKABLE void     installFromPath(const QUrl &url);

private:
    explicit DashboardWindow(QObject *parent = nullptr);
    void setupAndShow();
    void applyRequest(const QString &page, const QString &selectPath);
    AppImageBackend *createBackend(const QString &filePath, bool withCorpses = false);

    AppImageIconProvider      *m_iconProvider       = nullptr;
    AppImageListModel         *m_listModel          = nullptr;
    AppImageSortFilterModel   *m_proxyModel         = nullptr;
    AMStoreModel              *m_amStoreModel       = nullptr;
    AppImageBackend       *m_uninstallBackend   = nullptr;
    QQmlApplicationEngine *m_engine             = nullptr;
    QQuickWindow          *m_window             = nullptr;

    static DashboardWindow *s_instance;
};
