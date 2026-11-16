// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#pragma once

#include <QByteArray>
#include <QFile>
#include <QList>
#include <QObject>
#include <QPair>
#include <QUrl>
#include <QVector>
#include "appimagemanager_qml_export.h"

class QNetworkAccessManager;
class QNetworkReply;

// zsync without zsync2: builds the new version of a file from the blocks the local file
// already has and downloads only the rest with HTTP range requests.
//
// The .zsync control file: "Key: value" header lines, an empty line, then per block of the
// target file its weak rolling checksum (the last rsum_bytes of a and b, big-endian) and the
// first checksum_bytes of its MD4. The last block is padded with zeros.
class APPIMAGEMANAGER_EXPORT ZsyncDownloader : public QObject
{
    Q_OBJECT
public:
    struct Control {
        qint64 blockSize = 0;
        qint64 length = 0;
        int seqMatches = 1;
        int rsumBytes = 4;
        int checksumBytes = 16;
        QString url;   // as written (may be relative to the .zsync URL)
        QString sha1;  // lower-case hex
        QVector<quint32> rsums;       // per block, masked to rsumBytes
        QVector<QByteArray> checksums; // per block, checksumBytes of MD4
        int blockCount() const { return int(rsums.size()); }
    };

    static bool parseControl(const QByteArray &data, Control &control);
    // For each target block, an offset in the local data holding it, or -1.
    static QVector<qint64> matchBlocks(const Control &control, const uchar *local, qint64 size);
    // Weak checksum of one block, as stored (masked to rsumBytes).
    static quint32 blockRsum(const uchar *data, qint64 blockSize, int rsumBytes);
    // The byte ranges [start, end] to download: missing blocks, neighbours merged, gaps up to
    // maxGap bytes bridged to save requests.
    static QList<QPair<qint64, qint64>> missingRanges(const Control &control, const QVector<qint64> &found,
                                                      qint64 maxGap);

    ZsyncDownloader(QNetworkAccessManager *nam, const QString &localFile, const QUrl &zsyncUrl,
                    const QString &outFile, QObject *parent = nullptr);
    void start();

    // Known once the control file is read; for a full download when this one fails.
    QUrl targetUrl() const { return m_targetUrl; }
    QString sha1() const { return m_control.sha1; }

Q_SIGNALS:
    void progress(int percent);
    // ok: outFile holds the new version, its SHA-1 checked. Otherwise outFile is removed.
    void finished(bool ok);

private:
    void onControlFetched();
    void onBlocksMatched(const QVector<qint64> &found);
    void fetchNextRanges();
    void onRangeData(QNetworkReply *reply);
    void onRangeFinished(QNetworkReply *reply);
    void verify();
    void fail(const QString &why);

    QNetworkAccessManager *m_nam;
    QString m_localFile;
    QUrl m_zsyncUrl;
    QString m_outFile;
    QUrl m_targetUrl;
    Control m_control;

    QList<QPair<qint64, qint64>> m_ranges;  // still to request
    int m_running = 0;
    qint64 m_toFetch = 0;
    qint64 m_fetched = 0;
    QFile m_out;
    bool m_failed = false;
};
