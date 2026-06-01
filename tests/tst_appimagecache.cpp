// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors

#include "../src/core/appimagecache.h"
#include "../src/core/appimageinfo.h"

#include <QTest>
#include <QTemporaryDir>
#include <QThread>
#include <QtConcurrent/QtConcurrent>

class TstAppImageCache : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void storeAndLoad_roundTrip()
    {
        AppImageInfo info;
        info.originalName  = QStringLiteral("TestApp-1.0-x86_64.AppImage");
        info.cleanName     = QStringLiteral("Test App.AppImage");
        info.appId         = QStringLiteral("testapp");
        info.appName       = QStringLiteral("Test App");
        info.version       = QStringLiteral("1.0");
        info.categories    = QStringLiteral("Utility;");
        info.comment       = QStringLiteral("A test application");
        info.description   = QStringLiteral("<p>A test description.</p>");
        info.developerName = QStringLiteral("Test Developer");
        info.homepage      = QStringLiteral("https://example.com");
        info.execArgs      = QStringLiteral("--no-sandbox");
        info.fileSize      = 42'000'000;
        info.iconExt       = QStringLiteral(".png");
        info.iconData      = QByteArray("fake-png-bytes");
        info.updateInfo    = QStringLiteral("gh-releases-zsync|owner|repo|latest|*.AppImage.zsync");
        info.isValid       = true;

        const QString fakePath = QStringLiteral("/tmp/fake-TestApp.AppImage");
        const qint64 mtime = 1'700'000'000LL;

        AppImageCache::instance().store(fakePath, mtime, info);

        const AppImageInfo loaded = AppImageCache::instance().load(fakePath, mtime);
        QVERIFY(loaded.isValid);
        QCOMPARE(loaded.originalName,  info.originalName);
        QCOMPARE(loaded.cleanName,     info.cleanName);
        QCOMPARE(loaded.appId,         info.appId);
        QCOMPARE(loaded.appName,       info.appName);
        QCOMPARE(loaded.version,       info.version);
        QCOMPARE(loaded.categories,    info.categories);
        QCOMPARE(loaded.comment,       info.comment);
        QCOMPARE(loaded.description,   info.description);
        QCOMPARE(loaded.developerName, info.developerName);
        QCOMPARE(loaded.homepage,      info.homepage);
        QCOMPARE(loaded.execArgs,      info.execArgs);
        QCOMPARE(loaded.fileSize,      info.fileSize);
        QCOMPARE(loaded.iconExt,       info.iconExt);
        QCOMPARE(loaded.iconData,      info.iconData);
        QCOMPARE(loaded.updateInfo,    info.updateInfo);
    }

    void staleMtime_returnsInvalid()
    {
        AppImageInfo info;
        info.isValid = true;
        info.appName = QStringLiteral("StaleMtimeApp");

        const QString fakePath = QStringLiteral("/tmp/fake-stale.AppImage");
        const qint64 storedMtime = 1'000'000'000LL;
        AppImageCache::instance().store(fakePath, storedMtime, info);

        // Different mtime → cache miss
        const AppImageInfo loaded = AppImageCache::instance().load(fakePath, storedMtime + 1);
        QVERIFY(!loaded.isValid);
    }

    void unknownPath_returnsInvalid()
    {
        const AppImageInfo loaded = AppImageCache::instance().load(
            QStringLiteral("/nonexistent/path/app.AppImage"), 12345LL);
        QVERIFY(!loaded.isValid);
    }

    void concurrentStoreLoad_noDeadlock()
    {
        // Hammer the cache from multiple threads simultaneously.
        // Verifies the per-thread connection + mutex pattern is deadlock-free.
        AppImageInfo info;
        info.isValid = true;
        info.appName = QStringLiteral("ConcurrentApp");
        const QString path = QStringLiteral("/tmp/fake-concurrent.AppImage");
        const qint64 mtime = 555'000LL;

        AppImageCache::instance().store(path, mtime, info);

        const int N = 10;
        QList<QFuture<AppImageInfo>> futures;
        futures.reserve(N);
        for (int i = 0; i < N; ++i) {
            futures.append(QtConcurrent::run([path, mtime]() {
                return AppImageCache::instance().load(path, mtime);
            }));
        }
        for (auto &f : futures) {
            f.waitForFinished();
            QVERIFY(f.result().isValid);
        }
    }

    void iconData_preservedExact()
    {
        const QByteArray icon = QByteArray::fromHex("89504e470d0a1a0a"); // PNG magic bytes
        AppImageInfo info;
        info.isValid  = true;
        info.iconData = icon;
        info.iconExt  = QStringLiteral(".png");

        const QString path = QStringLiteral("/tmp/fake-icon.AppImage");
        const qint64 mtime = 9'999'999LL;
        AppImageCache::instance().store(path, mtime, info);

        const AppImageInfo loaded = AppImageCache::instance().load(path, mtime);
        QCOMPARE(loaded.iconData, icon);
    }
};

QTEST_GUILESS_MAIN(TstAppImageCache)
#include "tst_appimagecache.moc"
