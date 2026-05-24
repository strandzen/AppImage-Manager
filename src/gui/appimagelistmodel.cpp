// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#include "appimagelistmodel.h"
#include "appimageiconprovider.h"
#include "appimageupdate.h"
#include "downloadwatcher.h"
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
#include <QFileInfo>
#include <QFutureWatcher>
#include <QIcon>
#include <QNetworkAccessManager>
#include <QProcess>
#include <QCryptographicHash>
#include <QtConcurrent/QtConcurrent>

AppImageListModel::AppImageListModel(AppImageIconProvider *iconProvider, QObject *parent)
    : QAbstractListModel(parent)
    , m_iconProvider(iconProvider)
    , m_updateManager(new AppImageUpdateManager(new QNetworkAccessManager(this), this))
    , m_downloadWatcher(new DownloadWatcher(this))
{
    // ── Refresh timer: debounce filesystem watcher signals ────────────────────
    m_refreshTimer.setSingleShot(true);
    m_refreshTimer.setInterval(500);
    connect(&m_refreshTimer, &QTimer::timeout, this, &AppImageListModel::refresh);

    connect(&m_watcher, &QFileSystemWatcher::directoryChanged,
            this, [this](const QString &) { m_refreshTimer.start(); });

    connect(AppSettings::instance(), &AppSettings::applicationsPathChanged,
            this, [this]() {
                m_watcher.removePaths(m_watcher.directories());
                m_refreshTimer.start();
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
    case IsSelectedRole:      return m_selected.contains(item.filePath);
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
        { IsSelectedRole,      "isSelected"      },
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

    if (!m_watcher.directories().contains(dir))
        m_watcher.addPath(dir);

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

    // Re-trigger loading for existing items whose futures were invalidated.
    // Each produces 1 stale decrement (old future) + 1 real decrement (new future) = 2 total.
    int reTriggers = 0;
    for (int i = 0; i < m_items.size(); ++i) {
        if (!m_items.at(i).metadataLoaded) {
            ++reTriggers;
            loadMetadataForRow(i);
        }
    }

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
        for (int i = first; i < m_items.size(); ++i)
            loadMetadataForRow(i);
    }

    // Total expected decrements:
    //   staleFutures — old gen futures arriving and returning early
    //   reTriggers   — new futures for existing unloaded items
    //   toAdd.size() — new futures for newly inserted items
    // Re-triggered items produce 2 decrements each (stale + new), hence staleFutures + reTriggers.
    m_pendingLoads = staleFutures + reTriggers + static_cast<int>(toAdd.size());

    if (m_pendingLoads > 0) {
        if (!m_scanning) { m_scanning = true; Q_EMIT scanningChanged(); }
    } else {
        if (m_scanning)  { m_scanning = false; Q_EMIT scanningChanged(); }
    }
}

void AppImageListModel::loadMetadataForRow(int row)
{
    const QString path = m_items.at(row).filePath;
    const int gen = m_generation;

    auto *watcher = new QFutureWatcher<AppImageInfo>(this);
    connect(watcher, &QFutureWatcher<AppImageInfo>::finished, this, [this, watcher, path, gen]() {
        watcher->deleteLater();

        auto finalize = [this]() {
            if (--m_pendingLoads == 0 && m_scanning) {
                m_scanning = false;
                Q_EMIT scanningChanged();
            }
        };

        if (gen != m_generation) { finalize(); return; }

        // Locate by path — index may have shifted due to smart-diff inserts/removes
        const int row = findRowByPath(path);
        if (row < 0) { finalize(); return; }

        AppImageInfo info = watcher->result();
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

        Q_EMIT dataChanged(index(row, 0), index(row, 0));
        finalize();
    });

    watcher->setFuture(QtConcurrent::run([path]() {
        return AppImageReader(path).read();
    }));
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
        Q_EMIT dataChanged(index(row, 0), index(row, 0));
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

// ── Batch selection ───────────────────────────────────────────────────────────

void AppImageListModel::setSelectionMode(bool mode)
{
    if (m_selectionMode == mode) return;
    m_selectionMode = mode;
    if (!mode) clearSelection();
    Q_EMIT selectionModeChanged();
}

void AppImageListModel::setSelected(const QString &path, bool selected)
{
    if (selected) m_selected.insert(path);
    else          m_selected.remove(path);
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items.at(i).filePath == path) {
            Q_EMIT dataChanged(index(i), index(i), {IsSelectedRole});
            break;
        }
    }
    Q_EMIT selectionChanged();
}

void AppImageListModel::selectAll()
{
    for (const Item &item : std::as_const(m_items))
        m_selected.insert(item.filePath);
    if (!m_items.isEmpty())
        Q_EMIT dataChanged(index(0), index(rowCount() - 1), {IsSelectedRole});
    Q_EMIT selectionChanged();
}

void AppImageListModel::clearSelection()
{
    if (m_selected.isEmpty()) return;
    m_selected.clear();
    if (!m_items.isEmpty())
        Q_EMIT dataChanged(index(0), index(rowCount() - 1), {IsSelectedRole});
    Q_EMIT selectionChanged();
}

QStringList AppImageListModel::selectedPaths() const
{
    return QStringList(m_selected.begin(), m_selected.end());
}

void AppImageListModel::trashSelected()
{
    QList<QUrl> urls;
    for (const Item &item : std::as_const(m_items)) {
        if (!m_selected.contains(item.filePath)) continue;
        if (item.hasDesktopLink && item.metadataLoaded)
            AppImageManager::removeDesktopLink(item.filePath, item.info);
        urls << QUrl::fromLocalFile(item.filePath);
    }
    clearSelection();
    setSelectionMode(false);
    if (!urls.isEmpty())
        KIO::trash(urls)->start();
}

// ── Private helpers ───────────────────────────────────────────────────────────

// static
QString AppImageListModel::iconIdForPath(const QString &path)
{
    return QString::number(qHash(path));
}

int AppImageListModel::findRowByPath(const QString &path) const
{
    for (int i = 0; i < m_items.size(); ++i)
        if (m_items.at(i).filePath == path) return i;
    return -1;
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
