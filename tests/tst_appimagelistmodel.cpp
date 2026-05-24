// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors

#include "../src/core/appsettings.h"
#include "../src/gui/appimageiconprovider.h"
#include "../src/gui/appimagelistmodel.h"
#include "../src/gui/appimagesortfiltermodel.h"

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

namespace {
// Drop a stub *.AppImage of the requested size into `dir`.
QString writeStub(const QString &dir, const QString &name, qint64 bytes)
{
    const QString path = dir + QLatin1Char('/') + name;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        qFatal("writeStub: cannot create %s", qPrintable(path));
    if (bytes > 0)
        f.resize(bytes);
    f.close();
    return path;
}
}

class TstAppImageListModel : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_tmp;
    AppImageIconProvider *m_provider = nullptr;
    AppImageListModel *m_model = nullptr;
    AppImageSortFilterModel *m_proxy = nullptr;

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(m_tmp.isValid());
    }

    void init()
    {
        // Fresh tempdir per test so writes from earlier tests don't leak.
        QDir(m_tmp.path()).removeRecursively();
        QDir().mkpath(m_tmp.path());

        AppSettings::instance()->setApplicationsPath(m_tmp.path());

        m_provider = new AppImageIconProvider;
        m_model = new AppImageListModel(m_provider);
        m_proxy = new AppImageSortFilterModel;
        m_proxy->setSourceModel(m_model);
    }

    void cleanup()
    {
        delete m_proxy;
        delete m_model;
        delete m_provider;
        m_proxy = nullptr;
        m_model = nullptr;
        m_provider = nullptr;
    }

    // ── Scan + scanning state ────────────────────────────────────────────────

    void scan_emitsInsertsAndSettlesScanning()
    {
        writeStub(m_tmp.path(), QStringLiteral("Alpha-1.0.AppImage"), 100);
        writeStub(m_tmp.path(), QStringLiteral("Beta-2.0.AppImage"), 200);
        writeStub(m_tmp.path(), QStringLiteral("Gamma-3.0.AppImage"), 50);

        QSignalSpy insertSpy(m_model, &AppImageListModel::rowsInserted);
        QSignalSpy scanningSpy(m_model, &AppImageListModel::scanningChanged);

        m_model->scan();

        QCOMPARE(m_model->rowCount(), 3);
        QVERIFY(insertSpy.count() >= 1);

        // Reader on zero-byte stubs returns invalid AppImageInfo; metadataLoaded
        // still flips to true via the worker path. Wait for scanning → false.
        QTRY_VERIFY_WITH_TIMEOUT(!m_model->isScanning(), 5000);
        QVERIFY(scanningSpy.count() >= 1);
    }

    void disappearedFile_emitsRowsRemoved()
    {
        const QString gone = writeStub(m_tmp.path(), QStringLiteral("ToRemove.AppImage"), 100);
        m_model->scan();
        QTRY_VERIFY_WITH_TIMEOUT(!m_model->isScanning(), 5000);
        QCOMPARE(m_model->rowCount(), 1);

        QSignalSpy removeSpy(m_model, &AppImageListModel::rowsRemoved);
        QVERIFY(QFile::remove(gone));
        m_model->refresh();
        QCOMPARE(m_model->rowCount(), 0);
        QCOMPARE(removeSpy.count(), 1);
    }

    // ── Sort filter ──────────────────────────────────────────────────────────

    void filter_caseInsensitiveMatch()
    {
        writeStub(m_tmp.path(), QStringLiteral("KritaApp.AppImage"), 10);
        writeStub(m_tmp.path(), QStringLiteral("ProtonUp.AppImage"), 10);
        m_model->scan();
        QTRY_VERIFY_WITH_TIMEOUT(!m_model->isScanning(), 5000);
        QCOMPARE(m_proxy->rowCount(), 2);

        m_proxy->setFilterText(QStringLiteral("KRITA"));
        QCOMPARE(m_proxy->rowCount(), 1);
        QCOMPARE(m_proxy->data(m_proxy->index(0, 0), AppImageListModel::FilePathRole)
                     .toString().endsWith(QStringLiteral("KritaApp.AppImage")), true);

        m_proxy->setFilterText(QString());
        QCOMPARE(m_proxy->rowCount(), 2);
    }

    void sort_byName_alphabetical()
    {
        writeStub(m_tmp.path(), QStringLiteral("Charlie.AppImage"), 10);
        writeStub(m_tmp.path(), QStringLiteral("Alpha.AppImage"),   10);
        writeStub(m_tmp.path(), QStringLiteral("Bravo.AppImage"),   10);
        m_model->scan();
        QTRY_VERIFY_WITH_TIMEOUT(!m_model->isScanning(), 5000);

        m_proxy->setSortField(AppImageSortFilterModel::SortByName);
        QCOMPARE(m_proxy->rowCount(), 3);
        const auto name = [&](int r) {
            return m_proxy->data(m_proxy->index(r, 0), AppImageListModel::CleanNameRole).toString();
        };
        QVERIFY(name(0) < name(1));
        QVERIFY(name(1) < name(2));
    }

    void sort_bySize_ascending()
    {
        writeStub(m_tmp.path(), QStringLiteral("Small.AppImage"),  50);
        writeStub(m_tmp.path(), QStringLiteral("Medium.AppImage"), 150);
        writeStub(m_tmp.path(), QStringLiteral("Large.AppImage"),  300);
        m_model->scan();
        QTRY_VERIFY_WITH_TIMEOUT(!m_model->isScanning(), 5000);

        m_proxy->setSortField(AppImageSortFilterModel::SortBySize);
        const auto size = [&](int r) {
            return m_proxy->data(m_proxy->index(r, 0), AppImageListModel::AppSizeRole).toLongLong();
        };
        QCOMPARE(size(0), 50LL);
        QCOMPARE(size(1), 150LL);
        QCOMPARE(size(2), 300LL);
    }

    void sort_tieBreakByName()
    {
        // Two equally-sized files; size sort must tie-break alphabetically.
        writeStub(m_tmp.path(), QStringLiteral("Zeta.AppImage"),  100);
        writeStub(m_tmp.path(), QStringLiteral("Alpha.AppImage"), 100);
        m_model->scan();
        QTRY_VERIFY_WITH_TIMEOUT(!m_model->isScanning(), 5000);

        m_proxy->setSortField(AppImageSortFilterModel::SortBySize);
        const auto name = [&](int r) {
            return m_proxy->data(m_proxy->index(r, 0), AppImageListModel::CleanNameRole).toString();
        };
        QVERIFY(name(0).startsWith(QStringLiteral("Alpha")));
        QVERIFY(name(1).startsWith(QStringLiteral("Zeta")));
    }

    // ── itemData bulk fetch ──────────────────────────────────────────────────

    void itemData_returnsAllRoles()
    {
        writeStub(m_tmp.path(), QStringLiteral("Solo.AppImage"), 42);
        m_model->scan();
        QTRY_VERIFY_WITH_TIMEOUT(!m_model->isScanning(), 5000);

        const QVariantMap data = m_proxy->itemData(0);
        QVERIFY(!data.isEmpty());
        QVERIFY(data.contains(QStringLiteral("filePath")));
        QVERIFY(data.contains(QStringLiteral("displayName")));
        QVERIFY(data.contains(QStringLiteral("appSize")));
        QCOMPARE(data.value(QStringLiteral("appSize")).toLongLong(), 42LL);
    }
};

QTEST_MAIN(TstAppImageListModel)
#include "tst_appimagelistmodel.moc"
