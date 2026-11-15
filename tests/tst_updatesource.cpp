// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors

#include "../src/core/releasechecker.h"
#include "../src/core/launchenvironment.h"
#include "../src/core/updatesource.h"

#include <KConfigGroup>
#include <KSharedConfig>

#include <QJsonObject>
#include <QStandardPaths>
#include <QTest>

static QJsonArray assets(std::initializer_list<const char *> names)
{
    QJsonArray array;
    for (const char *raw : names) {
        const QString name = QString::fromLatin1(raw);
        array.append(QJsonObject{ { QStringLiteral("name"), name },
                                  { QStringLiteral("browser_download_url"), QString(QStringLiteral("https://x/") + name) } });
    }
    return array;
}

class TstUpdateSource : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true); // keep the user's appimagemanagerrc untouched
    }

    void parseGitHubRepo_data()
    {
        QTest::addColumn<QString>("input");
        QTest::addColumn<QString>("expected");
        QTest::newRow("plain") << QStringLiteral("owner/repo") << QStringLiteral("owner/repo");
        QTest::newRow("spaces") << QStringLiteral("  owner/repo ") << QStringLiteral("owner/repo");
        QTest::newRow("url") << QStringLiteral("https://github.com/owner/repo") << QStringLiteral("owner/repo");
        QTest::newRow("url-releases") << QStringLiteral("https://github.com/owner/repo/releases/latest") << QStringLiteral("owner/repo");
        QTest::newRow("url-git") << QStringLiteral("https://github.com/owner/repo.git") << QStringLiteral("owner/repo");
        QTest::newRow("other-host") << QStringLiteral("https://gitlab.com/owner/repo") << QStringLiteral("");
        QTest::newRow("one-part") << QStringLiteral("owner") << QStringLiteral("");
        QTest::newRow("three-parts") << QStringLiteral("a/b/c") << QStringLiteral("");
        QTest::newRow("bad-chars") << QStringLiteral("own er/repo") << QStringLiteral("");
        QTest::newRow("empty") << QStringLiteral("") << QStringLiteral("");
    }

    void parseGitHubRepo()
    {
        QFETCH(QString, input);
        QFETCH(QString, expected);
        QCOMPARE(::parseGitHubRepo(input), expected);
    }

    void effectiveInfo()
    {
        const QString embedded = QStringLiteral("zsync|https://x/a.zsync");
        QCOMPARE(effectiveUpdateInfo(embedded, {}), embedded);
        auto source = [](UpdateSourceType type, const char *value) {
            UpdateSource s;
            s.type = type;
            s.value = QString::fromLatin1(value);
            return s;
        };
        QCOMPARE(effectiveUpdateInfo(embedded, source(UpdateSourceType::GitHub, "o/r")),
                 QStringLiteral("gh-releases-zsync|o|r|latest|"));
        QCOMPARE(effectiveUpdateInfo(embedded, source(UpdateSourceType::Forgejo, "codeberg.org/o/r")),
                 QStringLiteral("forgejo-releases|codeberg.org|o|r"));
        QCOMPARE(effectiveUpdateInfo(embedded, source(UpdateSourceType::Zsync, "https://h/a.zsync")),
                 QStringLiteral("zsync|https://h/a.zsync"));
        QCOMPARE(effectiveUpdateInfo(embedded, source(UpdateSourceType::Url, "https://h/a.AppImage")),
                 QStringLiteral("direct|https://h/a.AppImage"));
    }

    void parseInput_data()
    {
        QTest::addColumn<QString>("input");
        QTest::addColumn<QString>("forced");
        QTest::addColumn<QString>("type");
        QTest::addColumn<QString>("value");
        QTest::newRow("bare-github") << "owner/repo" << "" << "github" << "owner/repo";
        QTest::newRow("github-url") << "https://github.com/o/r/releases" << "" << "github" << "o/r";
        QTest::newRow("codeberg") << "https://codeberg.org/o/r" << "" << "forgejo" << "codeberg.org/o/r";
        QTest::newRow("forgejo-git") << "https://git.example.com/o/r.git" << "" << "forgejo" << "git.example.com/o/r";
        QTest::newRow("zsync") << "https://h/x/App.AppImage.zsync" << "" << "zsync" << "https://h/x/App.AppImage.zsync";
        QTest::newRow("appimage-url") << "https://h/dl/App-x86_64.AppImage" << "" << "url" << "https://h/dl/App-x86_64.AppImage";
        QTest::newRow("query-url") << "https://h/download?os=linux" << "" << "url" << "https://h/download?os=linux";
        QTest::newRow("forced-url") << "https://h/o/r" << "url" << "url" << "https://h/o/r";
        QTest::newRow("forced-forgejo-bare") << "o/r" << "forgejo" << "" << "";
        QTest::newRow("ftp") << "ftp://h/a.AppImage" << "" << "" << "";
        QTest::newRow("junk") << "foo" << "" << "" << "";
        QTest::newRow("empty") << "" << "" << "" << "";
    }

    void parseInput()
    {
        QFETCH(QString, input);
        QFETCH(QString, forced);
        QFETCH(QString, type);
        QFETCH(QString, value);
        const UpdateSource s = parseUpdateSourceInput(input, updateSourceTypeFromName(forced));
        QCOMPARE(updateSourceTypeName(s.type), type);
        QCOMPARE(s.value, value);
    }

    void describe()
    {
        QCOMPARE(describeUpdateInfo(QStringLiteral("gh-releases-zsync|o|r|latest|")), QStringLiteral("GitHub · o/r"));
        QCOMPARE(describeUpdateInfo(QStringLiteral("forgejo-releases|codeberg.org|o|r")), QStringLiteral("Codeberg · o/r"));
        QCOMPARE(describeUpdateInfo(QStringLiteral("direct|https://dl.example.com/a.AppImage")),
                 QStringLiteral("Direct download · dl.example.com"));
        QVERIFY(describeUpdateInfo(QString()).isEmpty());
        QVERIFY(isReleaseUpdateInfo(QStringLiteral("direct|https://h/a")));
        QVERIFY(!isReleaseUpdateInfo(QStringLiteral("zsync|https://h/a.zsync")));
        QVERIFY(isSupportedUpdateInfo(QStringLiteral("zsync|https://h/a.zsync")));
    }

    void configRoundTrip()
    {
        const QString path = QStringLiteral("/apps/Foo-1.0-x86_64.AppImage");
        UpdateSource source = parseUpdateSourceInput(QStringLiteral("o/r"));
        source.prerelease = true;
        saveUpdateSource(path, source);
        saveInstalledTag(path, QStringLiteral("v1.1"));
        UpdateSource s = loadUpdateSource(QStringLiteral("/elsewhere/Foo-1.0-x86_64.AppImage"));
        QCOMPARE(s.type, UpdateSourceType::GitHub);
        QCOMPARE(s.value, QStringLiteral("o/r"));
        QVERIFY(s.prerelease);
        QCOMPARE(s.installedTag, QStringLiteral("v1.1"));

        // Same source keeps the tag; a new one forgets it; None clears.
        source.prerelease = false;
        saveUpdateSource(path, source);
        QCOMPARE(loadUpdateSource(path).installedTag, QStringLiteral("v1.1"));
        saveUpdateSource(path, parseUpdateSourceInput(QStringLiteral("https://codeberg.org/o/r")));
        QVERIFY(loadUpdateSource(path).installedTag.isEmpty());
        QCOMPARE(loadUpdateSource(path).type, UpdateSourceType::Forgejo);
        saveUpdateSource(path, UpdateSource{});
        QCOMPARE(loadUpdateSource(path).type, UpdateSourceType::None);
    }

    void legacyRepoKey()
    {
        // Sources saved by the first version only had a Repo key.
        KConfigGroup group = KSharedConfig::openConfig(QStringLiteral("appimagemanagerrc"))
                                 ->group(QStringLiteral("UpdateSources")).group(QStringLiteral("Old.AppImage"));
        group.writeEntry("Repo", QStringLiteral("o/old"));
        group.sync();
        const UpdateSource s = loadUpdateSource(QStringLiteral("/x/Old.AppImage"));
        QCOMPARE(s.type, UpdateSourceType::GitHub);
        QCOMPARE(s.value, QStringLiteral("o/old"));
    }

    void environment()
    {
        const EnvironmentParse ok = parseEnvironment(QStringLiteral(
            "WEBKIT_DISABLE_DMABUF_RENDERER=1\n\n# comment\nQT_SCALE_FACTOR=1.25\nEMPTY="));
        QVERIFY(ok.invalidLines.isEmpty());
        QCOMPARE(ok.variables, QStringList({ QStringLiteral("WEBKIT_DISABLE_DMABUF_RENDERER=1"),
                                             QStringLiteral("QT_SCALE_FACTOR=1.25"), QStringLiteral("EMPTY=") }));

        const EnvironmentParse bad = parseEnvironment(QStringLiteral("A=1\nnot a var\n1A=2\nA=3\n=x"));
        QCOMPARE(bad.invalidLines, QList<int>({ 2, 3, 4, 5 }));

        const QString path = QStringLiteral("/apps/Env.AppImage");
        saveEnvironment(path, ok.variables);
        QCOMPARE(loadEnvironment(QStringLiteral("/other/Env.AppImage")), ok.variables);
        QCOMPARE(environmentExecPrefix({ QStringLiteral("A=1"), QStringLiteral("B=two words") }),
                 QStringLiteral("env A=1 'B=two words' "));
        QVERIFY(environmentExecPrefix({}).isEmpty());
        saveEnvironment(path, {});
        QVERIFY(loadEnvironment(path).isEmpty());
    }

    void contentType()
    {
        QVERIFY(ReleaseChecker::isNonBinaryContentType(QStringLiteral("text/html; charset=utf-8")));
        QVERIFY(ReleaseChecker::isNonBinaryContentType(QStringLiteral("application/json")));
        QVERIFY(!ReleaseChecker::isNonBinaryContentType(QStringLiteral("application/octet-stream")));
        QVERIFY(!ReleaseChecker::isNonBinaryContentType(QStringLiteral("application/vnd.appimage")));
        QVERIFY(!ReleaseChecker::isNonBinaryContentType(QString()));
    }

    void architecture()
    {
        const QString x64 = QStringLiteral("x86_64");
        const QString arm = QStringLiteral("arm64");
        QVERIFY(ReleaseChecker::matchesArchitecture(QStringLiteral("Foo-x86_64.AppImage"), x64));
        QVERIFY(ReleaseChecker::matchesArchitecture(QStringLiteral("Foo-amd64.AppImage"), x64));
        QVERIFY(ReleaseChecker::matchesArchitecture(QStringLiteral("Foo.AppImage"), x64));
        QVERIFY(!ReleaseChecker::matchesArchitecture(QStringLiteral("Foo-aarch64.AppImage"), x64));
        QVERIFY(ReleaseChecker::matchesArchitecture(QStringLiteral("Foo-aarch64.AppImage"), arm));
        QVERIFY(!ReleaseChecker::matchesArchitecture(QStringLiteral("Foo-x86_64.AppImage"), arm));
        // Aliases count only as whole tokens.
        QVERIFY(ReleaseChecker::matchesArchitecture(QStringLiteral("Swarm-1.0.AppImage"), x64));
        QVERIFY(ReleaseChecker::matchesArchitecture(QStringLiteral("Foo_x86_64.AppImage"), x64));
        QVERIFY(!ReleaseChecker::matchesArchitecture(QStringLiteral("Foo-arm64.AppImage"), x64));
        QVERIFY(!ReleaseChecker::matchesArchitecture(QStringLiteral("Foo-x86_64.AppImage"), QStringLiteral("i686")));
    }

    void pickAsset()
    {
        const QString x64 = QStringLiteral("x86_64");
        const QString local = QStringLiteral("Foo-1.0-x86_64.AppImage");

        // .zsync wins over .AppImage; other architectures are skipped.
        QCOMPARE(ReleaseChecker::pickAsset(
                     assets({ "Foo-2.0-x86_64.AppImage", "Foo-2.0-x86_64.AppImage.zsync", "Foo-2.0-aarch64.AppImage" }),
                     {}, local, x64),
                 QStringLiteral("https://x/Foo-2.0-x86_64.AppImage.zsync"));

        // No .zsync: the single fitting AppImage.
        QCOMPARE(ReleaseChecker::pickAsset(
                     assets({ "Foo-2.0-x86_64.AppImage", "Foo-2.0-aarch64.AppImage", "Foo-2.0.tar.gz" }),
                     {}, local, x64),
                 QStringLiteral("https://x/Foo-2.0-x86_64.AppImage"));

        // Several fitting builds: the one named like the installed file.
        QCOMPARE(ReleaseChecker::pickAsset(
                     assets({ "Foo-Qt5-2.0-x86_64.AppImage", "Foo-2.0-x86_64.AppImage" }), {}, local, x64),
                 QStringLiteral("https://x/Foo-2.0-x86_64.AppImage"));

        // Several fitting builds and none named like ours: ambiguous.
        QVERIFY(ReleaseChecker::pickAsset(
                    assets({ "Foo-Qt5-2.0-x86_64.AppImage", "Foo-Qt6-2.0-x86_64.AppImage" }), {}, local, x64)
                    .isEmpty());

        // A gh-releases-zsync pattern selects among them (and matches the .AppImage sibling).
        QCOMPARE(ReleaseChecker::pickAsset(
                     assets({ "Foo-Qt5-2.0-x86_64.AppImage", "Foo-Qt6-2.0-x86_64.AppImage" }),
                     QStringLiteral("Foo-Qt6-*-x86_64.AppImage.zsync"), local, x64),
                 QStringLiteral("https://x/Foo-Qt6-2.0-x86_64.AppImage"));
    }
};

QTEST_GUILESS_MAIN(TstUpdateSource)
#include "tst_updatesource.moc"
