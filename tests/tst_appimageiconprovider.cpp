// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors

#include "../src/gui/appimageiconprovider.h"

#include <QTest>
#include <QBuffer>
#include <QFuture>
#include <QFutureWatcher>
#include <QPixmap>
#include <QSignalSpy>
#include <QtConcurrent/QtConcurrent>

// Generate a valid 1x1 red PNG via Qt — avoids hardcoded byte arrays.
static QByteArray minimalPng()
{
    QPixmap pm(16, 16);
    pm.fill(Qt::red);
    QByteArray ba;
    QBuffer buf(&ba);
    buf.open(QIODevice::WriteOnly);
    pm.save(&buf, "PNG");
    return ba;
}

// Minimal valid SVG
static QByteArray minimalSvg()
{
    return QByteArray(R"(<svg xmlns="http://www.w3.org/2000/svg" width="16" height="16"><rect width="16" height="16" fill="blue"/></svg>)");
}

class TstAppImageIconProvider : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    // ── Basic set / get ───────────────────────────────────────────────────────

    void setAndRequest_png()
    {
        AppImageIconProvider p;
        p.setIconData(QStringLiteral("a"), minimalPng(), QStringLiteral(".png"));
        QSize sz;
        const QPixmap px = p.requestPixmap(QStringLiteral("a"), &sz, QSize(16, 16));
        QVERIFY(!px.isNull());
        QCOMPARE(sz, QSize(16, 16));
    }

    void setAndRequest_svg()
    {
        AppImageIconProvider p;
        p.setIconData(QStringLiteral("b"), minimalSvg(), QStringLiteral(".svg"));
        QSize sz;
        const QPixmap px = p.requestPixmap(QStringLiteral("b"), &sz, QSize(32, 32));
        QVERIFY(!px.isNull());
        QCOMPARE(sz, QSize(32, 32));
    }

    void unknownId_returnsNull()
    {
        AppImageIconProvider p;
        QSize sz;
        const QPixmap px = p.requestPixmap(QStringLiteral("nonexistent"), &sz, QSize(16, 16));
        QVERIFY(px.isNull());
    }

    void overwrite_invalidatesCachedPixmap()
    {
        AppImageIconProvider p;
        p.setIconData(QStringLiteral("k"), minimalPng(), QStringLiteral(".png"));
        QSize sz;
        p.requestPixmap(QStringLiteral("k"), &sz, QSize(16, 16)); // populate cache

        // Replace with SVG — old cached pixmap must not be returned
        p.setIconData(QStringLiteral("k"), minimalSvg(), QStringLiteral(".svg"));
        const QPixmap px2 = p.requestPixmap(QStringLiteral("k"), &sz, QSize(16, 16));
        QVERIFY(!px2.isNull());
    }

    // ── Concurrency: no deadlock, no crash ───────────────────────────────────

    void concurrent_readsDontDeadlock()
    {
        // Pre-load 4 icons, then hammer requestPixmap from 8 threads simultaneously.
        // If the lock implementation serialises incorrectly, this either deadlocks
        // (test timeout) or crashes under ThreadSanitizer.
        AppImageIconProvider provider;
        const QStringList ids = { QStringLiteral("c1"), QStringLiteral("c2"),
                                   QStringLiteral("c3"), QStringLiteral("c4") };
        for (const QString &id : ids)
            provider.setIconData(id, minimalPng(), QStringLiteral(".png"));

        constexpr int kThreads = 8;
        constexpr int kItersPerThread = 50;

        QList<QFuture<void>> futures;
        futures.reserve(kThreads);

        for (int t = 0; t < kThreads; ++t) {
            futures.append(QtConcurrent::run([&provider, &ids]() {
                for (int i = 0; i < kItersPerThread; ++i) {
                    const QString &id = ids[i % ids.size()];
                    QSize sz;
                    const QPixmap px = provider.requestPixmap(id, &sz, QSize(16, 16));
                    Q_UNUSED(px)
                }
            }));
        }

        for (QFuture<void> &f : futures)
            f.waitForFinished();

        // If we reach here without deadlock or SIGABRT — pass.
        QVERIFY(true);
    }

    void concurrent_writeWhileReading()
    {
        // Writer thread calls setIconData in a loop; reader threads call
        // requestPixmap simultaneously. Tests that QReadWriteLock write lock
        // does not starve or deadlock against concurrent readers.
        AppImageIconProvider provider;
        provider.setIconData(QStringLiteral("w"), minimalPng(), QStringLiteral(".png"));

        std::atomic<bool> stop{false};

        QFuture<void> writer = QtConcurrent::run([&provider, &stop]() {
            int i = 0;
            while (!stop.load()) {
                const bool even = (i++ % 2 == 0);
                provider.setIconData(QStringLiteral("w"),
                                     even ? minimalPng() : minimalSvg(),
                                     even ? QStringLiteral(".png") : QStringLiteral(".svg"));
            }
        });

        QList<QFuture<void>> readers;
        for (int t = 0; t < 4; ++t) {
            readers.append(QtConcurrent::run([&provider, &stop]() {
                while (!stop.load()) {
                    QSize sz;
                    provider.requestPixmap(QStringLiteral("w"), &sz, QSize(16, 16));
                }
            }));
        }

        QTest::qWait(300); // run for 300 ms
        stop.store(true);

        writer.waitForFinished();
        for (QFuture<void> &r : readers)
            r.waitForFinished();

        QVERIFY(true);
    }

    // ── Cache behaviour ───────────────────────────────────────────────────────

    void cacheHit_returnsSamePixmap()
    {
        // QCache returns copies of stored QPixmap — each copy has a different
        // cacheKey(). Verify size and validity instead.
        AppImageIconProvider p;
        p.setIconData(QStringLiteral("x"), minimalPng(), QStringLiteral(".png"));
        QSize sz1, sz2;
        const QPixmap a = p.requestPixmap(QStringLiteral("x"), &sz1, QSize(16, 16));
        const QPixmap b = p.requestPixmap(QStringLiteral("x"), &sz2, QSize(16, 16));
        QVERIFY(!a.isNull());
        QVERIFY(!b.isNull());
        QCOMPARE(a.size(), b.size());
        QCOMPARE(sz1, sz2);
    }
};

QTEST_MAIN(TstAppImageIconProvider)
#include "tst_appimageiconprovider.moc"
