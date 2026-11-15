// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors

// App menu entries: one entry per AppImage even when the app (or a vendor launcher)
// writes its own, named after the app's desktop id when it has a reverse-DNS one.

#include "../src/core/appimagemanager.h"

#include <KConfigGroup>
#include <KDesktopFile>

#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTest>
#include <QThread>

class TstDesktopLink : public QObject
{
    Q_OBJECT

    QString m_apps;
    QString m_appImage;

    AppImageInfo info(const QString &appId) const
    {
        AppImageInfo i;
        i.appId = appId;
        i.appName = QStringLiteral("FigCraft");
        i.cleanName = QStringLiteral("figcraft 0.18.1 linux");
        i.originalName = QStringLiteral("figcraft-0.18.1-linux-x86_64.AppImage");
        i.execArgs = QStringLiteral("%U");
        return i;
    }

    QString writeEntry(const QString &fileName, const QString &exec, const QString &extra = {})
    {
        // The entry index notices changes by the directory's mtime; make sure it moves.
        QThread::msleep(15);
        const QString path = m_apps + QLatin1Char('/') + fileName;
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly))
            return {};
        f.write("[Desktop Entry]\nType=Application\nName=FigCraft\nExec=" + exec.toUtf8() + "\n" + extra.toUtf8());
        return path;
    }

    QStringList entries() const
    {
        return QDir(m_apps).entryList({ QStringLiteral("*.desktop") }, QDir::Files, QDir::Name);
    }

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        m_apps = QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation);
        m_appImage = QStandardPaths::writableLocation(QStandardPaths::HomeLocation)
                     + QStringLiteral("/Applications/figcraft-0.18.1-linux-x86_64.AppImage");
    }

    void cleanupTestCase()
    {
        QDir(m_apps).removeRecursively();
        QDir(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/Trash")).removeRecursively();
    }

    void init()
    {
        QDir(m_apps).removeRecursively();
        QDir().mkpath(m_apps);
    }

    void ownIdNaming()
    {
        QVERIFY(AppImageManager::desktopFilePath(info(QStringLiteral("ai.storyteller.figcraft")))
                    .endsWith(QLatin1String("/ai.storyteller.figcraft.desktop")));
        // No reverse-DNS id: the appimage_<slug>_<hash> name.
        QVERIFY(QFileInfo(AppImageManager::desktopFilePath(info(QStringLiteral("figcraft"))))
                    .fileName().startsWith(QLatin1String("appimage_figcraft_0.18.1_linux_")));
    }

    void foreignEntryCounts()
    {
        const AppImageInfo i = info(QStringLiteral("figcraft"));
        QVERIFY(!AppImageManager::isDesktopLinkEnabled(m_appImage, i));
        // Quoted path and %U, as apps write it; hidden entries don't count.
        writeEntry(QStringLiteral("hidden.desktop"), QLatin1Char('"') + m_appImage + QStringLiteral("\" %U"), QStringLiteral("NoDisplay=true\n"));
        QVERIFY(!AppImageManager::isDesktopLinkEnabled(m_appImage, i));
        writeEntry(QStringLiteral("vendor.desktop"), QStringLiteral("env FOO=1 \"") + m_appImage + QStringLiteral("\" %U"));
        QVERIFY(AppImageManager::isDesktopLinkEnabled(m_appImage, i));
    }

    void duplicateCleanup()
    {
        // The situation from the bug: our old entry plus the one the app wrote itself.
        const AppImageInfo i = info(QStringLiteral("ai.storyteller.figcraft"));
        // Our pre-2.1.1 name is what desktopFilePath() gives for an id that isn't reverse-DNS.
        const QString ours = QFileInfo(AppImageManager::desktopFilePath(info(QStringLiteral("figcraft")))).fileName();
        writeEntry(ours, m_appImage + QStringLiteral(" %U"));
        writeEntry(QStringLiteral("ai.storyteller.figcraft.desktop"), QLatin1Char('"') + m_appImage + QStringLiteral("\" %U"));
        writeEntry(QStringLiteral("unrelated.desktop"), QStringLiteral("/usr/bin/true"));

        QVERIFY(AppImageManager::removeDuplicateDesktopLinks(m_appImage, i));
        QCOMPARE(entries(), QStringList({ QStringLiteral("ai.storyteller.figcraft.desktop"), QStringLiteral("unrelated.desktop") }));
        QVERIFY(AppImageManager::isDesktopLinkEnabled(m_appImage, i));
        // Nothing left to clean.
        QVERIFY(!AppImageManager::removeDuplicateDesktopLinks(m_appImage, i));
    }

    void onlyOursIsKept()
    {
        const AppImageInfo i = info(QStringLiteral("figcraft"));
        writeEntry(QFileInfo(AppImageManager::desktopFilePath(i)).fileName(), m_appImage);
        QVERIFY(!AppImageManager::removeDuplicateDesktopLinks(m_appImage, i));
        QCOMPARE(entries().size(), 1);
    }

    void createKeepsAppKeys()
    {
        const AppImageInfo i = info(QStringLiteral("ai.storyteller.figcraft"));
        writeEntry(QStringLiteral("ai.storyteller.figcraft.desktop"), QStringLiteral("\"/old/figcraft.AppImage\" %U"),
                   QStringLiteral("Comment=Design tool\nMimeType=image/svg+xml;\nStartupWMClass=ai.storyteller.figcraft\n"));
        QVERIFY(AppImageManager::createDesktopLink(m_appImage, i));
        QCOMPARE(entries(), QStringList({ QStringLiteral("ai.storyteller.figcraft.desktop") }));
        KDesktopFile df(m_apps + QStringLiteral("/ai.storyteller.figcraft.desktop"));
        const KConfigGroup grp = df.desktopGroup();
        QVERIFY(grp.readEntry("Exec", QString()).contains(m_appImage));
        QCOMPARE(grp.readEntry("Comment", QString()), QStringLiteral("Design tool"));
        QCOMPARE(grp.readEntry("StartupWMClass", QString()), QStringLiteral("ai.storyteller.figcraft"));
        QVERIFY(!grp.readEntry("MimeType", QString()).isEmpty());
    }

    void removeTakesEveryEntry()
    {
        const AppImageInfo i = info(QStringLiteral("figcraft"));
        writeEntry(QStringLiteral("vendor.desktop"), QLatin1Char('"') + m_appImage + QLatin1Char('"'));
        QVERIFY(AppImageManager::isDesktopLinkEnabled(m_appImage, i));
        QVERIFY(AppImageManager::removeDesktopLink(m_appImage, i));
        QVERIFY(!AppImageManager::isDesktopLinkEnabled(m_appImage, i));
        QVERIFY(entries().isEmpty());
    }
};

QTEST_GUILESS_MAIN(TstDesktopLink)
#include "tst_desktoplink.moc"
