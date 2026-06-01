// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors

#include "../src/dbus/appimagedbusadaptor.h"
#include "../src/gui/appimageiconprovider.h"
#include "../src/gui/appimagelistmodel.h"
#include "version.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QStandardPaths>
#include <QTest>

static constexpr auto kService    = "io.github.appimagemanager.TestAdaptor";
static constexpr auto kObjectPath = "/io/github/appimagemanager/Manager";
static constexpr auto kInterface  = "io.github.appimagemanager.Manager1";

class TstAppImageDBusAdaptor : public QObject
{
    Q_OBJECT

private:
    AppImageIconProvider *m_provider = nullptr;
    AppImageListModel    *m_model    = nullptr;

private Q_SLOTS:
    void initTestCase()
    {
        if (!QDBusConnection::sessionBus().isConnected())
            QSKIP("No D-Bus session bus available");

        QStandardPaths::setTestModeEnabled(true);

        m_provider = new AppImageIconProvider;
        m_model    = new AppImageListModel(m_provider, this);
        m_provider->setParent(m_model);

        new AppImageDBusAdaptor(m_model);

        QVERIFY2(QDBusConnection::sessionBus().registerObject(
                     QString::fromLatin1(kObjectPath), m_model),
                 "Failed to register D-Bus object");
        QVERIFY2(QDBusConnection::sessionBus().registerService(
                     QString::fromLatin1(kService)),
                 "Failed to register D-Bus service");
    }

    void cleanupTestCase()
    {
        QDBusConnection::sessionBus().unregisterService(QString::fromLatin1(kService));
        QDBusConnection::sessionBus().unregisterObject(QString::fromLatin1(kObjectPath));
    }

    void testVersionMatchesBuildVersion()
    {
        QDBusInterface iface(QString::fromLatin1(kService),
                             QString::fromLatin1(kObjectPath),
                             QString::fromLatin1(kInterface),
                             QDBusConnection::sessionBus());
        QVERIFY2(iface.isValid(), qPrintable(iface.lastError().message()));

        const QDBusReply<QString> reply = iface.call(QStringLiteral("Version"));
        QVERIFY2(reply.isValid(), qPrintable(reply.error().message()));
        QCOMPARE(reply.value(), QStringLiteral(APPIMAGEMANAGER_VERSION_STRING));
    }

    void testInstalledAppImagesReturnsVariantList()
    {
        QDBusInterface iface(QString::fromLatin1(kService),
                             QString::fromLatin1(kObjectPath),
                             QString::fromLatin1(kInterface),
                             QDBusConnection::sessionBus());
        QVERIFY2(iface.isValid(), qPrintable(iface.lastError().message()));

        const QDBusReply<QVariantList> reply = iface.call(QStringLiteral("InstalledAppImages"));
        QVERIFY2(reply.isValid(), qPrintable(reply.error().message()));
        // Empty model → empty list; type must still be a QVariantList
        QVERIFY(reply.value().isEmpty());
    }
};

QTEST_MAIN(TstAppImageDBusAdaptor)
#include "tst_appimagedbusadaptor.moc"
