// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#pragma once

#include "../core/appimageinfo.h"

#include <QAbstractListModel>
#include <QDateTime>
#include <QFileSystemWatcher>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

class AppImageIconProvider;
class AppImageManager;

class AppImageListModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Use the 'listModel' context property")

    Q_PROPERTY(bool scanning READ isScanning NOTIFY scanningChanged)

public:
    enum Roles {
        FilePathRole = Qt::UserRole,
        CleanNameRole,
        AppNameRole,
        VersionRole,
        IconSourceRole,
        HasDesktopLinkRole,
        MetadataLoadedRole,
        AppSizeRole,
        FormattedSizeRole,
        AddedDateRole,
        DisplayNameRole,
    };
    Q_ENUM(Roles)

private:
    struct Item {
        QString   filePath;
        QDateTime addedDate;
        AppImageInfo info;
        bool metadataLoaded = false;
        bool hasDesktopLink = false;
    };

public:
    explicit AppImageListModel(AppImageIconProvider *iconProvider, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    bool isScanning() const { return m_scanning; }

    Q_INVOKABLE void scan();
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void toggleDesktopLink(int row, bool enable);
    Q_INVOKABLE void requestRemoveAt(int row);
    Q_INVOKABLE void requestLaunch(int row);

Q_SIGNALS:
    void scanningChanged();
    void openUninstallWindow(const QString &filePath);

private:
    void loadMetadataForRow(int row);
    static QString iconIdForPath(const QString &path);

    AppImageIconProvider  *m_iconProvider;
    AppImageManager       *m_manager;
    QList<Item>            m_items;
    bool                   m_scanning    = false;
    int                    m_pendingLoads = 0;
    int                    m_generation  = 0;

    QFileSystemWatcher     m_watcher;
    QTimer                 m_refreshTimer;
};
