// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#pragma once

#include <QFileSystemWatcher>
#include <QObject>
#include <QSet>

// Watches ~/Downloads for newly appearing AppImage files.
// Call setEnabled(true) to start watching; the watcher deduplicates
// against already-present files so only genuinely new arrivals fire.
// D-Bus / notification logic stays in the caller — this class only discovers files.
class DownloadWatcher : public QObject
{
    Q_OBJECT
public:
    explicit DownloadWatcher(QObject *parent = nullptr);

    void setEnabled(bool enabled);
    bool isEnabled() const { return m_enabled; }

Q_SIGNALS:
    // Emitted once per new file. displayName is a human-readable app name
    // derived from the filename (arch suffixes stripped, capitalised).
    void appImageFound(const QString &path, const QString &displayName);

private:
    void onDirectoryChanged();
    void seedKnown();
    static QString makeDisplayName(const QString &fileName);

    QFileSystemWatcher m_fsWatcher;
    QSet<QString>      m_known;
    bool               m_enabled = false;
};
