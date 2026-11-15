// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#pragma once

#include <QObject>
#include <QTimer>
#include <QQueue>

class QNetworkAccessManager;
class QUrl;

// Handles update checking (release sources via ReleaseChecker — GitHub, Forgejo/Codeberg, direct
// URL — and plain zsync|, after applying the user's per-AppImage source from updatesource.h) and
// in-place downloading: .zsync URLs via the zsync2 subprocess (or a full download when zsync2 is
// missing), anything else as a plain HTTP download that is refused when the server sends a web
// page instead of a file.
//
// Lifecycle:
//   1. Call checkForUpdates(items) to start a batch check.
//      Items are checked one at a time; updateFound is emitted for each available update.
//      checkFinished is emitted when all checks resolve, or with -1 when one check hangs.
//   2. Call downloadUpdate(...) per file to start a zsync2 subprocess or HTTP download.
//      downloadProgress emits as bytes arrive; downloadFinished on exit.
//
// Thread: all signals are emitted on the main thread.
class AppImageUpdateManager : public QObject
{
    Q_OBJECT
public:
    struct CheckItem {
        QString filePath;
        QString updateInfo;
        QString currentVersion;
        QString displayName;
        QString appIconId;
    };

    explicit AppImageUpdateManager(QNetworkAccessManager *nam, QObject *parent = nullptr);

    bool isChecking() const { return m_pendingChecks > 0; }

    void checkForUpdates(const QList<CheckItem> &items);
    // url: a .zsync control file or a direct .AppImage download.
    void downloadUpdate(const QString &filePath,
                        const QString &url,
                        const QString &displayName,
                        const QString &appIconId);

Q_SIGNALS:
    void checkingChanged();
    // updatesFound >= 0: batch complete. -1: timed out.
    void checkFinished(int updatesFound, int networkFailures);
    void updateFound(const QString &filePath, const QString &newVersion, const QString &url);
    void downloadProgress(const QString &filePath, int percent);
    // success=true: new file is in place; success=false: old file preserved.
    void downloadFinished(const QString &filePath, bool success);

private:
    void checkZsyncItem(const CheckItem &item, quint64 batch);
    void finishOneCheck(bool foundUpdate, bool networkFailed = false);
    void startFullHttpDownload(const QString &filePath,
                               const QString &zsyncUrl,
                               const QString &displayName,
                               const QString &appIconId);
    void downloadBinary(const QString &filePath,
                        const QUrl &binaryUrl,
                        const QString &remoteSha1,
                        const QString &displayName,
                        const QString &appIconId);
    static QString calculateFileSha1(const QString &filePath);
    void swapAndFinalizeDownload(const QString &filePath,
                                 const QString &newFile,
                                 const QString &displayName,
                                 const QString &appIconId);

    void checkNextItem();

    QNetworkAccessManager *m_nam;
    int m_pendingChecks    = 0;
    int m_updatesFound     = 0;
    int m_networkFailures  = 0;
    QTimer m_timeoutTimer;
    quint64 m_batch = 0; // bumped per batch and on timeout; stale replies compare against it
    QQueue<CheckItem> m_checkQueue;
};
