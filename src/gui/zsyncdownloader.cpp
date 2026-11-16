// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#include "zsyncdownloader.h"
#include "logging.h"

#include <QCryptographicHash>
#include <QFutureWatcher>
#include <QHash>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QtConcurrent/QtConcurrent>

namespace {
constexpr int kParallelRanges = 4;
constexpr qint64 kMaxGap = 64 * 1024;         // bridge small gaps: fewer requests
constexpr int kMaxRequests = 300;              // more than this: a full download is quicker

quint32 maskFor(int rsumBytes)
{
    // Stored: the last rsumBytes of [a (2 bytes), b (2 bytes)], big-endian.
    switch (rsumBytes) {
    case 1: return 0x000000ffu;
    case 2: return 0x0000ffffu;
    case 3: return 0x00ffffffu;
    default: return 0xffffffffu;
    }
}

QByteArray strongChecksum(const uchar *data, qint64 blockSize, int bytes)
{
    return QCryptographicHash::hash(QByteArrayView(reinterpret_cast<const char *>(data), blockSize),
                                    QCryptographicHash::Md4).left(bytes);
}
}

quint32 ZsyncDownloader::blockRsum(const uchar *data, qint64 blockSize, int rsumBytes)
{
    quint16 a = 0, b = 0;
    for (qint64 i = 0; i < blockSize; ++i) {
        a += data[i];
        b += quint16((blockSize - i) * data[i]);
    }
    return ((quint32(a) << 16) | b) & maskFor(rsumBytes);
}

bool ZsyncDownloader::parseControl(const QByteArray &data, Control &control)
{
    const qsizetype bodyStart = data.indexOf("\n\n");
    if (bodyStart < 0)
        return false;
    for (const QByteArray &line : data.left(bodyStart).split('\n')) {
        const qsizetype colon = line.indexOf(':');
        if (colon <= 0)
            continue;
        const QByteArray key = line.left(colon).trimmed();
        const QByteArray value = line.mid(colon + 1).trimmed();
        if (key == "Blocksize") {
            control.blockSize = value.toLongLong();
        } else if (key == "Length") {
            control.length = value.toLongLong();
        } else if (key == "Hash-Lengths") {
            const QList<QByteArray> parts = value.split(',');
            if (parts.size() != 3)
                return false;
            control.seqMatches = parts.at(0).toInt();
            control.rsumBytes = parts.at(1).toInt();
            control.checksumBytes = parts.at(2).toInt();
        } else if (key == "URL") {
            control.url = QString::fromUtf8(value);
        } else if (key == "SHA-1") {
            control.sha1 = QString::fromLatin1(value).toLower();
        }
    }
    if (control.blockSize <= 0 || control.length < 0 || control.rsumBytes < 1 || control.rsumBytes > 4
        || control.checksumBytes < 3 || control.checksumBytes > 16 || control.seqMatches < 1 || control.seqMatches > 2)
        return false;

    const qint64 blocks = (control.length + control.blockSize - 1) / control.blockSize;
    const qint64 entry = control.rsumBytes + control.checksumBytes;
    const QByteArray body = data.mid(bodyStart + 2);
    if (body.size() < blocks * entry)
        return false;
    control.rsums.resize(blocks);
    control.checksums.resize(blocks);
    for (qint64 i = 0; i < blocks; ++i) {
        const auto *p = reinterpret_cast<const uchar *>(body.constData() + i * entry);
        quint32 r = 0;
        for (int k = 0; k < control.rsumBytes; ++k)
            r = (r << 8) | p[k];
        control.rsums[i] = r;
        control.checksums[i] = QByteArray(reinterpret_cast<const char *>(p + control.rsumBytes), control.checksumBytes);
    }
    return true;
}

QVector<qint64> ZsyncDownloader::matchBlocks(const Control &control, const uchar *local, qint64 size)
{
    const qint64 bs = control.blockSize;
    const int blocks = control.blockCount();
    QVector<qint64> found(blocks, -1);
    if (blocks == 0)
        return found;

    QHash<quint32, QList<int>> byRsum;
    byRsum.reserve(blocks);
    for (int i = 0; i < blocks; ++i)
        byRsum[control.rsums.at(i)].append(i);

    // Past its end the local data reads as zeros, like the padding of the target's last block.
    const qint64 end = size; // windows start in [0, size)
    const quint32 mask = maskFor(control.rsumBytes);
    const auto byteAt = [&](qint64 i) -> uchar { return i < size ? local[i] : 0; };
    QByteArray tail; // a window that runs past the end, padded
    const auto window = [&](qint64 pos) -> const uchar * {
        if (pos + bs <= size)
            return local + pos;
        tail = QByteArray(reinterpret_cast<const char *>(local + pos), size - pos);
        tail.append(QByteArray(bs - (size - pos), '\0'));
        return reinterpret_cast<const uchar *>(tail.constData());
    };
    const auto strongAt = [&](qint64 pos) { return strongChecksum(window(pos), bs, control.checksumBytes); };

    // Weak sums of the window at pos and, for seq_matches 2, of the one after it: with short
    // weak sums most positions hit the table, and checking the next block's weak sum first
    // keeps MD4 for real candidates (as zsync does).
    const bool pairs = control.seqMatches > 1;
    quint16 a = 0, b = 0, a2 = 0, b2 = 0;
    const auto sumsAt = [&](qint64 at, quint16 &sa, quint16 &sb) {
        sa = 0;
        sb = 0;
        for (qint64 i = 0; i < bs; ++i) {
            sa += byteAt(at + i);
            sb += quint16((bs - i) * byteAt(at + i));
        }
    };
    sumsAt(0, a, b);
    if (pairs)
        sumsAt(bs, a2, b2);
    int remaining = blocks;
    qint64 pos = 0;
    while (pos < end && remaining > 0) {
        bool jumped = false;
        const auto it = byRsum.constFind(((quint32(a) << 16) | b) & mask);
        if (it != byRsum.constEnd()) {
            const quint32 next = ((quint32(a2) << 16) | b2) & mask;
            QByteArray strong;
            for (int block : *it) {
                if (found.at(block) >= 0)
                    continue;
                if (pairs && block + 1 < blocks && control.rsums.at(block + 1) != next)
                    continue;
                if (strong.isNull())
                    strong = strongAt(pos);
                if (strong != control.checksums.at(block))
                    continue;
                // With seq_matches 2 the next block must follow, as zsync requires.
                if (control.seqMatches > 1 && block + 1 < blocks && pos + bs < end
                    && strongAt(pos + bs) != control.checksums.at(block + 1))
                    continue;
                found[block] = pos;
                --remaining;
                jumped = true;
            }
        }
        if (jumped && pos + bs < end) {
            // Matched: the next block most likely starts right after this one.
            pos += bs;
            if (pairs) {
                a = a2;  // the next window is the one already summed
                b = b2;
                sumsAt(pos + bs, a2, b2);
            } else {
                sumsAt(pos, a, b);
            }
            continue;
        }
        // Roll one byte.
        const uchar out = byteAt(pos);
        const uchar in = byteAt(pos + bs);
        a += quint16(in - out);
        b += quint16(a - bs * out);
        if (pairs) {
            const uchar in2 = byteAt(pos + 2 * bs);
            a2 += quint16(in2 - in);
            b2 += quint16(a2 - bs * in);
        }
        ++pos;
    }
    return found;
}

QList<QPair<qint64, qint64>> ZsyncDownloader::missingRanges(const Control &control, const QVector<qint64> &found,
                                                            qint64 maxGap)
{
    QList<QPair<qint64, qint64>> ranges;
    for (int i = 0; i < found.size(); ++i) {
        if (found.at(i) >= 0)
            continue;
        const qint64 start = i * control.blockSize;
        const qint64 end = qMin(start + control.blockSize, control.length) - 1;
        if (!ranges.isEmpty() && start - ranges.last().second - 1 <= maxGap)
            ranges.last().second = end;
        else
            ranges.append({ start, end });
    }
    return ranges;
}

ZsyncDownloader::ZsyncDownloader(QNetworkAccessManager *nam, const QString &localFile, const QUrl &zsyncUrl,
                                 const QString &outFile, QObject *parent)
    : QObject(parent)
    , m_nam(nam)
    , m_localFile(localFile)
    , m_zsyncUrl(zsyncUrl)
    , m_outFile(outFile)
{
}

void ZsyncDownloader::start()
{
    QNetworkRequest request(m_zsyncUrl);
    request.setRawHeader("User-Agent", "AppImageManager/zsync");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply *reply = m_nam->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            fail(QStringLiteral("control file: ") + reply->errorString());
            return;
        }
        if (!parseControl(reply->readAll(), m_control)) {
            fail(QStringLiteral("unreadable control file"));
            return;
        }
        m_targetUrl = m_zsyncUrl.resolved(QUrl(m_control.url));
        onControlFetched();
    });
}

void ZsyncDownloader::onControlFetched()
{
    Q_EMIT progress(1);
    // Matching reads the whole local file and writes the found blocks: off the UI thread.
    auto *watcher = new QFutureWatcher<QVector<qint64>>(this);
    connect(watcher, &QFutureWatcher<QVector<qint64>>::finished, this, [this, watcher]() {
        watcher->deleteLater();
        onBlocksMatched(watcher->result());
    });
    watcher->setFuture(QtConcurrent::run([control = m_control, local = m_localFile, out = m_outFile]() {
        QFile in(local);
        QFile dest(out);
        if (!in.open(QIODevice::ReadOnly) || !dest.open(QIODevice::WriteOnly | QIODevice::Truncate))
            return QVector<qint64>();
        const qint64 size = in.size();
        const uchar *data = size > 0 ? in.map(0, size) : nullptr;
        if (size > 0 && !data)
            return QVector<qint64>();
        const QVector<qint64> found = matchBlocks(control, data, size);
        if (!dest.resize(control.length))
            return QVector<qint64>();
        for (int i = 0; i < found.size(); ++i) {
            if (found.at(i) < 0)
                continue;
            const qint64 start = i * control.blockSize;
            const qint64 len = qMin(control.blockSize, control.length - start);
            const qint64 avail = qMin(len, size - found.at(i));
            dest.seek(start);
            dest.write(reinterpret_cast<const char *>(data + found.at(i)), avail);
        }
        return found;
    }));
}

void ZsyncDownloader::onBlocksMatched(const QVector<qint64> &found)
{
    if (found.size() != m_control.blockCount()) {
        fail(QStringLiteral("could not read the local file or write the new one"));
        return;
    }
    m_ranges = missingRanges(m_control, found, kMaxGap);
    for (const auto &range : std::as_const(m_ranges))
        m_toFetch += range.second - range.first + 1;
    qCDebug(AIM_LOG) << "zsync:" << m_control.blockCount() - found.count(-1) << "of" << m_control.blockCount()
                     << "blocks local," << m_toFetch << "bytes to download in" << m_ranges.size() << "requests";

    // Little to reuse, or scattered changes: one plain download is quicker than many ranges.
    if (m_toFetch > m_control.length * 3 / 4 || m_ranges.size() > kMaxRequests) {
        fail(QStringLiteral("too little to reuse; downloading the whole file"));
        return;
    }

    m_out.setFileName(m_outFile);
    if (!m_out.open(QIODevice::ReadWrite)) {
        fail(QStringLiteral("cannot open ") + m_outFile);
        return;
    }
    if (m_ranges.isEmpty()) {
        verify();
        return;
    }
    fetchNextRanges();
}

void ZsyncDownloader::fetchNextRanges()
{
    while (!m_failed && m_running < kParallelRanges && !m_ranges.isEmpty()) {
        // One range per request: no multipart responses to take apart.
        const auto range = m_ranges.takeFirst();
        const QByteArray header = "bytes=" + QByteArray::number(range.first) + '-' + QByteArray::number(range.second);
        QNetworkRequest request(m_targetUrl);
        request.setRawHeader("User-Agent", "AppImageManager/zsync");
        request.setRawHeader("Range", header);
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        QNetworkReply *reply = m_nam->get(request);
        reply->setProperty("offset", range.first);
        ++m_running;
        connect(reply, &QNetworkReply::readyRead, this, [this, reply]() { onRangeData(reply); });
        connect(reply, &QNetworkReply::finished, this, [this, reply]() { onRangeFinished(reply); });
    }
}

void ZsyncDownloader::onRangeData(QNetworkReply *reply)
{
    if (m_failed)
        return;
    // A server that ignores Range sends the whole file (200): not usable here.
    if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 206) {
        reply->abort();
        fail(QStringLiteral("the server does not support partial downloads"));
        return;
    }
    const QByteArray chunk = reply->readAll();
    const qint64 offset = reply->property("offset").toLongLong();
    m_out.seek(offset);
    m_out.write(chunk);
    reply->setProperty("offset", offset + chunk.size());
    m_fetched += chunk.size();
    if (m_toFetch > 0)
        Q_EMIT progress(int(1 + 97 * m_fetched / m_toFetch));
}

void ZsyncDownloader::onRangeFinished(QNetworkReply *reply)
{
    reply->deleteLater();
    --m_running;
    if (m_failed)
        return;
    if (reply->error() != QNetworkReply::NoError) {
        fail(QStringLiteral("range request: ") + reply->errorString());
        return;
    }
    onRangeData(reply); // anything left in the buffer
    if (m_failed)
        return;
    if (!m_ranges.isEmpty()) {
        fetchNextRanges();
    } else if (m_running == 0) {
        verify();
    }
}

void ZsyncDownloader::verify()
{
    m_out.close();
    Q_EMIT progress(99);
    auto *watcher = new QFutureWatcher<QString>(this);
    connect(watcher, &QFutureWatcher<QString>::finished, this, [this, watcher]() {
        watcher->deleteLater();
        if (!m_control.sha1.isEmpty() && watcher->result() != m_control.sha1) {
            fail(QStringLiteral("SHA-1 mismatch"));
            return;
        }
        Q_EMIT finished(true);
    });
    watcher->setFuture(QtConcurrent::run([path = m_outFile]() {
        QFile file(path);
        QCryptographicHash hash(QCryptographicHash::Sha1);
        if (!file.open(QIODevice::ReadOnly) || !hash.addData(&file))
            return QString();
        return QString::fromLatin1(hash.result().toHex());
    }));
}

void ZsyncDownloader::fail(const QString &why)
{
    if (m_failed)
        return;
    m_failed = true;
    qCWarning(AIM_LOG) << "zsync failed:" << why;
    m_out.close();
    QFile::remove(m_outFile);
    Q_EMIT finished(false);
}
