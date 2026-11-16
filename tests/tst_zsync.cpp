// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
//
// Built-in zsync: control file parsing, block matching against a changed local file, and
// the ranges left to download. The control data is made here the way zsyncmake does
// (rolling checksum + MD4 per zero-padded block).

#include "../src/gui/zsyncdownloader.h"

#include <QCryptographicHash>
#include <QRandomGenerator>
#include <QElapsedTimer>
#include <QNetworkAccessManager>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

class TstZsync : public QObject
{
    Q_OBJECT

    static QByteArray randomBytes(int size, quint32 seed)
    {
        QRandomGenerator gen(seed);
        QByteArray data(size, Qt::Uninitialized);
        for (char &c : data)
            c = char(gen.bounded(256));
        return data;
    }

    static QByteArray makeControl(const QByteArray &target, int blockSize, int seqMatches, int rsumBytes, int checksumBytes)
    {
        QByteArray out = "zsync: 0.6.2\nFilename: t\nBlocksize: " + QByteArray::number(blockSize)
                         + "\nLength: " + QByteArray::number(target.size())
                         + "\nHash-Lengths: " + QByteArray::number(seqMatches) + ',' + QByteArray::number(rsumBytes)
                         + ',' + QByteArray::number(checksumBytes) + "\nURL: t.bin\nSHA-1: "
                         + QCryptographicHash::hash(target, QCryptographicHash::Sha1).toHex() + "\n\n";
        for (qsizetype pos = 0; pos < target.size(); pos += blockSize) {
            QByteArray block = target.mid(pos, blockSize);
            block.append(QByteArray(blockSize - block.size(), '\0'));
            const quint32 r = ZsyncDownloader::blockRsum(reinterpret_cast<const uchar *>(block.constData()), blockSize, 4);
            const QByteArray full = QByteArray(1, char(r >> 24)) + char(r >> 16) + char(r >> 8) + char(r);
            out += full.right(rsumBytes);
            out += QCryptographicHash::hash(block, QCryptographicHash::Md4).left(checksumBytes);
        }
        return out;
    }

    // Builds the target from the local blocks found plus the missing ranges taken from it.
    static QByteArray rebuild(const ZsyncDownloader::Control &control, const QByteArray &local, const QByteArray &target,
                              qint64 *downloaded = nullptr)
    {
        const QVector<qint64> found = ZsyncDownloader::matchBlocks(
            control, reinterpret_cast<const uchar *>(local.constData()), local.size());
        QByteArray out(control.length, '\0');
        for (int i = 0; i < found.size(); ++i) {
            if (found.at(i) < 0)
                continue;
            const qint64 start = i * control.blockSize;
            const qint64 len = qMin(control.blockSize, control.length - start);
            const QByteArray piece = local.mid(found.at(i), len);
            out.replace(start, piece.size(), piece);
        }
        qint64 fetched = 0;
        for (const auto &range : ZsyncDownloader::missingRanges(control, found, 0)) {
            out.replace(range.first, range.second - range.first + 1, target.mid(range.first, range.second - range.first + 1));
            fetched += range.second - range.first + 1;
        }
        if (downloaded)
            *downloaded = fetched;
        return out;
    }

private Q_SLOTS:
    void parse()
    {
        const QByteArray target = randomBytes(10000, 1);
        ZsyncDownloader::Control control;
        QVERIFY(ZsyncDownloader::parseControl(makeControl(target, 1024, 2, 2, 4), control));
        QCOMPARE(control.blockSize, 1024);
        QCOMPARE(control.length, 10000);
        QCOMPARE(control.blockCount(), 10);
        QCOMPARE(control.url, QStringLiteral("t.bin"));
        QCOMPARE(control.seqMatches, 2);
        QVERIFY(!ZsyncDownloader::parseControl("zsync: 1\nBlocksize: 0\n\n", control));
    }

    void rebuild_data()
    {
        QTest::addColumn<int>("seqMatches");
        QTest::addColumn<int>("rsumBytes");
        QTest::addColumn<int>("checksumBytes");
        QTest::newRow("1,4,16") << 1 << 4 << 16;
        QTest::newRow("2,2,5") << 2 << 2 << 5;
        QTest::newRow("2,3,4") << 2 << 3 << 4;
    }

    void rebuild()
    {
        QFETCH(int, seqMatches);
        QFETCH(int, rsumBytes);
        QFETCH(int, checksumBytes);
        const int bs = 512;
        const QByteArray target = randomBytes(200 * bs + 123, 7);
        ZsyncDownloader::Control control;
        QVERIFY(ZsyncDownloader::parseControl(makeControl(target, bs, seqMatches, rsumBytes, checksumBytes), control));

        // The same file: nothing to download.
        qint64 fetched = -1;
        QCOMPARE(rebuild(control, target, target, &fetched), target);
        QCOMPARE(fetched, 0);

        // Bytes inserted at the start (everything shifted), one block changed, the end cut.
        QByteArray local = randomBytes(37, 9) + target;
        local[37 + 50 * bs + 10] = char(local[37 + 50 * bs + 10] ^ 0xff);
        local.chop(3 * bs);
        QCOMPARE(rebuild(control, local, target, &fetched), target);
        QVERIFY2(fetched < 10 * bs, qPrintable(QString::number(fetched))); // a few blocks, not the file

        // Nothing usable locally: everything is downloaded.
        QCOMPARE(rebuild(control, QByteArray(), target, &fetched), target);
        QCOMPARE(fetched, target.size());
    }

    // Against a real server, only when asked: AIM_ZSYNC_URL=<.zsync URL> AIM_ZSYNC_LOCAL=<older file>.
    void live()
    {
        const QString url = qEnvironmentVariable("AIM_ZSYNC_URL");
        const QString local = qEnvironmentVariable("AIM_ZSYNC_LOCAL");
        if (url.isEmpty() || local.isEmpty())
            QSKIP("set AIM_ZSYNC_URL and AIM_ZSYNC_LOCAL to run against a real server");
        QTemporaryDir dir;
        const QString out = dir.filePath(QStringLiteral("out"));
        QNetworkAccessManager nam;
        ZsyncDownloader zsync(&nam, local, QUrl(url), out);
        QSignalSpy finished(&zsync, &ZsyncDownloader::finished);
        zsync.start();
        QVERIFY(finished.wait(300000));
        QCOMPARE(finished.first().first().toBool(), true);
        QFile file(out);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCryptographicHash hash(QCryptographicHash::Sha1);
        hash.addData(&file);
        QCOMPARE(QString::fromLatin1(hash.result().toHex()), zsync.sha1());
    }

    // A local file that shares nothing with the target (a new release that changed
    // everything): with 2-byte weak sums most positions hit the table, so the next block's
    // weak sum must filter before any MD4 is computed, or this takes minutes.
    void unrelatedLocal_isFast()
    {
        const int bs = 2048;
        const QByteArray target = randomBytes(16000 * bs, 3);  // 32 MB, like a real AppImage
        ZsyncDownloader::Control control;
        QVERIFY(ZsyncDownloader::parseControl(makeControl(target, bs, 2, 2, 5), control));
        const QByteArray local = randomBytes(16000 * bs, 4);
        QElapsedTimer timer;
        timer.start();
        const QVector<qint64> found = ZsyncDownloader::matchBlocks(
            control, reinterpret_cast<const uchar *>(local.constData()), local.size());
        qDebug() << "unrelated 32 MB matched in" << timer.elapsed() << "ms";
        QVERIFY(timer.elapsed() < 10000);
        QCOMPARE(found.count(-1), found.size());
    }

    void ranges_bridgeGaps()
    {
        ZsyncDownloader::Control control;
        control.blockSize = 100;
        control.length = 1000;
        control.rsums.resize(10);
        control.checksums.resize(10);
        QVector<qint64> found(10, 0);
        found[1] = -1;
        found[2] = -1;
        found[4] = -1;
        found[9] = -1;
        const auto exact = ZsyncDownloader::missingRanges(control, found, 0);
        QCOMPARE(exact.size(), 3);
        QCOMPARE(exact.at(0), qMakePair(qint64(100), qint64(299)));
        QCOMPARE(exact.at(2), qMakePair(qint64(900), qint64(999)));
        const auto bridged = ZsyncDownloader::missingRanges(control, found, 100);
        QCOMPARE(bridged.size(), 2);
        QCOMPARE(bridged.at(0), qMakePair(qint64(100), qint64(499)));
    }
};

QTEST_GUILESS_MAIN(TstZsync)
#include "tst_zsync.moc"
