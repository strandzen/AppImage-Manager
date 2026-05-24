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
}

DownloadWatcher::DownloadWatcher(QObject *parent)
    : QObject(parent)
{
    connect(&m_fsWatcher, &QFileSystemWatcher::directoryChanged,
            this, &DownloadWatcher::onDirectoryChanged);

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
        seedKnown();
        m_fsWatcher.addPath(dlDir);
    } else {
        m_fsWatcher.removePath(dlDir);
        m_known.clear();
        m_pending.clear();
        m_settleTimer.stop();
    }
}

void DownloadWatcher::seedKnown()
{
    const QString dlDir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    m_known.clear();
    for (const QFileInfo &fi : QDir(dlDir).entryInfoList(kAppImageFilters(), QDir::Files))
        m_known.insert(fi.absoluteFilePath());
}

void DownloadWatcher::onDirectoryChanged()
{
    const QString dlDir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    const QFileInfoList entries = QDir(dlDir).entryInfoList(kAppImageFilters(), QDir::Files);

    QSet<QString> current;
    current.reserve(entries.size());
    for (const QFileInfo &fi : entries)
        current.insert(fi.absoluteFilePath());

    // Snapshot the current set: vanished entries are forgotten so a re-add
    // fires again; freshly-seen entries become known.
    m_known = current;

    // Newly-seen, not yet in the settle queue → start tracking.
    for (const QFileInfo &fi : entries) {
        const QString path = fi.absoluteFilePath();
        if (m_pending.contains(path))
            continue;
        if (m_known.contains(path) && QFileInfo(path).exists()) {
            // First-time observation: seed PendingFile so the next poll can
            // detect "size and mtime have stopped changing".
            PendingFile pf;
            pf.size  = fi.size();
            pf.mtime = fi.lastModified().toMSecsSinceEpoch();
            m_pending.insert(path, pf);
        }
    }

    if (!m_pending.isEmpty() && !m_settleTimer.isActive())
        m_settleTimer.start();
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
