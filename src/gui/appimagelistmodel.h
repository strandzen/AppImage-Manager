// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#pragma once

#include "../core/checkresults.h"
#include "../core/appimageinfo.h"
#include "appimagemanager_qml_export.h"

#include <KDirWatch>

#include <QAbstractListModel>
#include <QDateTime>
#include <QHash>
#include <QThreadPool>
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
    // Progress of the running (or last) update check: items finished / items with a source.
    Q_PROPERTY(int  checkDone       READ checkDone        NOTIFY checkProgressChanged)
    Q_PROPERTY(int  checkTotal      READ checkTotal       NOTIFY checkProgressChanged)
    Q_PROPERTY(bool downloadWatcherSandboxed READ isDownloadWatcherSandboxed CONSTANT)
    // Number of rows with an update available (drives the "Installed (N)" tab and Update All).
    Q_PROPERTY(int  updateCount     READ updateCount      NOTIFY updateCountChanged)
    // The overview: rows per update state, total size, when updates were last checked.
    Q_PROPERTY(int  upToDateCount   READ upToDateCount    NOTIFY stateSummaryChanged)
    Q_PROPERTY(int  failedCount     READ failedCount      NOTIFY stateSummaryChanged)
    Q_PROPERTY(int  notCheckedCount READ notCheckedCount  NOTIFY stateSummaryChanged)
    Q_PROPERTY(int  noSourceCount   READ noSourceCount    NOTIFY stateSummaryChanged)
    Q_PROPERTY(QString totalSizeText READ totalSizeText   NOTIFY stateSummaryChanged)
    // "14:02", "yesterday 14:02", …; empty when never checked (here or by the daemon).
    Q_PROPERTY(QString lastCheckTime READ lastCheckTime   NOTIFY stateSummaryChanged)
    // What the listSection role groups by: 0 nothing, 1 category, 2 date added, 3 update state.
    // Rows with an update come first under "Updates available" in modes 0-2.
    Q_PROPERTY(int  sectionMode     READ sectionMode WRITE setSectionMode NOTIFY sectionModeChanged)

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
        CategoriesRole,
        CommentRole,
        DescriptionRole,
        DeveloperNameRole,
        HomepageRole,
        SectionCategoryRole,  // readable main category, or "Other" — list section header when sorted by category
        SectionAddedRole,     // "Today" / "This Week" / "This Month" / "Earlier" — when sorted by date
        UpdateStateRole,      // UpdateState (checkresults.h) as int
        UpdateReasonRole,     // why the last check failed
        OlderCopyOfRole,      // name of a newer copy of the same app (same appId), or empty
        ListSectionRole,      // the section header for the current sectionMode
        CategoryLabelRole,    // readable main category ("Graphics", "Internet")
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
        QString updateUrl;  // .zsync control file or direct .AppImage
        int  updateState     = 0;  // UpdateState; UpdateAvailable keeps updateVersion/updateUrl
        QString updateReason;
        QString olderCopyOf;
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

    bool isScanning()                  const { return m_scanning; }
    bool isCheckingUpdates()           const;
    int  checkDone()                   const { return m_checkDone; }
    int  checkTotal()                  const { return m_checkTotal; }
    bool isDownloadWatcherSandboxed()  const;
    int  updateCount() const { return m_updateCount; }
    int  upToDateCount()   const { return m_upToDateCount; }
    int  failedCount()     const { return m_failedCount; }
    int  notCheckedCount() const { return m_notCheckedCount; }
    int  noSourceCount()   const { return m_noSourceCount; }
    QString totalSizeText() const;
    QString lastCheckTime() const;
    int  sectionMode() const { return m_sectionMode; }
    void setSectionMode(int mode);
    // For AppImageSortFilterModel: 0 update (or updating), 1 check failed, 2 up to date,
    // 3 not checked, 4 no update source.
    int  statusRank(int row) const;
    bool hasPendingUpdate(int row) const;
    int  updateStateForRow(int row) const;

    // Direct typed accessors used by AppImageSortFilterModel::lessThan to skip
    // the QVariant/role dispatch in data(). `field` matches SortField in
    // AppImageSortFilterModel (0 Name, 1 Size, 5 Category, 9 Date).
    QVariant sortKey(int row, int field) const;
    QString  displayNameForRow(int row)  const;

    // Bulk role fetch for QML: returns every role keyed by its roleName
    // string in a single trip. Avoids QML's per-role .data() round-trips
    // when populating the dashboard detail pane.
    Q_INVOKABLE QVariantMap itemData(int row) const;
    // Row of the AppImage at `path`, or -1.
    int rowForPath(const QString &path) const { return findRowByPath(path); }

    Q_INVOKABLE void scan();
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void toggleDesktopLink(int row, bool enable);
    Q_INVOKABLE void requestRemoveAt(int row);
    Q_INVOKABLE void requestLaunch(int row);
    Q_INVOKABLE void checkForUpdates();
    Q_INVOKABLE void downloadUpdate(int row);
    // Per-AppImage update source (see core/updatesource.h).
    // Returns { input, type, prerelease, embedded, embeddedLabel }: input as the user would type
    // it, type "" (built-in) / "github" / "forgejo" / "zsync" / "url", embedded the AppImage's own
    // update info and embeddedLabel its description.
    Q_INVOKABLE QVariantMap updateSource(int row) const;
    // input: anything parseUpdateSourceInput() accepts, or empty to clear; type "" auto-detects.
    // Returns false when the input is unusable. On success the row's update state resets (run
    // checkForUpdates to re-check).
    Q_INVOKABLE bool setUpdateSource(int row, const QString &input, const QString &type, bool prerelease);
    // Live feedback for the dialog: { valid, type, label } ("Codeberg · owner/repo").
    Q_INVOKABLE QVariantMap describeUpdateSourceInput(const QString &input, const QString &type) const;
    // Launch environment (see core/launchenvironment.h) as "KEY=value" lines.
    Q_INVOKABLE QString environment(int row) const;
    // Saves the lines and rewrites the app menu entry. Returns the invalid 1-based line numbers
    // (nothing saved) or an empty list on success.
    Q_INVOKABLE QVariantList setEnvironment(int row, const QString &text);

private Q_SLOTS:
    // Re-reads the shared check results (checkresults.h); the daemon wrote new ones.
    void reloadStoredResults();

Q_SIGNALS:
    void scanningChanged();
    void checkingUpdatesChanged();
    void updateCountChanged();
    void stateSummaryChanged();
    void sectionModeChanged();
    void updateCheckFinished(int updatesFound, int networkFailures);
    void checkProgressChanged();
    void openUninstallWindow(const QString &filePath);

private:
    void loadMetadataForRow(int row);
    void applyMetadata(int row, const AppImageInfo &info);
    int  findRowByPath(const QString &path) const;
    void recountUpdates();
    void rebuildPathIndex();

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
    static QString categoryLabel(const Item &item);
    QString listSection(const Item &item) const;
    void applyStoredResult(Item &item) const;
    void setRowState(int row, int state, const QString &reason = {});
    void storeResults();
    void notifyDaemon();
    void updateOlderCopies();
    static bool hasUpdate(const Item &item);
    static QString addedGroup(const QDateTime &added);

    AppImageIconProvider  *m_iconProvider;
    AppImageUpdateManager *m_updateManager;
    DownloadWatcher       *m_downloadWatcher;
    QList<Item>            m_items;
    bool                   m_scanning      = false;
    int                    m_updateCount   = 0;
    int                    m_pendingLoads  = 0;
    int                    m_generation   = 0;
    QHash<QString, CheckResult> m_results; // shared check results by file name
    QDateTime              m_lastCheck;
    int                    m_upToDateCount   = 0;
    int                    m_failedCount     = 0;
    int                    m_notCheckedCount = 0;
    int                    m_noSourceCount   = 0;
    qint64                 m_totalSize       = 0;
    int                    m_sectionMode     = 0;
    int                    m_checkDone  = 0;
    int                    m_checkTotal = 0;
    QHash<QString, int>    m_pathIndex;  // path → row; kept in sync for O(1) findRowByPath

    QThreadPool            m_readerPool;
    KDirWatch              m_watcher;
    QString                m_watchedDir;   // current applicationsPath registered with m_watcher
};
