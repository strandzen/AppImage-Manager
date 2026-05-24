// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#pragma once

#include "appimageinfo.h"

// Thread-safe on-disk cache for AppImage metadata backed by SQLite (WAL mode).
// Each calling thread uses its own database connection to satisfy Qt's
// "one connection per thread" requirement; SQLite's WAL mode handles
// file-level concurrency between connections.
//
// Keyed by (path, mtime) — stale entries are silently ignored and overwritten.
// Call from any thread.
class AppImageCache
{
public:
    static AppImageCache &instance();

    // Returns cached info if mtime matches and cache_version is current, else invalid AppImageInfo{}.
    AppImageInfo load(const QString &path, qint64 mtime);

    void store(const QString &path, qint64 mtime, const AppImageInfo &info);

private:
    AppImageCache();
    QString m_dbPath;
};
