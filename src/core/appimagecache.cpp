// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#include "appimagecache.h"

#include <QCryptographicHash>
#include <QDir>
#include <QMutexLocker>
#include <QStandardPaths>

static QString keyFor(const QString &path)
{
    return QString::fromLatin1(
        QCryptographicHash::hash(path.toUtf8(), QCryptographicHash::Md5).toHex());
}

static QString cacheFilePath()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir + QStringLiteral("/cache.ini");
}

AppImageCache &AppImageCache::instance()
{
    static AppImageCache s;
    return s;
}

AppImageCache::AppImageCache()
    : m_settings(cacheFilePath(), QSettings::IniFormat)
{
}

static constexpr int kCacheVersion = 3;

AppImageInfo AppImageCache::load(const QString &path, qint64 mtime)
{
    QMutexLocker lock(&m_mutex);
    const QString key = keyFor(path);
    m_settings.beginGroup(key);
    const int ver = m_settings.value(QStringLiteral("cacheVersion"), 0).toInt();
    const qint64 cachedMtime = m_settings.value(QStringLiteral("mtime"), -1LL).toLongLong();
    if (ver != kCacheVersion || cachedMtime != mtime) {
        m_settings.endGroup();
        return {};
    }
    AppImageInfo info;
    info.originalName = m_settings.value(QStringLiteral("originalName")).toString();
    info.cleanName    = m_settings.value(QStringLiteral("cleanName")).toString();
    info.appId        = m_settings.value(QStringLiteral("appId")).toString();
    info.appName      = m_settings.value(QStringLiteral("appName")).toString();
    info.version      = m_settings.value(QStringLiteral("version")).toString();
    info.categories   = m_settings.value(QStringLiteral("categories")).toString();
    info.comment      = m_settings.value(QStringLiteral("comment")).toString();
    info.description  = m_settings.value(QStringLiteral("description")).toString();
    info.developerName= m_settings.value(QStringLiteral("developerName")).toString();
    info.homepage     = m_settings.value(QStringLiteral("homepage")).toString();
    info.execArgs     = m_settings.value(QStringLiteral("execArgs")).toString();
    info.fileSize     = m_settings.value(QStringLiteral("fileSize")).toLongLong();
    info.iconExt      = m_settings.value(QStringLiteral("iconExt")).toString();
    info.updateInfo   = m_settings.value(QStringLiteral("updateInfo")).toString();
    info.iconData     = QByteArray::fromBase64(
        m_settings.value(QStringLiteral("iconData")).toByteArray());
    info.isValid      = m_settings.value(QStringLiteral("isValid"), false).toBool();
    m_settings.endGroup();
    return info;
}

void AppImageCache::store(const QString &path, qint64 mtime, const AppImageInfo &info)
{
    QMutexLocker lock(&m_mutex);
    const QString key = keyFor(path);
    m_settings.beginGroup(key);
    m_settings.setValue(QStringLiteral("cacheVersion"), kCacheVersion);
    m_settings.setValue(QStringLiteral("mtime"),        mtime);
    m_settings.setValue(QStringLiteral("originalName"), info.originalName);
    m_settings.setValue(QStringLiteral("cleanName"),    info.cleanName);
    m_settings.setValue(QStringLiteral("appId"),        info.appId);
    m_settings.setValue(QStringLiteral("appName"),      info.appName);
    m_settings.setValue(QStringLiteral("version"),      info.version);
    m_settings.setValue(QStringLiteral("categories"),   info.categories);
    m_settings.setValue(QStringLiteral("comment"),      info.comment);
    m_settings.setValue(QStringLiteral("description"),   info.description);
    m_settings.setValue(QStringLiteral("developerName"), info.developerName);
    m_settings.setValue(QStringLiteral("homepage"),      info.homepage);
    m_settings.setValue(QStringLiteral("execArgs"),      info.execArgs);
    m_settings.setValue(QStringLiteral("fileSize"),     info.fileSize);
    m_settings.setValue(QStringLiteral("iconExt"),      info.iconExt);
    m_settings.setValue(QStringLiteral("updateInfo"),   info.updateInfo);
    m_settings.setValue(QStringLiteral("iconData"),     info.iconData.toBase64());
    m_settings.setValue(QStringLiteral("isValid"),      info.isValid);
    m_settings.endGroup();
    m_settings.sync();
}
