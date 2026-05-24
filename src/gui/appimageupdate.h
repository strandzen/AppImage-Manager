// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#pragma once

#include <QObject>
#include <QTimer>

class QNetworkAccessManager;

// Handles update checking (GitHub gh-releases-zsync and plain zsync|)
// and in-place downloading via the zsync2 subprocess.
//
// Lifecycle:
//   1. Call checkForUpdates(items) to start a batch check.
//      Each item produces exactly one updateFound/upToDate/failed signal.
//      checkFinished is emitted when all checks resolve (or on timeout).
//   2. Call downloadUpdate(...) per file to start a zsync2 subprocess.
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
    void downloadUpdate(const QString &filePath,
                        const QString &zsyncUrl,
                        const QString &displayName,
                        const QString &appIconId);

Q_SIGNALS:
    void checkingChanged();
    // updatesFound >= 0: batch complete. -1: timed out.
    void checkFinished(int updatesFound, int networkFailures);
    void updateFound(const QString &filePath, const QString &newVersion, const QString &zsyncUrl);
    void downloadProgress(const QString &filePath, int percent);
    // success=true: new file is in place; success=false: old file preserved.
    void downloadFinished(const QString &filePath, bool success);

private:
    void checkZsyncItem(const CheckItem &item);
    void finishOneCheck(bool foundUpdate, bool networkFailed = false);

    QNetworkAccessManager *m_nam;
    int m_pendingChecks    = 0;
    int m_updatesFound     = 0;
    int m_networkFailures  = 0;
    QTimer m_timeoutTimer;
};
