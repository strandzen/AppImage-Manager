// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#pragma once

#include "../core/appimageinfo.h"
#include "appimagemanager_qml_export.h"

#include <KDirWatch>

#include <QAbstractListModel>
#include <QDateTime>
#include <QSet>
#include <QtQml/qqmlregistration.h>

class AppImageIconProvider;
class AppImageUpdateManager;
class DownloadWatcher;
class QNetworkAccessManager;

class APPIMAGEMANAGER_EXPORT AppImageListModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Use the 'listModel' context property")

    Q_PROPERTY(bool scanning        READ isScanning       NOTIFY scanningChanged)
    Q_PROPERTY(bool checkingUpdates READ isCheckingUpdates NOTIFY checkingUpdatesChanged)
    Q_PROPERTY(bool selectionMode  READ selectionMode    WRITE setSelectionMode  NOTIFY selectionModeChanged)
    Q_PROPERTY(int  selectedCount  READ selectedCount    NOTIFY selectionChanged)

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
        UpdateAvailableRole,
        UpdateVersionRole,
        IsUpdatingRole,
        UpdateProgressRole,
        IsSelectedRole,
        CategoriesRole,
        CommentRole,
        DescriptionRole,
        DeveloperNameRole,
        HomepageRole,
    };
    Q_ENUM(Roles)

private:
    struct Item {
        QString   filePath;
        QDateTime addedDate;
        AppImageInfo info;
        QString cachedIconSource    = QStringLiteral("application-x-executable");
        QString cachedFormattedSize;
        QString cachedDisplayName;
        QString cachedDescription;
        QString updateVersion;
        QString zsyncUrl;
        bool updateAvailable = false;
        bool isUpdating      = false;
        int  updateProgress  = 0;
        bool metadataLoaded  = false;
        bool hasDesktopLink  = false;
    };

public:
    explicit AppImageListModel(AppImageIconProvider *iconProvider, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    bool isScanning()        const { return m_scanning; }
    bool isCheckingUpdates() const;
    bool selectionMode()     const { return m_selectionMode; }
    int  selectedCount()     const { return static_cast<int>(m_selected.size()); }

    void setSelectionMode(bool mode);

    // Direct typed accessors used by AppImageSortFilterModel::lessThan to skip
    // the QVariant/role dispatch in data(). `field` matches SortField in
    // AppImageSortFilterModel (0 Name, 1 Size, 5 Category, 9 Date).
    QVariant sortKey(int row, int field) const;
    QString  displayNameForRow(int row)  const;

    // Bulk role fetch for QML: returns every role keyed by its roleName
    // string in a single trip. Avoids QML's per-role .data() round-trips
    // when populating the dashboard detail pane.
    Q_INVOKABLE QVariantMap itemData(int row) const;

    Q_INVOKABLE void scan();
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void toggleDesktopLink(int row, bool enable);
    Q_INVOKABLE void requestRemoveAt(int row);
    Q_INVOKABLE void requestLaunch(int row);
    Q_INVOKABLE void checkForUpdates();
    Q_INVOKABLE void downloadUpdate(int row);

    Q_INVOKABLE void setSelected(const QString &path, bool selected);
    Q_INVOKABLE void selectAll();
    Q_INVOKABLE void clearSelection();
    Q_INVOKABLE QStringList selectedPaths() const;
    Q_INVOKABLE void trashSelected();

Q_SIGNALS:
    void scanningChanged();
    void checkingUpdatesChanged();
    void updateCheckFinished(int updatesFound, int networkFailures);
    void selectionModeChanged();
    void selectionChanged();
    void openUninstallWindow(const QString &filePath);

private:
    void loadMetadataForRow(int row);
    void applyMetadata(int row, AppImageInfo info);
    int  findRowByPath(const QString &path) const;

    // Incremental KDirWatch handlers — feed inserts/removes/dirty events
    // straight into the model without re-scanning the whole directory.
    void onFileCreated(const QString &path);
    void onFileDeleted(const QString &path);
    void onFileDirty(const QString &path);
    static void sendError(QObject *parent, const QString &title, const QString &text);
    static QString iconIdForPath(const QString &path);
    static QString formatBytes(qint64 bytes);
    static QString computeDisplayName(const Item &item);
    static QString computeIconSource(const Item &item);

    AppImageIconProvider  *m_iconProvider;
    AppImageUpdateManager *m_updateManager;
    DownloadWatcher       *m_downloadWatcher;
    QList<Item>            m_items;
    bool                   m_scanning      = false;
    int                    m_pendingLoads  = 0;
    int                    m_generation   = 0;
    bool                   m_selectionMode = false;
    QSet<QString>          m_selected;

    KDirWatch              m_watcher;
    QString                m_watchedDir;   // current applicationsPath registered with m_watcher
};
