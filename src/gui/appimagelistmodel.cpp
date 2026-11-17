// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#include "appimagelistmodel.h"
#include "appimageiconprovider.h"
#include "appimageupdate.h"
#include "downloadwatcher.h"
#include "../core/appimagecache.h"
#include "../core/appimagemanager.h"
#include "../core/appimagereader.h"
#include "../core/appsettings.h"
#include "../core/updatesource.h"
#include "../core/launchenvironment.h"
#include "../core/previousversion.h"
#include "../core/sandbox.h"
#include "../core/releasechecker.h"
#include "../core/releasenotes.h"

#include <KFormat>
#include <KIO/CopyJob>
#include <KLocalizedString>
#include <KNotification>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDir>
#include <QGuiApplication>
#include <algorithm>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QIcon>
#include <QNetworkAccessManager>
#include <QLocale>
#include <QNetworkReply>
#include <QProcess>
#include <QStandardPaths>
#include <QThread>
#include <QtConcurrent/QtConcurrent>

AppImageListModel::AppImageListModel(AppImageIconProvider *iconProvider, QObject *parent)
    : QAbstractListModel(parent)
    , m_iconProvider(iconProvider)
    , m_network(new QNetworkAccessManager(this))
    , m_updateManager(new AppImageUpdateManager(m_network, this))
    , m_downloadWatcher(new DownloadWatcher(this))
{
    m_readerPool.setMaxThreadCount(qBound(1, QThread::idealThreadCount() / 2, 4));
    // KDirWatch in WatchFiles mode emits per-file created/deleted/dirty events
    // (de-duplicated internally) so the model can do incremental inserts and
    // removes instead of debouncing + re-scanning the whole directory.
    connect(&m_watcher, &KDirWatch::created, this, &AppImageListModel::onFileCreated);
    connect(&m_watcher, &KDirWatch::deleted, this, &AppImageListModel::onFileDeleted);
    connect(&m_watcher, &KDirWatch::dirty,   this, &AppImageListModel::onFileDirty);

    connect(AppSettings::instance(), &AppSettings::applicationsPathChanged,
            this, [this]() {
                // Re-bind the watcher to the new directory and replay every file.
                if (!m_watchedDir.isEmpty()) {
                    m_watcher.removeDir(m_watchedDir);
                    m_watchedDir.clear();
                }
                refresh();
            });

    // Sandbox all: menu entries of sandboxed apps start through appimagemanager --run.
    connect(AppSettings::instance(), &AppSettings::sandboxAllChanged, this, [this]() {
        for (const Item &item : std::as_const(m_items)) {
            if (item.hasDesktopLink)
                AppImageManager::createDesktopLink(item.filePath, item.info);
        }
        if (!m_items.isEmpty())
            Q_EMIT dataChanged(index(0, 0), index(m_items.size() - 1, 0));
    });

    // ── Download watcher ──────────────────────────────────────────────────────
    m_downloadWatcher->setEnabled(AppSettings::instance()->watchDownloads());
    connect(AppSettings::instance(), &AppSettings::watchDownloadsChanged,
            this, [this]() {
                m_downloadWatcher->setEnabled(AppSettings::instance()->watchDownloads());
            });

    connect(m_downloadWatcher, &DownloadWatcher::appImageFound,
            this, [this](const QString &path, const QString &displayName) {
                // Daemon handles download notifications when running — avoid duplicates.
                const auto *iface = QDBusConnection::sessionBus().interface();
                if (iface && iface->isServiceRegistered(
                        QStringLiteral("io.github.appimagemanager.Daemon")))
                    return;

                // HIG: don't notify while the app's main window is in the foreground.
                if (QGuiApplication::applicationState() == Qt::ApplicationActive)
                    return;

                auto *n = new KNotification(QStringLiteral("downloaded"),
                                            KNotification::Persistent, this);
                n->setTitle(i18n("%1 downloaded", displayName));
                n->setIconName(QStringLiteral("application-x-executable"));
                auto *action = n->addAction(i18n("Manage"));
                connect(action, &KNotificationAction::activated, this, [path]() {
                    QProcess::startDetached(QStringLiteral("appimagemanager"), { path });
                });
                n->sendEvent();
            });

    // Keep updateCount in step with every change that can add, drop or clear an update.
    connect(this, &QAbstractItemModel::dataChanged, this, &AppImageListModel::recountUpdates);
    connect(this, &QAbstractItemModel::rowsRemoved, this, &AppImageListModel::recountUpdates);
    connect(this, &QAbstractItemModel::modelReset,  this, &AppImageListModel::recountUpdates);

    // ── Update manager signals ────────────────────────────────────────────────
    // Check results are shared with the background daemon through the state config; it
    // announces new ones with UpdatesChanged.
    m_results = loadCheckResults();
    m_lastCheck = loadLastCheck();
    QDBusConnection::sessionBus().connect(QStringLiteral("io.github.appimagemanager.Daemon"),
                                          QStringLiteral("/Daemon"),
                                          QStringLiteral("io.github.appimagemanager.Daemon1"),
                                          QStringLiteral("UpdatesChanged"),
                                          this, SLOT(reloadStoredResults()));

    connect(m_updateManager, &AppImageUpdateManager::checkingChanged,
            this, &AppImageListModel::checkingUpdatesChanged);

    connect(m_updateManager, &AppImageUpdateManager::checkProgress, this, [this](int done, int total) {
        m_checkDone = done;
        m_checkTotal = total;
        Q_EMIT checkProgressChanged();
    });
    connect(m_updateManager, &AppImageUpdateManager::checkFinished, this, [this](int found, int failures) {
        // Share the results (the tray shows them too). A check that reached nothing does not
        // count as a check.
        if (found >= 0) {
            storeResults();
            if (!(m_checkTotal > 0 && failures >= m_checkTotal)) {
                m_lastCheck = QDateTime::currentDateTimeUtc();
                saveLastCheck(m_lastCheck);
            }
            notifyDaemon();
            Q_EMIT stateSummaryChanged();
        }
        Q_EMIT updateCheckFinished(found, failures);
    });

    connect(m_updateManager, &AppImageUpdateManager::updateFound,
            this, [this](const QString &filePath, const QString &version, const QString &url) {
                const int row = findRowByPath(filePath);
                if (row < 0) return;
                Item &item = m_items[row];
                item.updateVersion   = version;
                item.updateUrl       = url;
                // itemChecked follows and sets the state.
            });
    connect(m_updateManager, &AppImageUpdateManager::itemChecked, this,
            [this](const QString &filePath, AppImageUpdateManager::CheckResult result, const QString &reason) {
                const int row = findRowByPath(filePath);
                if (row < 0 || m_items.at(row).isUpdating) return;
                setRowState(row, result == AppImageUpdateManager::CheckResult::UpdateAvailable
                                     ? int(UpdateState::UpdateAvailable)
                                 : result == AppImageUpdateManager::CheckResult::UpToDate
                                     ? int(UpdateState::UpToDate) : int(UpdateState::Failed),
                            reason);
            });

    connect(m_updateManager, &AppImageUpdateManager::downloadProgress,
            this, [this](const QString &filePath, int percent) {
                const int row = findRowByPath(filePath);
                if (row < 0) return;
                m_items[row].updateProgress = percent;
                Q_EMIT dataChanged(index(row, 0), index(row, 0), {UpdateProgressRole});
            });

    connect(m_updateManager, &AppImageUpdateManager::downloadFinished,
            this, [this](const QString &filePath, bool success) {
                const int row = findRowByPath(filePath);
                if (row < 0) return;
                Item &item = m_items[row];
                item.isUpdating = false;
                if (success) {
                    // Remember the release (or direct-download build) now installed, so
                    // AppImages without a usable version string stop showing the update.
                    if (isReleaseUpdateInfo(effectiveUpdateInfo(item.info.updateInfo, loadUpdateSource(filePath))))
                        saveInstalledTag(filePath, item.updateVersion);
                    // Up to date at the version just installed (matched when the file is re-read).
                    const QString installed = normalizeVersion(item.updateVersion);
                    item.updateState     = int(UpdateState::UpToDate);
                    item.updateReason.clear();
                    item.updateProgress  = 0;
                    const CheckResult result{ UpdateState::UpToDate, {}, {}, installed, {} };
                    m_results.insert(item.info.originalName, result);
                    saveCheckResult(item.info.originalName, result);
                    notifyDaemon();
                    ++m_pendingLoads; // loadMetadataForRow decrements it
                    loadMetadataForRow(row);
                }
                Q_EMIT dataChanged(index(row, 0), index(row, 0),
                                   {IsUpdatingRole, UpdateAvailableRole, UpdateStateRole, UpdateProgressRole,
                                    ListSectionRole});
                if (!success)
                    sendError(this, i18n("Update Failed"),
                              i18n("%1 could not be updated.", item.cachedDisplayName));
            });
}

void AppImageListModel::recountUpdates()
{
    int updates = 0, upToDate = 0, failed = 0, notChecked = 0, noSource = 0, pinned = 0;
    qint64 size = 0;
    for (const Item &item : std::as_const(m_items)) {
        size += item.info.fileSize;
        switch (static_cast<UpdateState>(item.updateState)) {
        case UpdateState::UpdateAvailable: ++updates; break;
        case UpdateState::UpToDate:        ++upToDate; break;
        case UpdateState::Failed:          ++failed; break;
        case UpdateState::NoSource:        ++noSource; break;
        case UpdateState::Pinned:          ++pinned; break;
        default:                           ++notChecked; break;
        }
    }
    if (upToDate != m_upToDateCount || failed != m_failedCount || notChecked != m_notCheckedCount
        || noSource != m_noSourceCount || pinned != m_pinnedCount || size != m_totalSize) {
        m_upToDateCount = upToDate;
        m_failedCount = failed;
        m_notCheckedCount = notChecked;
        m_noSourceCount = noSource;
        m_pinnedCount = pinned;
        m_totalSize = size;
        Q_EMIT stateSummaryChanged();
    }
    if (updates == m_updateCount)
        return;
    const bool sectionsChange = (updates == 0) != (m_updateCount == 0);
    m_updateCount = updates;
    Q_EMIT updateCountChanged();
    // The other rows get (or lose) their "Installed" heading below "Updates available".
    if (sectionsChange && !m_items.isEmpty())
        Q_EMIT dataChanged(index(0, 0), index(m_items.size() - 1, 0), {ListSectionRole});
}

// static
bool AppImageListModel::hasUpdate(const Item &item)
{
    return item.updateState == int(UpdateState::UpdateAvailable);
}

QString AppImageListModel::totalSizeText() const
{
    return formatBytes(m_totalSize);
}

QString AppImageListModel::lastCheckTime() const
{
    return formatCheckTime(m_lastCheck);
}

void AppImageListModel::setSectionMode(int mode)
{
    if (mode == m_sectionMode)
        return;
    m_sectionMode = mode;
    Q_EMIT sectionModeChanged();
    if (!m_items.isEmpty())
        Q_EMIT dataChanged(index(0, 0), index(m_items.size() - 1, 0), {ListSectionRole});
}

int AppImageListModel::statusRank(int row) const
{
    if (row < 0 || row >= m_items.size())
        return 5;
    const Item &item = m_items.at(row);
    if (hasUpdate(item) || item.isUpdating)
        return 0;
    switch (static_cast<UpdateState>(item.updateState)) {
    case UpdateState::Failed:     return 1;
    case UpdateState::UpToDate:   return 2;
    case UpdateState::NoSource:   return 4;
    case UpdateState::Pinned:     return 5;
    default:                      return 3;
    }
}

bool AppImageListModel::hasPendingUpdate(int row) const
{
    return row >= 0 && row < m_items.size() && (hasUpdate(m_items.at(row)) || m_items.at(row).isUpdating);
}

int AppImageListModel::updateStateForRow(int row) const
{
    return row >= 0 && row < m_items.size() ? m_items.at(row).updateState : 0;
}

// The stored result for this file and version, or what follows from its update source.
void AppImageListModel::applyStoredResult(Item &item) const
{
    if (!item.metadataLoaded || item.isUpdating)
        return;
    item.updateReason.clear();
    if (isPinned(item.filePath)) {
        item.updateState = int(UpdateState::Pinned);
        return;
    }
    if (!isSupportedUpdateInfo(effectiveUpdateInfo(item.info.updateInfo, loadUpdateSource(item.filePath)))) {
        item.updateState = int(UpdateState::NoSource);
        return;
    }
    const auto it = m_results.constFind(item.info.originalName);
    if (it != m_results.cend() && it->versionAtCheck == normalizeVersion(item.info.version)) {
        item.updateState = int(it->state);
        item.updateVersion = it->newVersion;
        item.updateUrl = it->url;
        item.updateReason = it->reason;
    } else {
        item.updateState = int(UpdateState::NotChecked);
    }
}

void AppImageListModel::setRowState(int row, int state, const QString &reason)
{
    Item &item = m_items[row];
    item.updateState = state;
    item.updateReason = state == int(UpdateState::Failed) ? reason : QString();
    Q_EMIT dataChanged(index(row, 0), index(row, 0),
                       {UpdateStateRole, UpdateReasonRole, UpdateAvailableRole, UpdateVersionRole, ListSectionRole});
}

void AppImageListModel::storeResults()
{
    QHash<QString, CheckResult> results;
    for (const Item &item : std::as_const(m_items)) {
        if (!item.metadataLoaded)
            continue;
        results.insert(item.info.originalName,
                       { static_cast<UpdateState>(item.updateState), item.updateVersion, item.updateUrl,
                         normalizeVersion(item.info.version), item.updateReason });
    }
    saveCheckResults(results);
    m_results = loadCheckResults(); // only the stored states
}

// Tells the background daemon (if running) to re-read the shared results.
void AppImageListModel::notifyDaemon()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected() || !bus.interface()
        || !bus.interface()->isServiceRegistered(QStringLiteral("io.github.appimagemanager.Daemon")))
        return;
    bus.asyncCall(QDBusMessage::createMethodCall(QStringLiteral("io.github.appimagemanager.Daemon"),
                                                 QStringLiteral("/Daemon"),
                                                 QStringLiteral("io.github.appimagemanager.Daemon1"),
                                                 QStringLiteral("ReloadState")));
}

void AppImageListModel::reloadStoredResults()
{
    m_results = loadCheckResults();
    m_lastCheck = loadLastCheck();
    for (Item &item : m_items)
        applyStoredResult(item);
    if (!m_items.isEmpty())
        Q_EMIT dataChanged(index(0, 0), index(m_items.size() - 1, 0),
                           {UpdateStateRole, UpdateReasonRole, UpdateAvailableRole, UpdateVersionRole, ListSectionRole});
    Q_EMIT stateSummaryChanged();
}

// Several AppImages of the same app (same appId): all but the newest version are marked.
void AppImageListModel::updateOlderCopies()
{
    QHash<QString, QList<int>> byApp;
    for (int row = 0; row < m_items.size(); ++row) {
        const Item &item = m_items.at(row);
        if (item.metadataLoaded && !item.info.appId.isEmpty())
            byApp[item.info.appId].append(row);
    }
    QHash<int, QString> olderOf;
    for (const QList<int> &rows : std::as_const(byApp)) {
        if (rows.size() < 2)
            continue;
        int newest = rows.first();
        for (int row : rows) {
            const Item &a = m_items.at(row);
            const Item &b = m_items.at(newest);
            if (isNewerVersion(normalizeVersion(a.info.version), normalizeVersion(b.info.version))
                || (a.info.version == b.info.version && a.addedDate > b.addedDate))
                newest = row;
        }
        for (int row : rows) {
            if (row != newest)
                olderOf.insert(row, m_items.at(newest).cachedDisplayName);
        }
    }
    for (int row = 0; row < m_items.size(); ++row) {
        const QString older = olderOf.value(row);
        if (m_items.at(row).olderCopyOf != older) {
            m_items[row].olderCopyOf = older;
            Q_EMIT dataChanged(index(row, 0), index(row, 0), {OlderCopyOfRole});
        }
    }
}

// ── QAbstractListModel interface ──────────────────────────────────────────────

int AppImageListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return m_items.size();
}

QVariant AppImageListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_items.size())
        return {};

    const Item &item = m_items.at(index.row());

    switch (role) {
    case FilePathRole:        return item.filePath;
    case CleanNameRole:       return item.info.cleanName;
    case AppNameRole:         return item.info.appName;
    case VersionRole:         return item.info.version;
    case HasDesktopLinkRole:  return item.hasDesktopLink;
    case MetadataLoadedRole:  return item.metadataLoaded;
    case AppSizeRole:         return item.info.fileSize;
    case FormattedSizeRole:   return item.cachedFormattedSize;
    case AddedDateRole:       return item.addedDate;
    case DisplayNameRole:     return item.cachedDisplayName;
    case IconSourceRole:      return item.cachedIconSource;
    case UpdateAvailableRole: return hasUpdate(item);
    case UpdateVersionRole:   return item.updateVersion;
    case IsUpdatingRole:      return item.isUpdating;
    case UpdateProgressRole:  return item.updateProgress;
    case CategoriesRole:      return item.info.categories;
    case CommentRole:         return item.info.comment;
    case DescriptionRole:     return item.cachedDescription;
    case DeveloperNameRole:   return item.info.developerName;
    case HomepageRole:        return item.info.homepage;
    case SectionCategoryRole: return categoryLabel(item);
    case SectionAddedRole:    return addedGroup(item.addedDate);
    case UpdateStateRole:     return item.updateState;
    case UpdateReasonRole:    return item.updateReason;
    case OlderCopyOfRole:     return item.olderCopyOf;
    case ListSectionRole:     return listSection(item);
    case CategoryLabelRole:   return categoryLabel(item);
    default: return {};
    }
}

QHash<int, QByteArray> AppImageListModel::roleNames() const
{
    return {
        { FilePathRole,        "filePath"        },
        { CleanNameRole,       "cleanName"       },
        { AppNameRole,         "appName"         },
        { VersionRole,         "version"         },
        { IconSourceRole,      "iconSource"      },
        { HasDesktopLinkRole,  "hasDesktopLink"  },
        { MetadataLoadedRole,  "metadataLoaded"  },
        { AppSizeRole,         "appSize"         },
        { FormattedSizeRole,   "formattedSize"   },
        { AddedDateRole,       "addedDate"       },
        { DisplayNameRole,     "displayName"     },
        { UpdateAvailableRole, "updateAvailable" },
        { UpdateVersionRole,   "updateVersion"   },
        { IsUpdatingRole,      "isUpdating"      },
        { UpdateProgressRole,  "updateProgress"  },
        { CategoriesRole,      "categories"      },
        { CommentRole,         "comment"         },
        { DescriptionRole,     "description"     },
        { DeveloperNameRole,   "developerName"   },
        { HomepageRole,        "homepage"        },
        { SectionCategoryRole, "sectionCategory" },
        { SectionAddedRole,    "sectionAdded"    },
        { UpdateStateRole,     "updateState"     },
        { UpdateReasonRole,    "updateReason"    },
        { OlderCopyOfRole,     "olderCopyOf"     },
        { ListSectionRole,     "listSection"     },
        { CategoryLabelRole,   "categoryLabel"   },
    };
}

// static
// The first freedesktop main category in plain words ("Graphics", "Internet"); additional and
// vendor (X-…) categories are skipped unless there is nothing else.
QString AppImageListModel::categoryLabel(const Item &item)
{
    static const QList<std::pair<QString, KLocalizedString>> mainCategories = {
        { QStringLiteral("AudioVideo"),  ki18nc("app category", "Multimedia") },
        { QStringLiteral("Audio"),       ki18nc("app category", "Audio") },
        { QStringLiteral("Video"),       ki18nc("app category", "Video") },
        { QStringLiteral("Development"), ki18nc("app category", "Development") },
        { QStringLiteral("Education"),   ki18nc("app category", "Education") },
        { QStringLiteral("Game"),        ki18nc("app category", "Games") },
        { QStringLiteral("Graphics"),    ki18nc("app category", "Graphics") },
        { QStringLiteral("Network"),     ki18nc("app category", "Internet") },
        { QStringLiteral("Office"),      ki18nc("app category", "Office") },
        { QStringLiteral("Science"),     ki18nc("app category", "Science") },
        { QStringLiteral("Settings"),    ki18nc("app category", "Settings") },
        { QStringLiteral("System"),      ki18nc("app category", "System") },
        { QStringLiteral("Utility"),     ki18nc("app category", "Utilities") },
    };
    const QStringList categories = item.info.categories.split(QLatin1Char(';'), Qt::SkipEmptyParts);
    for (const QString &category : categories) {
        for (const auto &[name, label] : mainCategories) {
            if (category == name)
                return label.toString();
        }
    }
    for (const QString &category : categories) {
        if (!category.startsWith(QLatin1String("X-")))
            return category;
    }
    return i18nc("list section for apps without a category", "Other");
}

QString AppImageListModel::listSection(const Item &item) const
{
    const bool pending = hasUpdate(item) || item.isUpdating;
    if (m_sectionMode == 3) {
        if (pending)
            return i18nc("list section", "Updates available");
        switch (static_cast<UpdateState>(item.updateState)) {
        case UpdateState::Failed:   return i18nc("list section", "Check failed");
        case UpdateState::UpToDate: return i18nc("list section", "Up to date");
        case UpdateState::NoSource: return i18nc("list section", "No update source");
        case UpdateState::Pinned:   return i18nc("list section", "Pinned");
        default:                    return i18nc("list section", "Not checked yet");
        }
    }
    if (pending)
        return i18nc("list section", "Updates available");
    switch (m_sectionMode) {
    case 1:  return categoryLabel(item);
    case 2:  return addedGroup(item.addedDate);
    default: return m_updateCount > 0 ? i18nc("list section, below Updates available", "Installed") : QString();
    }
}

// static
QString AppImageListModel::addedGroup(const QDateTime &added)
{
    const qint64 days = added.date().daysTo(QDate::currentDate());
    if (days <= 0) return i18nc("list section", "Today");
    if (days < 7)  return i18nc("list section", "This Week");
    if (days < 31) return i18nc("list section", "This Month");
    return i18nc("list section", "Earlier");
}

bool AppImageListModel::isCheckingUpdates() const
{
    return m_updateManager->isChecking();
}

bool AppImageListModel::isDownloadWatcherSandboxed() const
{
    return m_downloadWatcher->isSandboxed();
}

// ── Scanning ──────────────────────────────────────────────────────────────────

void AppImageListModel::scan()
{
    if (m_scanning) return;
    refresh();
}

void AppImageListModel::refresh()
{
    ++m_generation;

    const QString dir = AppSettings::instance()->applicationsPath();
    const QFileInfoList files = QDir(dir).entryInfoList(kAppImageFilters(), QDir::Files);

    if (m_watchedDir != dir) {
        if (!m_watchedDir.isEmpty())
            m_watcher.removeDir(m_watchedDir);
        m_watcher.addDir(dir, KDirWatch::WatchFiles);
        m_watchedDir = dir;
    }

    QSet<QString> diskPaths;
    diskPaths.reserve(files.size());
    for (const QFileInfo &fi : files)
        diskPaths.insert(fi.absoluteFilePath());

    // Count stale in-flight futures (generation bump invalidated them).
    // Each stale future still arrives and decrements m_pendingLoads once.
    int staleFutures = 0;
    for (const Item &item : std::as_const(m_items))
        if (!item.metadataLoaded) ++staleFutures;

    // Remove items no longer on disk (backwards to keep indices stable)
    for (int i = m_items.size() - 1; i >= 0; --i) {
        if (!diskPaths.contains(m_items.at(i).filePath)) {
            beginRemoveRows({}, i, i);
            m_items.removeAt(i);
            endRemoveRows();
        }
    }

    QSet<QString> trackedPaths;
    trackedPaths.reserve(m_items.size());
    for (const Item &item : std::as_const(m_items))
        trackedPaths.insert(item.filePath);

    // Count re-triggers before dispatching — loadMetadataForRow may hit the
    // cache synchronously and decrement m_pendingLoads immediately, so the
    // assignment must happen first to avoid a stale non-zero count.
    int reTriggers = 0;
    for (const Item &item : std::as_const(m_items))
        if (!item.metadataLoaded) ++reTriggers;

    // Insert newly discovered files
    QList<QFileInfo> toAdd;
    for (const QFileInfo &fi : files) {
        if (!trackedPaths.contains(fi.absoluteFilePath()))
            toAdd.append(fi);
    }

    if (!toAdd.isEmpty()) {
        const int first = m_items.size();
        beginInsertRows({}, first, first + toAdd.size() - 1);
        for (const QFileInfo &fi : toAdd) {
            Item item;
            item.filePath            = fi.absoluteFilePath();
            item.addedDate           = fi.metadataChangeTime();
            item.info.originalName   = fi.fileName();
            item.info.cleanName      = fi.fileName();
            item.info.fileSize       = fi.size();
            item.cachedFormattedSize = formatBytes(fi.size());
            item.cachedDisplayName   = fi.fileName();
            m_items.append(item);
        }
        endInsertRows();
    }

    rebuildPathIndex();

    // Set m_pendingLoads before dispatching work. Synchronous cache hits in
    // loadMetadataForRow decrement immediately; assigning afterwards would
    // overwrite their decrements and leave a stuck non-zero count.
    // staleFutures — old-gen futures still in flight; each calls finalize() once
    // reTriggers   — new futures for existing unloaded items (+ 1 stale each = staleFutures)
    // toAdd.size() — new futures for freshly inserted items
    m_pendingLoads = staleFutures + reTriggers + static_cast<int>(toAdd.size());

    if (m_pendingLoads > 0) {
        if (!m_scanning) { m_scanning = true; Q_EMIT scanningChanged(); }
    } else {
        if (m_scanning)  { m_scanning = false; Q_EMIT scanningChanged(); }
    }

    // Dispatch after m_pendingLoads is live so synchronous decrements are safe.
    for (int i = 0; i < m_items.size(); ++i) {
        if (!m_items.at(i).metadataLoaded)
            loadMetadataForRow(i);
    }
}

void AppImageListModel::loadMetadataForRow(int row)
{
    const QString path = m_items.at(row).filePath;
    m_items[row].signatureState = -1; // a changed file is checked again
    m_items[row].signatureKey.clear();
    const int gen = m_generation;

    // Fast path: check the SQLite cache on the main thread before spawning a
    // worker. Cache reads are point lookups (PRIMARY KEY) and dominated by
    // thread-pool spawn/join overhead. For 100 already-known AppImages this
    // turns ~3 s of cold-start "scanning" into ~50 ms.
    const qint64 mtime = QFileInfo(path).lastModified().toMSecsSinceEpoch();
    const AppImageInfo cached = AppImageCache::instance().load(path, mtime);
    if (cached.isValid) {
        applyMetadata(row, cached);
        --m_pendingLoads;
        if (m_pendingLoads == 0 && m_scanning) {
            m_scanning = false;
            Q_EMIT scanningChanged();
        }
        return;
    }

    auto *watcher = new QFutureWatcher<AppImageInfo>(this);
    connect(watcher, &QFutureWatcher<AppImageInfo>::finished, this, [this, watcher, path, gen]() {
        watcher->deleteLater();

        auto finalize = [this]() {
            --m_pendingLoads;
            if (m_pendingLoads == 0 && m_scanning) {
                m_scanning = false;
                Q_EMIT scanningChanged();
            }
        };

        if (gen != m_generation) { finalize(); return; }

        // Locate by path — index may have shifted due to smart-diff inserts/removes
        const int row = findRowByPath(path);
        if (row < 0) { finalize(); return; }

        applyMetadata(row, watcher->result());
        finalize();
    });

    watcher->setFuture(QtConcurrent::run(&m_readerPool, [path]() {
        return AppImageReader(path).read();
    }));
}

static bool isAppImageFile(const QString &fileName)
{
    return fileName.endsWith(QStringLiteral(".AppImage"), Qt::CaseInsensitive)
        || fileName.endsWith(QStringLiteral(".appimage"), Qt::CaseInsensitive);
}

void AppImageListModel::onFileCreated(const QString &path)
{
    const QFileInfo fi(path);
    if (fi.absolutePath() != m_watchedDir || !isAppImageFile(fi.fileName()))
        return;
    if (findRowByPath(path) >= 0)
        return;

    const int row = m_items.size();
    beginInsertRows({}, row, row);
    Item item;
    item.filePath            = path;
    item.addedDate           = fi.metadataChangeTime();
    item.info.originalName   = fi.fileName();
    item.info.cleanName      = fi.fileName();
    item.info.fileSize       = fi.size();
    item.cachedFormattedSize = formatBytes(fi.size());
    item.cachedDisplayName   = fi.fileName();
    m_items.append(item);
    endInsertRows();
    m_pathIndex[path] = row;

    ++m_pendingLoads;
    if (!m_scanning) { m_scanning = true; Q_EMIT scanningChanged(); }
    loadMetadataForRow(row);
}

void AppImageListModel::onFileDeleted(const QString &path)
{
    const int row = findRowByPath(path);
    if (row < 0)
        return;
    beginRemoveRows({}, row, row);
    m_items.removeAt(row);
    endRemoveRows();
    rebuildPathIndex();
    updateOlderCopies();
}

void AppImageListModel::onFileDirty(const QString &path)
{
    // dirty fires on the watched dir itself too (mtime bump on child add /
    // remove); ignore that — the per-file created/deleted signals cover it.
    if (path == m_watchedDir)
        return;
    const int row = findRowByPath(path);
    if (row < 0)
        return;
    m_items[row].metadataLoaded = false;
    Q_EMIT dataChanged(index(row, 0), index(row, 0));

    ++m_pendingLoads;
    if (!m_scanning) { m_scanning = true; Q_EMIT scanningChanged(); }
    loadMetadataForRow(row);
}

void AppImageListModel::applyMetadata(int row, const AppImageInfo &info)
{
    const QString path = m_items.at(row).filePath;

    if (!info.iconData.isEmpty())
        m_iconProvider->setIconData(iconIdForPath(path), info.iconData, info.iconExt);

    Item &item          = m_items[row];
    item.info           = info;
    item.info.originalName = QFileInfo(path).fileName();
    // Drop our own entry when the app (or its launcher) wrote one for the same AppImage.
    AppImageManager::removeDuplicateDesktopLinks(path, item.info);
    item.hasDesktopLink = AppImageManager::isDesktopLinkEnabled(path, item.info);
    // Entries from before 2.3 lack the app's file types, actions and translations.
    if (item.hasDesktopLink && AppImageManager::desktopLinkOutdated(item.info))
        AppImageManager::createDesktopLink(path, item.info);
    item.metadataLoaded = true;
    item.cachedFormattedSize = formatBytes(info.fileSize);
    item.cachedDisplayName   = computeDisplayName(item);
    item.cachedIconSource    = computeIconSource(item);
    item.cachedDescription   = info.description;
    applyStoredResult(item);

    Q_EMIT dataChanged(index(row, 0), index(row, 0), {
        IconSourceRole, DisplayNameRole, CleanNameRole, AppNameRole,
        VersionRole, FormattedSizeRole, AppSizeRole, HasDesktopLinkRole,
        MetadataLoadedRole, CategoriesRole, CommentRole, DescriptionRole,
        DeveloperNameRole, HomepageRole, UpdateStateRole, UpdateReasonRole,
        UpdateAvailableRole, UpdateVersionRole, ListSectionRole, CategoryLabelRole,
        SectionCategoryRole
    });
    updateOlderCopies();
}

// ── Actions ───────────────────────────────────────────────────────────────────

void AppImageListModel::toggleDesktopLink(int row, bool enable)
{
    if (row < 0 || row >= m_items.size()) return;
    Item &item = m_items[row];
    const bool ok = enable
        ? AppImageManager::createDesktopLink(item.filePath, item.info)
        : AppImageManager::removeDesktopLink(item.filePath, item.info);
    if (ok) {
        item.hasDesktopLink = enable;
        Q_EMIT dataChanged(index(row, 0), index(row, 0), {HasDesktopLinkRole});
    } else {
        auto *n = new KNotification(QStringLiteral("error"),
                                    KNotification::CloseOnTimeout, this);
        n->setTitle(i18n("Desktop Link Failed"));
        n->setText(enable
            ? i18n("Could not create desktop link for %1.", item.info.appName)
            : i18n("Could not remove desktop link for %1.", item.info.appName));
        n->setIconName(QStringLiteral("dialog-error"));
        n->sendEvent();
    }
}

void AppImageListModel::requestRemoveAt(int row)
{
    if (row < 0 || row >= m_items.size()) return;
    Q_EMIT openUninstallWindow(m_items.at(row).filePath);
}

void AppImageListModel::requestLaunch(int row)
{
    if (row < 0 || row >= m_items.size()) return;
    if (!launchWithEnvironment(m_items.at(row).filePath))
        sendError(this, i18n("Launch Failed"),
                  i18n("Could not launch %1.", m_items.at(row).cachedDisplayName));
}

void AppImageListModel::checkForUpdates()
{
    QList<AppImageUpdateManager::CheckItem> items;
    items.reserve(m_items.size());
    for (const Item &item : std::as_const(m_items)) {
        // The update manager drops items without a supported (embedded or user-set) source.
        items.push_back({ item.filePath, item.info.updateInfo,
                          item.info.version, item.cachedDisplayName, item.info.appId });
    }
    m_updateManager->checkForUpdates(items);
}

QVariantMap AppImageListModel::updateSource(int row) const
{
    if (row < 0 || row >= m_items.size()) return {};
    const Item &item = m_items.at(row);
    const UpdateSource source = loadUpdateSource(item.filePath);
    QString input = source.value;
    if (source.type == UpdateSourceType::Forgejo)
        input = QStringLiteral("https://") + source.value;
    return {
        { QStringLiteral("input"), input },
        { QStringLiteral("type"), updateSourceTypeName(source.type) },
        { QStringLiteral("prerelease"), source.prerelease },
        { QStringLiteral("embedded"), item.info.updateInfo },
        { QStringLiteral("embeddedLabel"), describeUpdateInfo(item.info.updateInfo) },
    };
}

QVariantMap AppImageListModel::describeUpdateSourceInput(const QString &input, const QString &type) const
{
    const UpdateSource source = parseUpdateSourceInput(input, updateSourceTypeFromName(type));
    return {
        { QStringLiteral("valid"), source.type != UpdateSourceType::None },
        { QStringLiteral("type"), updateSourceTypeName(source.type) },
        { QStringLiteral("label"), describeUpdateInfo(effectiveUpdateInfo({}, source)) },
    };
}

QString AppImageListModel::environment(int row) const
{
    if (row < 0 || row >= m_items.size()) return {};
    return loadEnvironment(m_items.at(row).filePath).join(QLatin1Char('\n'));
}

QVariantList AppImageListModel::setEnvironment(int row, const QString &text)
{
    if (row < 0 || row >= m_items.size()) return {};
    const EnvironmentParse parsed = parseEnvironment(text);
    if (!parsed.invalidLines.isEmpty()) {
        QVariantList lines;
        for (int line : parsed.invalidLines)
            lines.append(line);
        return lines;
    }
    const Item &item = m_items.at(row);
    saveEnvironment(item.filePath, parsed.variables);
    // The app menu entry carries the environment in its Exec line: rewrite it.
    if (item.hasDesktopLink)
        AppImageManager::createDesktopLink(item.filePath, item.info);
    Q_EMIT dataChanged(index(row, 0), index(row, 0));
    return {};
}

QVariantMap AppImageListModel::configFolder(int row) const
{
    if (row < 0 || row >= m_items.size()) return {};
    const Item &item = m_items.at(row);
    const QString name = configFolderName(item.filePath);
    const QString path = QFileInfo(item.filePath).absolutePath() + QStringLiteral("/.config/")
                       + (name.isEmpty() ? defaultConfigFolderName(item.filePath) : name);
    QVariantList configDirs;
    if (name.isEmpty()) {
        for (const auto &dir : AppImageManager::findConfigDirs(item.info))
            configDirs.append(QVariantMap{ { QStringLiteral("path"), dir.path },
                                           { QStringLiteral("likely"), dir.confidence == AppImageManager::CorpseConfidence::High } });
    }
    QStringList contents;
    if (!name.isEmpty()) {
        for (const QFileInfo &child : QDir(path).entryInfoList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot))
            contents.append(child.absoluteFilePath());
    }
    return {
        { QStringLiteral("enabled"), !name.isEmpty() },
        { QStringLiteral("path"), path },
        { QStringLiteral("configDirs"), configDirs },
        { QStringLiteral("contents"), contents },
    };
}

void AppImageListModel::setConfigFolder(int row, bool enable, const QStringList &moveDirs)
{
    if (row < 0 || row >= m_items.size()) return;
    const Item &item = m_items.at(row);
    const QString filePath = item.filePath;
    QString target;
    if (enable) {
        setConfigFolderName(filePath, defaultConfigFolderName(filePath));
        target = configFolderPath(filePath);
    } else {
        target = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
        setConfigFolderName(filePath, {});
    }
    QDir().mkpath(target);
    // The app menu entry carries the folder in its Exec line: rewrite it.
    if (item.hasDesktopLink)
        AppImageManager::createDesktopLink(filePath, item.info);
    Q_EMIT dataChanged(index(row, 0), index(row, 0));

    // Never overwrite: a folder with the same name already at the target stays where it is.
    QList<QUrl> urls;
    QStringList skipped;
    for (const QString &dir : moveDirs) {
        if (QFileInfo::exists(target + QLatin1Char('/') + QFileInfo(dir).fileName()))
            skipped.append(QFileInfo(dir).fileName());
        else
            urls.append(QUrl::fromLocalFile(dir));
    }
    if (!skipped.isEmpty())
        sendError(this, i18n("Some Settings Were Not Moved"),
                  i18n("%1 already exists in %2.", skipped.join(QStringLiteral(", ")), target));
    if (urls.isEmpty())
        return;
    auto *job = KIO::move(urls, QUrl::fromLocalFile(target), KIO::HideProgressInfo);
    const QString oldFolder = enable ? QString() : QFileInfo(filePath).absolutePath() + QStringLiteral("/.config/")
                                                   + QFileInfo(moveDirs.first()).dir().dirName();
    connect(job, &KJob::result, this, [this, oldFolder](KJob *j) {
        if (j->error())
            sendError(this, i18n("Could Not Move Settings"), j->errorString());
        else if (!oldFolder.isEmpty())
            QDir().rmdir(oldFolder);  // only when empty
    });
    job->start();
}

QVariantMap AppImageListModel::sandbox(int row) const
{
    if (row < 0 || row >= m_items.size()) return {};
    const QString path = m_items.at(row).filePath;
    const SandboxSettings settings = loadSandbox(path);
    QString folder = configFolderPath(path);
    if (folder.isEmpty())
        folder = QFileInfo(path).absolutePath() + QStringLiteral("/.config/") + defaultConfigFolderName(path);
    return {
        { QStringLiteral("enabled"), settings.enabled },
        { QStringLiteral("folders"), settings.folders },
        { QStringLiteral("network"), settings.network },
        { QStringLiteral("available"), sandboxAvailable() },
        { QStringLiteral("settingsFolder"), folder },
    };
}

void AppImageListModel::setSandbox(int row, bool enabled, const QStringList &folders, bool network)
{
    if (row < 0 || row >= m_items.size()) return;
    const Item &item = m_items.at(row);
    if (enabled && configFolderName(item.filePath).isEmpty())
        setConfigFolderName(item.filePath, defaultConfigFolderName(item.filePath));
    saveSandbox(item.filePath, { enabled, folders, network });
    // The app menu entry starts it through appimagemanager --run when sandboxed: rewrite it.
    if (item.hasDesktopLink)
        AppImageManager::createDesktopLink(item.filePath, item.info);
    Q_EMIT dataChanged(index(row, 0), index(row, 0));
}

void AppImageListModel::checkSignature(int row)
{
    if (row < 0 || row >= m_items.size()) return;
    Item &item = m_items[row];
    if (item.signatureState >= 0 || item.signatureChecking)
        return;
    item.signatureChecking = true;
    const QString path = item.filePath;
    using Result = QPair<SignatureState, QString>;
    auto *watcher = new QFutureWatcher<Result>(this);
    connect(watcher, &QFutureWatcher<Result>::finished, this, [this, watcher, path]() {
        watcher->deleteLater();
        const int r = findRowByPath(path);
        if (r < 0)
            return;
        Item &it = m_items[r];
        it.signatureChecking = false;
        it.signatureState = int(watcher->result().first);
        it.signatureKey = watcher->result().second;
        Q_EMIT dataChanged(index(r, 0), index(r, 0));
    });
    watcher->setFuture(QtConcurrent::run(&m_readerPool, [path]() {
        QString fingerprint;
        const SignatureState state = AppImageReader::verifySignature(path, &fingerprint);
        return Result(state, fingerprint);
    }));
}

void AppImageListModel::fetchReleaseNotes(int row)
{
    if (row < 0 || row >= m_items.size()) return;
    const Item &item = m_items.at(row);
    const QString path = item.filePath;
    const UpdateSource source = loadUpdateSource(path);
    const QString info = effectiveUpdateInfo(item.info.updateInfo, source);
    const QString page = updateSourceWebUrl(info) + QStringLiteral("/releases");
    const QUrl url = releaseListUrl(info);
    if (!url.isValid()) {
        Q_EMIT releaseNotesReady(path, {}, {}, i18n("This app's update source has no release notes."));
        return;
    }
    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", "AppImageManager");
    request.setRawHeader("Accept", "application/json");
    request.setTransferTimeout(20000);
    QNetworkReply *reply = m_network->get(request);
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, path, page, version = item.info.version, tag = source.installedTag,
             prerelease = source.prerelease]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            Q_EMIT releaseNotesReady(path, {}, page, ReleaseChecker::describeReplyError(reply, true));
            return;
        }
        QVariantList notes;
        const QLocale locale;
        for (const ReleaseNote &note : pickReleaseNotes(reply->readAll(), version, tag, prerelease)) {
            notes.append(QVariantMap{
                { QStringLiteral("tag"), note.tag },
                { QStringLiteral("name"), note.name },
                { QStringLiteral("body"), note.body },
                { QStringLiteral("url"), note.url },
                { QStringLiteral("date"), note.published.isValid()
                      ? locale.toString(note.published.toLocalTime().date(), QLocale::LongFormat) : QString() },
                { QStringLiteral("prerelease"), note.prerelease },
            });
        }
        Q_EMIT releaseNotesReady(path, notes, page, notes.isEmpty() ? i18n("No releases found") : QString());
    });
}

bool AppImageListModel::restorePrevious(int row)
{
    if (row < 0 || row >= m_items.size()) return false;
    Item &item = m_items[row];
    if (item.isUpdating) return false;
    if (!::restorePrevious(item.filePath, item.info.version)) {
        sendError(this, i18n("Restore Failed"), i18n("The previous version of %1 could not be restored.", item.cachedDisplayName));
        return false;
    }
    // A different file now: what the last check said no longer applies.
    item.updateVersion.clear();
    item.updateUrl.clear();
    m_results.remove(item.info.originalName);
    saveCheckResult(item.info.originalName, {});
    setRowState(row, int(UpdateState::NotChecked));
    notifyDaemon();
    ++m_pendingLoads; // loadMetadataForRow decrements it
    loadMetadataForRow(row);
    return true;
}

bool AppImageListModel::setUpdateSource(int row, const QString &input, const QString &type, bool prerelease)
{
    if (row < 0 || row >= m_items.size()) return false;
    UpdateSource source = parseUpdateSourceInput(input, updateSourceTypeFromName(type));
    if (source.type == UpdateSourceType::None && !input.trimmed().isEmpty())
        return false;
    source.prerelease = prerelease;

    Item &item = m_items[row];
    saveUpdateSource(item.filePath, source);
    // The old result belongs to the old source: forget it (not checked yet, or no source).
    item.updateVersion.clear();
    item.updateUrl.clear();
    m_results.remove(item.info.originalName);
    saveCheckResult(item.info.originalName, {});
    applyStoredResult(item);
    Q_EMIT dataChanged(index(row, 0), index(row, 0),
                       {UpdateStateRole, UpdateReasonRole, UpdateAvailableRole, UpdateVersionRole, ListSectionRole});
    notifyDaemon();
    return true;
}

void AppImageListModel::setPinned(int row, bool pinned)
{
    if (row < 0 || row >= m_items.size()) return;
    Item &item = m_items[row];
    if (item.isUpdating) return;
    ::setPinned(item.filePath, pinned);
    // A waiting update is forgotten when pinning; un-pinned, the app is "not checked yet".
    item.updateVersion.clear();
    item.updateUrl.clear();
    m_results.remove(item.info.originalName);
    saveCheckResult(item.info.originalName, {});
    applyStoredResult(item);
    Q_EMIT dataChanged(index(row, 0), index(row, 0),
                       {UpdateStateRole, UpdateReasonRole, UpdateAvailableRole, UpdateVersionRole, ListSectionRole});
    recountUpdates();
    notifyDaemon();
}

void AppImageListModel::downloadUpdate(int row)
{
    if (row < 0 || row >= m_items.size()) return;
    Item &item = m_items[row];
    if (item.updateUrl.isEmpty() || item.isUpdating) return;

    item.isUpdating     = true;
    item.updateProgress = 0;
    Q_EMIT dataChanged(index(row, 0), index(row, 0), {IsUpdatingRole, UpdateProgressRole});

    m_updateManager->downloadUpdate(item.filePath, item.updateUrl,
                                    item.cachedDisplayName, item.info.appId, item.info.version);
}

// ── Private helpers ───────────────────────────────────────────────────────────

// static
QString AppImageListModel::iconIdForPath(const QString &path)
{
    return QString::number(qHash(path));
}

int AppImageListModel::findRowByPath(const QString &path) const
{
    return m_pathIndex.value(path, -1);
}

void AppImageListModel::rebuildPathIndex()
{
    m_pathIndex.clear();
    m_pathIndex.reserve(m_items.size());
    for (int i = 0; i < m_items.size(); ++i)
        m_pathIndex[m_items.at(i).filePath] = i;
}

// static
void AppImageListModel::sendError(QObject *parent, const QString &title, const QString &text)
{
    auto *n = new KNotification(QStringLiteral("error"), KNotification::CloseOnTimeout, parent);
    n->setTitle(title);
    n->setText(text);
    n->sendEvent();
}

// static
QString AppImageListModel::formatBytes(qint64 bytes)
{
    static const KFormat fmt;
    return fmt.formatByteSize(static_cast<double>(bytes));
}

// static
QString AppImageListModel::computeDisplayName(const Item &item)
{
    if (!item.info.appName.isEmpty() && item.info.appName != item.info.cleanName)
        return item.info.appName;
    return item.info.cleanName.isEmpty()
           ? QFileInfo(item.filePath).fileName()
           : item.info.cleanName;
}

QVariant AppImageListModel::sortKey(int row, int field) const
{
    if (row < 0 || row >= m_items.size())
        return {};
    const Item &item = m_items.at(row);
    // Field values mirror AppImageSortFilterModel::SortField (non-sequential
    // by historical accident). Kept as ints here so the model does not pull
    // the sort-filter header.
    enum { Name = 0, Size = 1, Category = 5, Date = 9 };
    switch (field) {
    case Size:     return item.info.fileSize;
    case Category: return categoryLabel(item);
    case Date:     return item.addedDate;
    case Name:
    default:       return item.cachedDisplayName;
    }
}

QString AppImageListModel::displayNameForRow(int row) const
{
    if (row < 0 || row >= m_items.size())
        return {};
    return m_items.at(row).cachedDisplayName;
}

QVariantMap AppImageListModel::itemData(int row) const
{
    if (row < 0 || row >= m_items.size())
        return {};
    const Item &item = m_items.at(row);
    const QString effective = effectiveUpdateInfo(item.info.updateInfo, loadUpdateSource(item.filePath));
    return {
        { QStringLiteral("filePath"),        item.filePath                                },
        { QStringLiteral("cleanName"),       item.info.cleanName                          },
        { QStringLiteral("appName"),         item.info.appName                            },
        { QStringLiteral("version"),         item.info.version                            },
        { QStringLiteral("iconSource"),      item.cachedIconSource                        },
        { QStringLiteral("hasDesktopLink"),  item.hasDesktopLink                          },
        { QStringLiteral("metadataLoaded"),  item.metadataLoaded                          },
        { QStringLiteral("appSize"),         item.info.fileSize                           },
        { QStringLiteral("formattedSize"),   item.cachedFormattedSize                     },
        { QStringLiteral("addedDate"),       item.addedDate                               },
        { QStringLiteral("displayName"),     item.cachedDisplayName                       },
        { QStringLiteral("updateAvailable"), hasUpdate(item)                              },
        { QStringLiteral("updateState"),     item.updateState                             },
        { QStringLiteral("updateReason"),    item.updateReason                            },
        { QStringLiteral("olderCopyOf"),     item.olderCopyOf                             },
        { QStringLiteral("categoryLabel"),   categoryLabel(item)                          },
        { QStringLiteral("updateVersion"),   item.updateVersion                           },
        { QStringLiteral("isUpdating"),      item.isUpdating                              },
        { QStringLiteral("updateProgress"),  item.updateProgress                          },
        { QStringLiteral("categories"),      item.info.categories                         },
        { QStringLiteral("comment"),         item.info.comment                            },
        { QStringLiteral("description"),     item.cachedDescription                       },
        { QStringLiteral("developerName"),   item.info.developerName                      },
        { QStringLiteral("homepage"),        item.info.homepage                           },
        { QStringLiteral("environment"),     loadEnvironment(item.filePath)                },
        { QStringLiteral("configFolder"),    configFolderPath(item.filePath)               },
        { QStringLiteral("hasPrevious"),     hasPrevious(item.filePath)                    },
        { QStringLiteral("sandboxed"),       loadSandbox(item.filePath).enabled            },
        { QStringLiteral("sandboxNetwork"),  loadSandbox(item.filePath).network            },
        { QStringLiteral("pinned"),          item.updateState == int(UpdateState::Pinned) },
        { QStringLiteral("signatureState"),  item.signatureState                           },
        { QStringLiteral("signatureKey"),    item.signatureKey                             },
        { QStringLiteral("previousVersion"), previousVersion(item.filePath)                },
        { QStringLiteral("updateSourceLabel"), describeUpdateInfo(effective)                },
        { QStringLiteral("updateSourceUrl"),   updateSourceWebUrl(effective)                },
    };
}

// static
QString AppImageListModel::computeIconSource(const Item &item)
{
    if (!item.metadataLoaded)
        return QStringLiteral("application-x-executable");
    if (!item.info.appId.isEmpty() && QIcon::hasThemeIcon(item.info.appId))
        return QStringLiteral("image://icon/%1").arg(item.info.appId);
    if (!item.info.iconData.isEmpty())
        return QStringLiteral("image://appimage/%1").arg(iconIdForPath(item.filePath));
    return QStringLiteral("application-x-executable");
}
