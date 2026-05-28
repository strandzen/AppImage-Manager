// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#pragma once

#include <KDirWatch>

#include <QHash>
#include <QObject>
#include <QTimer>

// Watches ~/Downloads for newly appearing AppImage files via KDirWatch
// (which delivers per-file created/deleted events with internal de-duplication).
//
// Mid-download protection: a newly observed file is queued for a settle pass
// and only emitted once its size and mtime have been stable for
// kSettleIntervalMs. Otherwise we would notify while a browser is still
// streaming bytes and the file is unusable.
//
// D-Bus / notification logic stays in the caller — this class only discovers files.
class DownloadWatcher : public QObject
{
    Q_OBJECT
public:
    explicit DownloadWatcher(QObject *parent = nullptr);

    void setEnabled(bool enabled);
    bool isEnabled()    const { return m_enabled; }
    bool isSandboxed()  const { return m_sandboxed; }

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

    void onCreated(const QString &path);
    void onDeleted(const QString &path);
    void pollSettle();
    static QString makeDisplayName(const QString &fileName);

    KDirWatch                m_watcher;
    QString                  m_watchedDir;
    QHash<QString, PendingFile> m_pending;
    QTimer                   m_settleTimer;
    bool                     m_enabled   = false;
    bool                     m_sandboxed = false;
};
