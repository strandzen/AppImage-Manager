// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#include "appimagelistmodel.h"
#include "appimageiconprovider.h"
#include "../core/appimagemanager.h"
#include "../core/appimagereader.h"
#include "../core/appsettings.h"

#include <KFormat>
#include <QDir>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QIcon>
#include <QProcess>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>
#include <KNotification>
#include <KLocalizedString>
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
            this, [this]() { m_refreshTimer.start(); });

    connect(AppSettings::instance(), &AppSettings::applicationsPathChanged,
            this, [this]() {
                m_watcher.removePaths(m_watcher.directories());
                m_refreshTimer.start();
            });
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
    const QStringList filters = { QStringLiteral("*.AppImage"), QStringLiteral("*.appimage") };
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
        int row = -1;
        for (int i = 0; i < m_items.size(); ++i) {
            if (m_items.at(i).filePath == path) { row = i; break; }
        }
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
    QProcess::startDetached(m_items.at(row).filePath, {});
}

// static
QString AppImageListModel::iconIdForPath(const QString &path)
{
    return QString::number(qHash(path));
}

void AppImageListModel::checkForUpdates()
{
    for (int i = 0; i < m_items.size(); ++i) {
        const Item &item = m_items.at(i);
        if (item.info.updateInfo.startsWith(QStringLiteral("gh-releases-zsync|"))) {
            QStringList parts = item.info.updateInfo.split(QLatin1Char('|'));
            if (parts.size() >= 3) {
                QString owner = parts.at(1);
                QString repo = parts.at(2);
                QString url = QStringLiteral("https://api.github.com/repos/%1/%2/releases/latest").arg(owner, repo);
                
                QNetworkRequest request((QUrl(url)));
                request.setRawHeader("User-Agent", "AppImageManager-UpdateCheck/1.0");
                
                QString filePath = item.filePath;
                
                QNetworkReply *reply = m_networkManager->get(request);
                connect(reply, &QNetworkReply::finished, this, [this, reply, filePath]() {
                    reply->deleteLater();
                    if (reply->error() != QNetworkReply::NoError) {
                        return;
                    }
                    
                    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
                    if (doc.isObject()) {
                        QString tag = doc.object().value(QLatin1String("tag_name")).toString();
                        if (!tag.isEmpty()) {
                            QString versionStr = tag;
                            if (versionStr.startsWith(QLatin1Char('v')) || versionStr.startsWith(QLatin1Char('V'))) {
                                versionStr.remove(0, 1);
                            }
                            
                            // Locate by path — index may have shifted
                            int row = -1;
                            for (int idx = 0; idx < m_items.size(); ++idx) {
                                if (m_items.at(idx).filePath == filePath) { row = idx; break; }
                            }
                            if (row < 0) return;
                            
                            Item &modelItem = m_items[row];
                            QString currentVer = modelItem.info.version;
                            if (currentVer.startsWith(QLatin1Char('v')) || currentVer.startsWith(QLatin1Char('V'))) {
                                currentVer.remove(0, 1);
                            }
                            
                            if (versionStr != currentVer) {
                                modelItem.updateAvailable = true;
                                modelItem.updateVersion = versionStr;

                                // Find the zsync url
                                QJsonArray assets = doc.object().value(QLatin1String("assets")).toArray();
                                for (const QJsonValue &assetVal : assets) {
                                    QJsonObject asset = assetVal.toObject();
                                    QString name = asset.value(QLatin1String("name")).toString();
                                    if (name.endsWith(QLatin1String(".zsync"))) {
                                        modelItem.zsyncUrl = asset.value(QLatin1String("browser_download_url")).toString();
                                        break;
                                    }
                                }

                                Q_EMIT dataChanged(index(row, 0), index(row, 0), {UpdateAvailableRole, UpdateVersionRole});
                            }
                        }
                    }
                });
            }
        } else if (item.info.updateInfo.startsWith(QStringLiteral("zsync|"))) {
            QString url = item.info.updateInfo.mid(6);
            // We would need to fetch the zsync header to get the version, 
            // for now just make it available if we don't know the version
            int row = i;
            Item &modelItem = m_items[row];
            modelItem.updateAvailable = true;
            modelItem.zsyncUrl = url;
            modelItem.updateVersion = i18n("Unknown");
            Q_EMIT dataChanged(index(row, 0), index(row, 0), {UpdateAvailableRole, UpdateVersionRole});
        }
    }
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

    process->setProgram(QStringLiteral("zsync"));
    process->setArguments({
        QStringLiteral("-i"), oldFile,
        QStringLiteral("-o"), newFile,
        item.zsyncUrl
    });

    connect(process, &QProcess::readyReadStandardOutput, this, [this, process, row]() {
        if (row >= m_items.size()) return;
        
        QString output = QString::fromUtf8(process->readAllStandardOutput());
        static const QRegularExpression regex(QStringLiteral(R"((\d+\.\d+)%)"));
        
        auto match = regex.match(output);
        if (match.hasMatch()) {
            double percent = match.captured(1).toDouble();
            m_items[row].updateProgress = static_cast<int>(percent);
            Q_EMIT dataChanged(index(row, 0), index(row, 0), {UpdateProgressRole});
        }
    });

    connect(process, &QProcess::finished, this, [this, process, row, oldFile, newFile](int exitCode, QProcess::ExitStatus) {
        process->deleteLater();
        if (row >= m_items.size()) return;

        Item &item = m_items[row];
        item.isUpdating = false;

        if (exitCode == 0 && QFile::exists(newFile)) {
            // Replace old file with new file
            QFile::remove(oldFile);
            QFile::rename(newFile, oldFile);
            
            // Make executable
            QFile file(oldFile);
            file.setPermissions(file.permissions() | QFileDevice::ExeUser | QFileDevice::ExeGroup | QFileDevice::ExeOther);

            item.updateAvailable = false;
            item.updateProgress = 0;
            
            // Notify user
            KNotification *notification = new KNotification(QStringLiteral("updateDownloaded"), KNotification::CloseOnTimeout, this);
            notification->setTitle(i18n("Update Completed"));
            notification->setText(i18n("%1 has been successfully updated.", item.cachedDisplayName));
            notification->setIconName(item.info.appId.isEmpty() ? QStringLiteral("application-x-executable") : item.info.appId);
            notification->sendEvent();

            // Refresh metadata
            loadMetadataForRow(row);
        } else {
            // Failed
            if (QFile::exists(newFile)) {
                QFile::remove(newFile);
            }
            Q_EMIT dataChanged(index(row, 0), index(row, 0), {IsUpdatingRole, UpdateProgressRole});
        }
    });

    process->start();
}
