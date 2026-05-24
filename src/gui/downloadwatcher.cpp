// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#include "downloadwatcher.h"
#include "../core/appimagereader.h"
#include "../core/appimageinfo.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStandardPaths>

namespace {
constexpr int kSettleIntervalMs = 1500;  // recheck cadence while a file is settling
constexpr int kMaxSettlePolls   = 5;     // ~7.5s ceiling before we give up on a stalled write

bool isAppImagePath(const QString &path)
{
    return path.endsWith(QStringLiteral(".AppImage"), Qt::CaseInsensitive)
        || path.endsWith(QStringLiteral(".appimage"), Qt::CaseInsensitive);
}
}

DownloadWatcher::DownloadWatcher(QObject *parent)
    : QObject(parent)
{
    connect(&m_watcher, &KDirWatch::created, this, &DownloadWatcher::onCreated);
    connect(&m_watcher, &KDirWatch::deleted, this, &DownloadWatcher::onDeleted);

    m_settleTimer.setInterval(kSettleIntervalMs);
    connect(&m_settleTimer, &QTimer::timeout, this, &DownloadWatcher::pollSettle);
}

void DownloadWatcher::setEnabled(bool enabled)
{
    if (m_enabled == enabled)
        return;
    m_enabled = enabled;

    const QString dlDir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (dlDir.isEmpty())
        return;

    if (enabled) {
        // KDirWatch in WatchFiles mode does not replay pre-existing files,
        // so we can skip the seedKnown / dir-scan dance that QFileSystemWatcher
        // required.
        m_watcher.addDir(dlDir, KDirWatch::WatchFiles);
        m_watchedDir = dlDir;
    } else {
        if (!m_watchedDir.isEmpty()) {
            m_watcher.removeDir(m_watchedDir);
            m_watchedDir.clear();
        }
        m_pending.clear();
        m_settleTimer.stop();
    }
}

void DownloadWatcher::onCreated(const QString &path)
{
    if (path == m_watchedDir || !isAppImagePath(path))
        return;
    if (m_pending.contains(path))
        return;
    const QFileInfo fi(path);
    if (!fi.exists())
        return;
    PendingFile pf;
    pf.size  = fi.size();
    pf.mtime = fi.lastModified().toMSecsSinceEpoch();
    m_pending.insert(path, pf);
    if (!m_settleTimer.isActive())
        m_settleTimer.start();
}

void DownloadWatcher::onDeleted(const QString &path)
{
    m_pending.remove(path);
    if (m_pending.isEmpty())
        m_settleTimer.stop();
}

void DownloadWatcher::pollSettle()
{
    QStringList settled;
    QStringList expired;

    for (auto it = m_pending.begin(); it != m_pending.end(); ++it) {
        const QString &path = it.key();
        PendingFile &pf = it.value();

        const QFileInfo fi(path);
        if (!fi.exists()) {
            expired << path;
            continue;
        }

        const qint64 size  = fi.size();
        const qint64 mtime = fi.lastModified().toMSecsSinceEpoch();

        if (size == pf.size && mtime == pf.mtime && size > 0) {
            settled << path;
            continue;
        }

        pf.size  = size;
        pf.mtime = mtime;
        if (++pf.polls >= kMaxSettlePolls)
            expired << path;
    }

    for (const QString &path : expired)
        m_pending.remove(path);

    for (const QString &path : settled) {
        m_pending.remove(path);
        Q_EMIT appImageFound(path, makeDisplayName(QFileInfo(path).fileName()));
    }

    if (m_pending.isEmpty())
        m_settleTimer.stop();
}

// static
QString DownloadWatcher::makeDisplayName(const QString &fileName)
{
    QString name = AppImageReader::cleanName(fileName);
    name.remove(QStringLiteral(".AppImage"), Qt::CaseInsensitive);
    // Strip trailing version number (e.g. "ProtonUp-Qt 2.9.1")
    name.remove(QRegularExpression(QStringLiteral(R"(\s+\d[\d.]*$)")));
    return name.trimmed();
}
