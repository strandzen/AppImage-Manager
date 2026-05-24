// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#pragma once

#include <QFileSystemWatcher>
#include <QHash>
#include <QObject>
#include <QSet>
#include <QTimer>

// Watches ~/Downloads for newly appearing AppImage files.
// Call setEnabled(true) to start watching; the watcher deduplicates
// against already-present files so only genuinely new arrivals fire.
//
// Mid-download protection: a new file is only emitted once its size and
// mtime have been stable for kSettleIntervalMs; otherwise we'd notify
// while a browser is still streaming bytes and the file is unusable.
//
// D-Bus / notification logic stays in the caller — this class only discovers files.
class DownloadWatcher : public QObject
{
    Q_OBJECT
public:
    explicit DownloadWatcher(QObject *parent = nullptr);

    void setEnabled(bool enabled);
    bool isEnabled() const { return m_enabled; }

Q_SIGNALS:
    // Emitted once per new file once it has stopped growing. displayName is
    // a human-readable app name derived from the filename (arch suffixes
    // stripped, capitalised).
    void appImageFound(const QString &path, const QString &displayName);

private:
    struct PendingFile {
        qint64 size  = -1;
        qint64 mtime = -1;
        int    polls = 0;
    };

    void onDirectoryChanged();
    void pollSettle();
    void seedKnown();
    static QString makeDisplayName(const QString &fileName);

    QFileSystemWatcher       m_fsWatcher;
    QSet<QString>            m_known;
    QHash<QString, PendingFile> m_pending;
    QTimer                   m_settleTimer;
    bool                     m_enabled = false;
};
