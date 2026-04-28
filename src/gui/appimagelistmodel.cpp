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
#include <QtConcurrent/QtConcurrent>

AppImageListModel::AppImageListModel(AppImageIconProvider *iconProvider, QObject *parent)
    : QAbstractListModel(parent)
    , m_iconProvider(iconProvider)
    , m_manager(new AppImageManager(this))
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
    case CleanNameRole:      return item.info.cleanName.isEmpty()
                                    ? QFileInfo(item.filePath).fileName()
                                    : item.info.cleanName;
    case AppNameRole:        return item.info.appName;
    case VersionRole:        return item.info.version;
    case HasDesktopLinkRole: return item.hasDesktopLink;
    case MetadataLoadedRole: return item.metadataLoaded;
    case AppSizeRole:        return item.info.fileSize;
    case FormattedSizeRole: {
        static const KFormat fmt;
        return fmt.formatByteSize(static_cast<double>(item.info.fileSize));
    }
    case AddedDateRole:      return item.addedDate;
    case DisplayNameRole: {
        if (!item.info.appName.isEmpty() && item.info.appName != item.info.cleanName)
            return item.info.appName;
        return item.info.cleanName.isEmpty()
               ? QFileInfo(item.filePath).fileName()
               : item.info.cleanName;
    }
    case IconSourceRole: {
        if (!item.metadataLoaded)
            return QStringLiteral("application-x-executable");
        if (!item.info.appId.isEmpty() && QIcon::hasThemeIcon(item.info.appId))
            return QStringLiteral("image://icon/%1").arg(item.info.appId);
        if (!item.info.iconData.isEmpty())
            return QStringLiteral("image://appimage/%1").arg(iconIdForPath(item.filePath));
        return QStringLiteral("application-x-executable");
    }
    default: return {};
    }
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
    };
}

void AppImageListModel::scan()
{
    if (m_scanning)
        return;

    m_scanning = true;
    Q_EMIT scanningChanged();

    const QString dir = AppSettings::instance()->applicationsPath();
    const QStringList filters = { QStringLiteral("*.AppImage"), QStringLiteral("*.appimage") };
    const QFileInfoList files = QDir(dir).entryInfoList(filters, QDir::Files);

    if (!m_watcher.directories().contains(dir))
        m_watcher.addPath(dir);

    if (!files.isEmpty()) {
        beginInsertRows({}, 0, files.size() - 1);
        for (const QFileInfo &fi : files) {
            Item item;
            item.filePath          = fi.absoluteFilePath();
            item.addedDate         = fi.metadataChangeTime();
            item.info.originalName = fi.fileName();
            item.info.cleanName    = fi.fileName();
            item.info.fileSize     = fi.size();
            m_items.append(item);
        }
        endInsertRows();

        m_pendingLoads = m_items.size();
        for (int i = 0; i < m_items.size(); ++i)
            loadMetadataForRow(i);
    } else {
        m_scanning = false;
        Q_EMIT scanningChanged();
    }
}

void AppImageListModel::refresh()
{
    ++m_generation;
    beginResetModel();
    m_items.clear();
    endResetModel();
    scan();
}

void AppImageListModel::loadMetadataForRow(int row)
{
    const QString path = m_items.at(row).filePath;
    const int gen = m_generation;

    auto *watcher = new QFutureWatcher<AppImageInfo>(this);
    connect(watcher, &QFutureWatcher<AppImageInfo>::finished, this, [this, watcher, row, gen]() {
        watcher->deleteLater();
        if (gen != m_generation || row >= m_items.size()) {
            if (--m_pendingLoads == 0 && m_scanning) {
                m_scanning = false;
                Q_EMIT scanningChanged();
            }
            return;
        }

        AppImageInfo info = watcher->result();
        info.originalName = QFileInfo(m_items.at(row).filePath).fileName();

        if (!info.iconData.isEmpty())
            m_iconProvider->setIconData(iconIdForPath(m_items.at(row).filePath),
                                        info.iconData, info.iconExt);

        m_items[row].info           = info;
        m_items[row].hasDesktopLink = m_manager->isDesktopLinkEnabled(m_items.at(row).filePath, info);
        m_items[row].metadataLoaded = true;

        Q_EMIT dataChanged(index(row, 0), index(row, 0));

        if (--m_pendingLoads == 0 && m_scanning) {
            m_scanning = false;
            Q_EMIT scanningChanged();
        }
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
        ? m_manager->createDesktopLink(item.filePath, item.info)
        : m_manager->removeDesktopLink(item.filePath, item.info);

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
