// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#pragma once

#include "appimagemanager_qml_export.h"

#include <QByteArray>
#include <QHash>
#include <QReadWriteLock>
#include <QString>
#include <QStringList>

// Singleton raw-icon data store shared across all QML engine instances.
// Each AppImageIconProvider delegates writes here and reads raw bytes from here,
// keeping only rendered QPixmap caches per-engine (pixmaps must not cross engines).
class APPIMAGEMANAGER_EXPORT AppImageIconStore
{
public:
    static AppImageIconStore &instance();

    void set(const QString &id, const QByteArray &data, const QString &ext);
    bool get(const QString &id, QByteArray &data, QString &ext) const;

    // Returns all stored keys whose string starts with prefix (for cache invalidation).
    QStringList keysWithPrefix(const QString &prefix) const;

private:
    struct Entry { QByteArray data; QString ext; };
    QHash<QString, Entry> m_icons;
    mutable QReadWriteLock m_lock;
};
