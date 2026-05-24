// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#include "downloadwatcher.h"
#include "../core/appimagereader.h"
#include "../core/appimageinfo.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStandardPaths>

DownloadWatcher::DownloadWatcher(QObject *parent)
    : QObject(parent)
{
    connect(&m_fsWatcher, &QFileSystemWatcher::directoryChanged,
            this, &DownloadWatcher::onDirectoryChanged);
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
    for (const QFileInfo &fi : entries)
        current.insert(fi.absoluteFilePath());

    for (const QString &path : current) {
        if (!m_known.contains(path))
            Q_EMIT appImageFound(path, makeDisplayName(QFileInfo(path).fileName()));
    }

    // Snapshot the current set: previously-known entries that vanished are
    // dropped (so a re-add fires again), and freshly-seen entries become known.
    m_known = current;
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
