// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#pragma once

#include "appimageinfo.h"

#include <QMutex>
#include <QSettings>

// Thread-safe on-disk cache for AppImage metadata.
// Keyed by (path, mtime) — stale entries are ignored and overwritten.
// Call from any thread; all access is mutex-guarded.
class AppImageCache
{
public:
    static AppImageCache &instance();

    // Returns cached info if mtime matches, else invalid AppImageInfo{}.
    AppImageInfo load(const QString &path, qint64 mtime);

    void store(const QString &path, qint64 mtime, const AppImageInfo &info);

private:
    AppImageCache();
    QSettings m_settings;
    QMutex m_mutex;
};
