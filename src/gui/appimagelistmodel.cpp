// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#include "appimagelistmodel.h"
#include "appimageiconprovider.h"
#include "../core/appimagemanager.h"
#include "../core/appimagereader.h"
#include "../core/appsettings.h"
#include "../core/githubreleasechecker.h"

#include <KFormat>
#include <KIO/CopyJob>
#include <KLocalizedString>
#include <KNotification>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDir>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QStandardPaths>
#include <QIcon>
#include <QNetworkReply>
#include <QProcess>
#include <QCryptographicHash>
#include <QRegularExpression>
#include <QtConcurrent/QtConcurrent>

AppImageListModel::AppImageListModel(AppImageIconProvider *iconProvider, QObject *parent)
    : QAbstractListModel(parent)
    , m_iconProvider(iconProvider)
    , m_networkManager(new QNetworkAccessManager(this))
{
    m_refreshTimer.setSingleShot(true);
    m_refreshTimer.setInterval(500);
    connect(&m_refreshTimer, &QTimer::timeout, this, &AppImageListModel::refresh);

    connect(&m_watcher, &QFileSystemWatcher::directoryChanged,
            this, [this](const QString &changedPath) {
                const QString dlDir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
                if (changedPath == dlDir)
                    checkNewDownloads();
                else
                    m_refreshTimer.start();
            });

    connect(AppSettings::instance(), &AppSettings::applicationsPathChanged,
            this, [this]() {
                m_watcher.removePaths(m_watcher.directories());
                m_refreshTimer.start();
            });

    connect(AppSettings::instance(), &AppSettings::watchDownloadsChanged,
            this, [this]() { updateDownloadWatcher(); });

    updateDownloadWatcher();
}

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
    case FilePathRole:       return item.filePath;
    case CleanNameRole:      return item.info.cleanName;
    case AppNameRole:        return item.info.appName;
    case VersionRole:        return item.info.version;
    case HasDesktopLinkRole: return item.hasDesktopLink;
    case MetadataLoadedRole: return item.metadataLoaded;
    case AppSizeRole:        return item.info.fileSize;
    case FormattedSizeRole:  return item.cachedFormattedSize;
    case AddedDateRole:      return item.addedDate;
    case DisplayNameRole:    return item.cachedDisplayName;
    case IconSourceRole:     return item.cachedIconSource;
    case UpdateAvailableRole:return item.updateAvailable;
    case UpdateVersionRole:  return item.updateVersion;
    case IsUpdatingRole:     return item.isUpdating;
    case UpdateProgressRole: return item.updateProgress;
    case IsSelectedRole:     return m_selected.contains(item.filePath);
    case CategoriesRole:     return item.info.categories;
    case CommentRole:        return item.info.comment;
    case DescriptionRole:    return item.cachedDescription;
    case DeveloperNameRole:  return item.info.developerName;
    case HomepageRole:       return item.info.homepage;
    default: return {};
    }
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

QHash<int, QByteArray> AppImageListModel::roleNames() const
{
    return {
        { FilePathRole,       "filePath"       },
        { CleanNameRole,      "cleanName"      },
        { AppNameRole,        "appName"        },
        { VersionRole,        "version"        },
        { IconSourceRole,     "iconSource"     },
        { HasDesktopLinkRole, "hasDesktopLink" },
        { MetadataLoadedRole, "metadataLoaded" },
        { AppSizeRole,        "appSize"        },
        { FormattedSizeRole,  "formattedSize"  },
        { AddedDateRole,      "addedDate"      },
        { DisplayNameRole,    "displayName"    },
        { UpdateAvailableRole,"updateAvailable"},
        { UpdateVersionRole,  "updateVersion"  },
        { IsUpdatingRole,     "isUpdating"     },
        { UpdateProgressRole, "updateProgress" },
        { IsSelectedRole,     "isSelected"     },
        { CategoriesRole,     "categories"     },
        { CommentRole,        "comment"        },
        { DescriptionRole,    "description"    },
        { DeveloperNameRole,  "developerName"  },
        { HomepageRole,       "homepage"       },
    };
}

void AppImageListModel::scan()
{
    if (m_scanning)
        return;
    refresh();
}

void AppImageListModel::refresh()
{
    ++m_generation;

    const QString dir = AppSettings::instance()->applicationsPath();
    const QStringList &filters = kAppImageFilters();
    const QFileInfoList files = QDir(dir).entryInfoList(filters, QDir::Files);

    if (!m_watcher.directories().contains(dir))
        m_watcher.addPath(dir);

    // Paths currently on disk
    QSet<QString> diskPaths;
    diskPaths.reserve(files.size());
    for (const QFileInfo &fi : files)
        diskPaths.insert(fi.absoluteFilePath());

    // Count in-flight futures that are now stale (gen bump invalidated them).
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

    // Paths now tracked in the model
    QSet<QString> trackedPaths;
    trackedPaths.reserve(m_items.size());
    for (const Item &item : std::as_const(m_items))
        trackedPaths.insert(item.filePath);

    // Re-trigger loading for existing items whose futures were invalidated by the gen bump.
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
    //   staleFutures          – old gen futures arriving and returning early
    //   reTriggers            – new futures spawned for existing unloaded items
    //   toAdd.size()          – new futures for newly inserted items
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

void AppImageListModel::toggleDesktopLink(int row, bool enable)
{
    if (row < 0 || row >= m_items.size())
        return;

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
    if (row < 0 || row >= m_items.size())
        return;
    Q_EMIT openUninstallWindow(m_items.at(row).filePath);
}

void AppImageListModel::requestLaunch(int row)
{
    if (row < 0 || row >= m_items.size())
        return;
    if (!QProcess::startDetached(m_items.at(row).filePath, {}))
        sendError(this, i18n("Launch Failed"), i18n("Could not launch %1.", m_items.at(row).cachedDisplayName));
}

// ──────────────────────────────────────────────────────────────────────────────
// Batch selection
// ──────────────────────────────────────────────────────────────────────────────

void AppImageListModel::setSelectionMode(bool mode)
{
    if (m_selectionMode == mode)
        return;
    m_selectionMode = mode;
    if (!mode)
        clearSelection();
    Q_EMIT selectionModeChanged();
}

void AppImageListModel::setSelected(const QString &path, bool selected)
{
    if (selected)
        m_selected.insert(path);
    else
        m_selected.remove(path);

    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items.at(i).filePath == path) {
            const auto idx = index(i);
            Q_EMIT dataChanged(idx, idx, {IsSelectedRole});
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
    if (m_selected.isEmpty())
        return;
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
        if (!m_selected.contains(item.filePath))
            continue;
        if (item.hasDesktopLink && item.metadataLoaded)
            AppImageManager::removeDesktopLink(item.filePath, item.info);
        urls << QUrl::fromLocalFile(item.filePath);
    }
    clearSelection();
    setSelectionMode(false);
    if (!urls.isEmpty())
        KIO::trash(urls)->start();
}

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

void AppImageListModel::finishOneUpdateCheck(bool foundUpdate)
{
    if (foundUpdate)
        ++m_updatesFoundInCheck;
    if (--m_pendingUpdateChecks == 0) {
        m_updateCheckTimer.stop();
        Q_EMIT checkingUpdatesChanged();
        Q_EMIT updateCheckFinished(m_updatesFoundInCheck);
    }
}

void AppImageListModel::checkForUpdates()
{
    int checkable = 0;
    for (const Item &item : std::as_const(m_items)) {
        if (item.info.updateInfo.startsWith(QStringLiteral("gh-releases-zsync|")) ||
            item.info.updateInfo.startsWith(QStringLiteral("zsync|")))
            ++checkable;
    }
    if (checkable == 0) {
        Q_EMIT updateCheckFinished(0);
        return;
    }

    m_pendingUpdateChecks = checkable;
    m_updatesFoundInCheck = 0;
    Q_EMIT checkingUpdatesChanged();

    m_updateCheckTimer.setSingleShot(true);
    m_updateCheckTimer.setInterval(30'000);
    connect(&m_updateCheckTimer, &QTimer::timeout, this, [this]() {
        m_pendingUpdateChecks = 0;
        Q_EMIT checkingUpdatesChanged();
        Q_EMIT updateCheckFinished(-1);
    }, Qt::SingleShotConnection);
    m_updateCheckTimer.start();

    for (int i = 0; i < m_items.size(); ++i) {
        const Item &item = m_items.at(i);
        if (item.info.updateInfo.startsWith(QStringLiteral("gh-releases-zsync|"))) {
            const QString filePath = item.filePath;
            auto *checker = new GitHubReleaseChecker(m_networkManager, this);
            connect(checker, &GitHubReleaseChecker::updateAvailable, this,
                    [this, filePath, checker](const QString &ver, const QString &url) {
                checker->deleteLater();
                const int row = findRowByPath(filePath);
                if (row >= 0) {
                    Item &modelItem = m_items[row];
                    modelItem.updateAvailable = true;
                    modelItem.updateVersion = ver;
                    modelItem.zsyncUrl = url;
                    Q_EMIT dataChanged(index(row, 0), index(row, 0), {UpdateAvailableRole, UpdateVersionRole});
                }
                finishOneUpdateCheck(row >= 0);
            });
            connect(checker, &GitHubReleaseChecker::upToDate, this, [this, checker]() {
                checker->deleteLater();
                finishOneUpdateCheck(false);
            });
            connect(checker, &GitHubReleaseChecker::failed, this, [this, checker]() {
                checker->deleteLater();
                finishOneUpdateCheck(false);
            });
            checker->check(item.info.updateInfo, item.info.version);
        } else if (item.info.updateInfo.startsWith(QStringLiteral("zsync|"))) {
            checkZsyncUpdate(i);
        }
    }
}

void AppImageListModel::checkZsyncUpdate(int row)
{
    const QString zsyncUrl = m_items.at(row).info.updateInfo.mid(6);
    const QString filePath = m_items.at(row).filePath;

    QNetworkRequest request((QUrl(zsyncUrl)));
    request.setRawHeader("User-Agent", "AppImageManager-UpdateCheck/1.0");
    request.setRawHeader("Range", "bytes=0-2047");

    QNetworkReply *reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, zsyncUrl, filePath]() {
        reply->deleteLater();

        const int row = findRowByPath(filePath);
        if (row < 0) { finishOneUpdateCheck(false); return; }

        QString remoteSha1;
        if (reply->error() == QNetworkReply::NoError) {
            const QByteArray data = reply->readAll();
            for (const QByteArray &line : data.split('\n')) {
                const QByteArray trimmed = line.trimmed();
                if (trimmed.isEmpty()) break;
                if (trimmed.startsWith("SHA-1: "))
                    remoteSha1 = QString::fromLatin1(trimmed.mid(7)).trimmed();
            }
        }

        if (!remoteSha1.isEmpty()) {
            QFile f(filePath);
            QString localSha1;
            if (f.open(QIODevice::ReadOnly)) {
                QCryptographicHash hash(QCryptographicHash::Sha1);
                hash.addData(&f);
                localSha1 = QString::fromLatin1(hash.result().toHex());
            }
            if (localSha1 == remoteSha1) { finishOneUpdateCheck(false); return; }
        }

        Item &modelItem = m_items[row];
        modelItem.updateAvailable = true;
        modelItem.zsyncUrl = zsyncUrl;
        modelItem.updateVersion = remoteSha1.isEmpty()
            ? i18n("New version available")
            : remoteSha1.left(8);
        Q_EMIT dataChanged(index(row, 0), index(row, 0), {UpdateAvailableRole, UpdateVersionRole});
        finishOneUpdateCheck(true);
    });
}

void AppImageListModel::downloadUpdate(int row)
{
    if (row < 0 || row >= m_items.size()) return;
    
    Item &item = m_items[row];
    if (item.zsyncUrl.isEmpty() || item.isUpdating) return;

    item.isUpdating = true;
    item.updateProgress = 0;
    Q_EMIT dataChanged(index(row, 0), index(row, 0), {IsUpdatingRole, UpdateProgressRole});

    QProcess *process = new QProcess(this);
    QString oldFile = item.filePath;
    QString newFile = oldFile + QStringLiteral(".new");

    process->setProgram(QStringLiteral("zsync2"));
    process->setArguments({
        QStringLiteral("-i"), oldFile,
        QStringLiteral("-o"), newFile,
        item.zsyncUrl
    });

    connect(process, &QProcess::readyReadStandardOutput, this, [this, process, oldFile]() {
        const int r = findRowByPath(oldFile);
        if (r < 0) return;

        QString output = QString::fromUtf8(process->readAllStandardOutput());
        static const QRegularExpression regex(QStringLiteral(R"((\d+\.\d+)%)"));
        auto match = regex.match(output);
        if (match.hasMatch()) {
            m_items[r].updateProgress = static_cast<int>(match.captured(1).toDouble());
            Q_EMIT dataChanged(index(r, 0), index(r, 0), {UpdateProgressRole});
        }
    });

    connect(process, &QProcess::finished, this, [this, process, oldFile, newFile](int exitCode, QProcess::ExitStatus) {
        process->deleteLater();

        const int r = findRowByPath(oldFile);
        if (r < 0) return;

        Item &item = m_items[r];
        item.isUpdating = false;

        if (exitCode == 0 && QFile::exists(newFile)) {
            if (!QFile::remove(oldFile) || !QFile::rename(newFile, oldFile)) {
                QFile::remove(newFile);
                sendError(this, i18n("Update Failed"),
                          i18n("Could not replace %1. The file may be in use.", item.cachedDisplayName));
                Q_EMIT dataChanged(index(r, 0), index(r, 0), {IsUpdatingRole, UpdateProgressRole});
                return;
            }

            QFile file(oldFile);
            file.setPermissions(file.permissions() | QFileDevice::ExeUser | QFileDevice::ExeGroup | QFileDevice::ExeOther);

            // Flash 100% before the progress bar disappears
            item.updateProgress = 100;
            Q_EMIT dataChanged(index(r, 0), index(r, 0), {UpdateProgressRole});

            item.updateAvailable = false;
            item.updateProgress = 0;

            auto *notification = new KNotification(QStringLiteral("updateDownloaded"), KNotification::CloseOnTimeout, this);
            notification->setTitle(i18n("Update Completed"));
            notification->setText(i18n("%1 has been successfully updated.", item.cachedDisplayName));
            notification->setIconName(item.info.appId.isEmpty() ? QStringLiteral("application-x-executable") : item.info.appId);
            notification->sendEvent();

            loadMetadataForRow(r);
        } else {
            if (QFile::exists(newFile))
                QFile::remove(newFile);
            sendError(this, i18n("Update Failed"), i18n("%1 could not be updated.", item.cachedDisplayName));
            Q_EMIT dataChanged(index(r, 0), index(r, 0), {IsUpdatingRole, UpdateProgressRole});
        }
    });

    process->start();
}

void AppImageListModel::updateDownloadWatcher()
{
    const QString dlDir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (dlDir.isEmpty())
        return;

    if (AppSettings::instance()->watchDownloads()) {
        const QStringList &filters = kAppImageFilters();
        m_knownDownloads.clear();
        for (const QFileInfo &fi : QDir(dlDir).entryInfoList(filters, QDir::Files))
            m_knownDownloads.insert(fi.absoluteFilePath());
        if (!m_watcher.directories().contains(dlDir))
            m_watcher.addPath(dlDir);
    } else {
        if (m_watcher.directories().contains(dlDir))
            m_watcher.removePath(dlDir);
    }
}

void AppImageListModel::checkNewDownloads()
{
    if (!AppSettings::instance()->watchDownloads())
        return;
    // Daemon handles download notifications when running — avoid duplicates
    const auto *iface = QDBusConnection::sessionBus().interface();
    if (iface && iface->isServiceRegistered(QStringLiteral("io.github.appimagemanager.Daemon")))
        return;

    const QString dlDir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    const QStringList &filters = kAppImageFilters();
    const QFileInfoList entries = QDir(dlDir).entryInfoList(filters, QDir::Files);

    for (const QFileInfo &fi : entries) {
        const QString path = fi.absoluteFilePath();
        if (m_knownDownloads.contains(path))
            continue;
        m_knownDownloads.insert(path);

        QString displayName = AppImageReader::cleanName(fi.fileName());
        displayName.remove(QStringLiteral(".AppImage"), Qt::CaseInsensitive);
        displayName.remove(QRegularExpression(QStringLiteral(R"(\s+\d[\d.]*$)")));
        displayName = displayName.trimmed();

        auto *n = new KNotification(QStringLiteral("downloaded"), KNotification::Persistent, this);
        n->setTitle(i18n("%1 downloaded", displayName));
        n->setIconName(QStringLiteral("application-x-executable"));
        auto *action = n->addAction(i18n("Manage"));
        connect(action, &KNotificationAction::activated, this, [path]() {
            QProcess::startDetached(QStringLiteral("appimagemanager"), { path });
        });
        n->sendEvent();
    }

    QSet<QString> current;
    for (const QFileInfo &fi : entries)
        current.insert(fi.absoluteFilePath());
    m_knownDownloads.intersect(current);
}
