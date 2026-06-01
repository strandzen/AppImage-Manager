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

#include <KFormat>
#include <KIO/CopyJob>
#include <KLocalizedString>
#include <KNotification>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDir>
#include <QGuiApplication>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QIcon>
#include <QNetworkAccessManager>
#include <QProcess>
#include <QCryptographicHash>
#include <QThread>
#include <QtConcurrent/QtConcurrent>

AppImageListModel::AppImageListModel(AppImageIconProvider *iconProvider, QObject *parent)
    : QAbstractListModel(parent)
    , m_iconProvider(iconProvider)
    , m_updateManager(new AppImageUpdateManager(new QNetworkAccessManager(this), this))
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

    // ── Update manager signals ────────────────────────────────────────────────
    connect(m_updateManager, &AppImageUpdateManager::checkingChanged,
            this, &AppImageListModel::checkingUpdatesChanged);

    connect(m_updateManager, &AppImageUpdateManager::checkFinished,
            this, &AppImageListModel::updateCheckFinished);

    connect(m_updateManager, &AppImageUpdateManager::updateFound,
            this, [this](const QString &filePath, const QString &version, const QString &zsyncUrl) {
                const int row = findRowByPath(filePath);
                if (row < 0) return;
                Item &item = m_items[row];
                item.updateAvailable = true;
                item.updateVersion   = version;
                item.zsyncUrl        = zsyncUrl;
                Q_EMIT dataChanged(index(row, 0), index(row, 0),
                                   {UpdateAvailableRole, UpdateVersionRole});
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
                    item.updateAvailable = false;
                    item.updateProgress  = 0;
                    loadMetadataForRow(row);
                }
                Q_EMIT dataChanged(index(row, 0), index(row, 0),
                                   {IsUpdatingRole, UpdateAvailableRole, UpdateProgressRole});
                if (!success)
                    sendError(this, i18n("Update Failed"),
                              i18n("%1 could not be updated.", item.cachedDisplayName));
            });
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
    case UpdateAvailableRole: return item.updateAvailable;
    case UpdateVersionRole:   return item.updateVersion;
    case IsUpdatingRole:      return item.isUpdating;
    case UpdateProgressRole:  return item.updateProgress;
    case CategoriesRole:      return item.info.categories;
    case CommentRole:         return item.info.comment;
    case DescriptionRole:     return item.cachedDescription;
    case DeveloperNameRole:   return item.info.developerName;
    case HomepageRole:        return item.info.homepage;
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
    };
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

    // Set pendingLoads before dispatching work. Synchronous cache hits in
    // loadMetadataForRow decrement immediately; assigning afterwards would
    // overwrite their decrements and leave a stuck non-zero count.
    // staleFutures — old-gen futures still in flight; each calls finalize() once
    // reTriggers   — new futures for existing unloaded items (+ 1 stale each = staleFutures)
    // toAdd.size() — new futures for freshly inserted items
    m_pendingLoads = staleFutures + reTriggers + static_cast<int>(toAdd.size());
    Q_EMIT pendingLoadsChanged();

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
        Q_EMIT pendingLoadsChanged();
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
            Q_EMIT pendingLoadsChanged();
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
    Q_EMIT pendingLoadsChanged();
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
    Q_EMIT pendingLoadsChanged();
    if (!m_scanning) { m_scanning = true; Q_EMIT scanningChanged(); }
    loadMetadataForRow(row);
}

void AppImageListModel::applyMetadata(int row, AppImageInfo info)
{
    const QString path = m_items.at(row).filePath;
    info.originalName = QFileInfo(path).fileName();

    if (!info.iconData.isEmpty())
        m_iconProvider->setIconData(iconIdForPath(path), info.iconData, info.iconExt);

    Item &item          = m_items[row];
    item.info           = info;
    item.hasDesktopLink = AppImageManager::isDesktopLinkEnabled(path, info);
    item.metadataLoaded = true;
    item.cachedFormattedSize = formatBytes(info.fileSize);
    item.cachedDisplayName   = computeDisplayName(item);
    item.cachedIconSource    = computeIconSource(item);
    item.cachedDescription   = info.description;

    Q_EMIT dataChanged(index(row, 0), index(row, 0), {
        IconSourceRole, DisplayNameRole, CleanNameRole, AppNameRole,
        VersionRole, FormattedSizeRole, AppSizeRole, HasDesktopLinkRole,
        MetadataLoadedRole, CategoriesRole, CommentRole, DescriptionRole,
        DeveloperNameRole, HomepageRole
    });
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
    if (!QProcess::startDetached(m_items.at(row).filePath, {}))
        sendError(this, i18n("Launch Failed"),
                  i18n("Could not launch %1.", m_items.at(row).cachedDisplayName));
}

void AppImageListModel::checkForUpdates()
{
    QList<AppImageUpdateManager::CheckItem> items;
    items.reserve(m_items.size());
    for (const Item &item : std::as_const(m_items)) {
        if (item.info.updateInfo.startsWith(QLatin1String("gh-releases-zsync|")) ||
            item.info.updateInfo.startsWith(QLatin1String("zsync|"))) {
            items.push_back({ item.filePath, item.info.updateInfo,
                              item.info.version, item.cachedDisplayName, item.info.appId });
        }
    }
    m_updateManager->checkForUpdates(items);
}

void AppImageListModel::downloadUpdate(int row)
{
    if (row < 0 || row >= m_items.size()) return;
    Item &item = m_items[row];
    if (item.zsyncUrl.isEmpty() || item.isUpdating) return;

    item.isUpdating     = true;
    item.updateProgress = 0;
    Q_EMIT dataChanged(index(row, 0), index(row, 0), {IsUpdatingRole, UpdateProgressRole});

    m_updateManager->downloadUpdate(item.filePath, item.zsyncUrl,
                                    item.cachedDisplayName, item.info.appId);
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
    case Category: return item.info.categories;
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
        { QStringLiteral("updateAvailable"), item.updateAvailable                         },
        { QStringLiteral("updateVersion"),   item.updateVersion                           },
        { QStringLiteral("isUpdating"),      item.isUpdating                              },
        { QStringLiteral("updateProgress"),  item.updateProgress                          },
        { QStringLiteral("categories"),      item.info.categories                         },
        { QStringLiteral("comment"),         item.info.comment                            },
        { QStringLiteral("description"),     item.cachedDescription                       },
        { QStringLiteral("developerName"),   item.info.developerName                      },
        { QStringLiteral("homepage"),        item.info.homepage                           },
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
