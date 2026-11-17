// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors

#include "../src/core/releasechecker.h"
#include "../src/core/launchenvironment.h"
#include "../src/core/previousversion.h"
#include "../src/core/sandbox.h"
#include "../src/core/releasenotes.h"
#include "../src/core/updatesource.h"

#include <KConfigGroup>
#include <KSharedConfig>

#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStandardPaths>
#include <QTest>
#include <QDir>
#include <QProcess>
#include <QFileInfo>
#include <QFile>
#include <QTemporaryDir>

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

    void releaseNotes_pick()
    {
        const auto release = [](const char *tag, const QString &body, bool pre, bool draft = false) {
            return QJsonObject{ { QStringLiteral("tag_name"), QString::fromLatin1(tag) }, { QStringLiteral("body"), body },
                                { QStringLiteral("prerelease"), pre }, { QStringLiteral("draft"), draft } };
        };
        QJsonObject first = release("v2.1.0", QStringLiteral("## New\r\n- a ![shot](https://x/s.png)\n<!-- template -->\n<img src=\"x\">"), false);
        first.insert(QStringLiteral("name"), QStringLiteral("Big one"));
        first.insert(QStringLiteral("html_url"), QStringLiteral("https://g/r/2.1.0"));
        first.insert(QStringLiteral("published_at"), QStringLiteral("2026-11-10T10:00:00Z"));
        const QByteArray json = QJsonDocument(QJsonArray{
            first, release("v2.1.0-rc1", QStringLiteral("rc"), true), release("v2.0.1", QStringLiteral("fix"), false),
            release("v2.0.0", QStringLiteral("old"), false), release("v3.0.0", QStringLiteral("draft"), false, true),
        }).toJson();
        // Everything newer than 2.0.0, newest first; drafts and pre-releases out.
        const QList<ReleaseNote> newer = pickReleaseNotes(json, QStringLiteral("2.0.0"), {}, false);
        QCOMPARE(newer.size(), 2);
        QCOMPARE(newer.at(0).tag, QStringLiteral("v2.1.0"));
        QCOMPARE(newer.at(0).body, QStringLiteral("## New\n- a"));  // image, comment, <img> gone
        QVERIFY(newer.at(0).published.isValid());
        QCOMPARE(newer.at(1).tag, QStringLiteral("v2.0.1"));
        // Pre-releases when the source allows them.
        QCOMPARE(pickReleaseNotes(json, QStringLiteral("2.0.0"), {}, true).size(), 3);
        // Up to date, or the version unknown: the newest only.
        QCOMPARE(pickReleaseNotes(json, QStringLiteral("2.1.0"), {}, false).size(), 1);
        QCOMPARE(pickReleaseNotes(json, {}, {}, false).size(), 1);
        // Stops at the installed tag (version-less AppImages).
        QCOMPARE(pickReleaseNotes(json, QStringLiteral("1.0"), QStringLiteral("v2.0.1"), false).size(), 1);
        QVERIFY(pickReleaseNotes("not json", QStringLiteral("1"), {}, false).isEmpty());

        QCOMPARE(releaseListUrl(QStringLiteral("gh-releases-zsync|o|r|latest|*.zsync")).toString(),
                 QStringLiteral("https://api.github.com/repos/o/r/releases?per_page=20"));
        QVERIFY(!releaseListUrl(QStringLiteral("zsync|https://x/a.zsync")).isValid());
    }

    void webUrl()
    {
        QCOMPARE(updateSourceWebUrl(QStringLiteral("gh-releases-zsync|owner|repo|latest|*.zsync")),
                 QStringLiteral("https://github.com/owner/repo"));
        QCOMPARE(updateSourceWebUrl(QStringLiteral("forgejo-releases|codeberg.org|o|r")),
                 QStringLiteral("https://codeberg.org/o/r"));
        QVERIFY(updateSourceWebUrl(QStringLiteral("zsync|https://x/a.zsync")).isEmpty());
        QVERIFY(updateSourceWebUrl(QStringLiteral("direct|https://x/a.AppImage")).isEmpty());
    }

    void configFolder()
    {
        QCOMPARE(defaultConfigFolderName(QStringLiteral("/apps/mailcraft-0.13.0-linux-x86_64.AppImage")),
                 QStringLiteral("mailcraft"));
        QCOMPARE(defaultConfigFolderName(QStringLiteral("/apps/ProtonUp-Qt-2.9.1-x86_64.AppImage")),
                 QStringLiteral("protonup-qt"));
        QCOMPARE(defaultConfigFolderName(QStringLiteral("/a/serein-v1.0.0-nightly.57-Linux-X64.AppImage")), QStringLiteral("serein"));
        QCOMPARE(defaultConfigFolderName(QStringLiteral("/a/kensaku_1.0.17_amd64.AppImage")), QStringLiteral("kensaku"));
        QCOMPARE(defaultConfigFolderName(QStringLiteral("/a/Feishin-linux-x86_64.AppImage")), QStringLiteral("feishin"));

        const QString path = QStringLiteral("/tmp/aim-test-apps/Cfg-1.0-x86_64.AppImage");
        QVERIFY(configFolderPath(path).isEmpty());
        QVERIFY(!launchVariables(path).join(QLatin1Char(' ')).contains(QStringLiteral("XDG_CONFIG_HOME")));

        setConfigFolderName(path, QStringLiteral("cfg"));
        QCOMPARE(configFolderPath(path), QStringLiteral("/tmp/aim-test-apps/.config/cfg"));
        const QStringList vars = launchVariables(path);
        QVERIFY(vars.contains(QStringLiteral("XDG_CONFIG_HOME=/tmp/aim-test-apps/.config/cfg")));
        QVERIFY(std::any_of(vars.cbegin(), vars.cend(), [](const QString &v) { return v.startsWith(QStringLiteral("XDG_CONFIG_DIRS=")); }));

        // The user's own XDG_CONFIG_HOME wins.
        saveEnvironment(path, { QStringLiteral("XDG_CONFIG_HOME=/mine") });
        QCOMPARE(launchVariables(path).filter(QStringLiteral("XDG_CONFIG_HOME=")),
                 QStringList{ QStringLiteral("XDG_CONFIG_HOME=/mine") });

        saveEnvironment(path, {});
        setConfigFolderName(path, {});
        QVERIFY(configFolderPath(path).isEmpty());
        QDir(QStringLiteral("/tmp/aim-test-apps")).removeRecursively();
    }

    void previousVersion_keepAndRestore()
    {
        QTemporaryDir dir;
        const QString app = dir.filePath(QStringLiteral("App.AppImage"));
        const auto write = [](const QString &path, const QByteArray &data) {
            QFile f(path);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write(data);
        };
        const auto read = [](const QString &path) {
            QFile f(path);
            return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
        };
        QVERIFY(!hasPrevious(app));

        // An update: the old file (renamed aside by the swap) is kept.
        write(app, "new");
        write(app + QStringLiteral(".bak"), "old");
        saveInstalledTag(app, QStringLiteral("v1"));
        QVERIFY(keepPrevious(app, app + QStringLiteral(".bak"), QStringLiteral("1.0")));
        QVERIFY(!QFile::exists(app + QStringLiteral(".bak")));
        QCOMPARE(read(previousFilePath(app)), QByteArray("old"));
        QCOMPARE(::previousVersion(app), QStringLiteral("1.0"));
        saveInstalledTag(app, QStringLiteral("v2")); // what the update installed

        // Restore swaps the files, versions and installed tags; again swaps back.
        QVERIFY(restorePrevious(app, QStringLiteral("2.0")));
        QCOMPARE(read(app), QByteArray("old"));
        QCOMPARE(read(previousFilePath(app)), QByteArray("new"));
        QCOMPARE(::previousVersion(app), QStringLiteral("2.0"));
        QCOMPARE(loadUpdateSource(app).installedTag, QStringLiteral("v1"));
        QVERIFY(QFileInfo(app).isExecutable());
        QVERIFY(restorePrevious(app, QStringLiteral("1.0")));
        QCOMPARE(read(app), QByteArray("new"));
        QCOMPARE(loadUpdateSource(app).installedTag, QStringLiteral("v2"));

        forgetPrevious(app);
        saveInstalledTag(app, {});
    }

    void sandbox_settingsAndCommand()
    {
        const QString path = QStringLiteral("/apps/Box-1.0.AppImage");
        QVERIFY(!loadSandbox(path).enabled);
        QCOMPARE(launchCommand(path), QStringList{ path });
        saveSandbox(path, { true, { QStringLiteral("/home/u/Music") } });
        QVERIFY(loadSandbox(path).enabled);
        QCOMPARE(loadSandbox(path).folders, QStringList{ QStringLiteral("/home/u/Music") });
        const QStringList command = launchCommand(path);
        QCOMPARE(command.size(), 3);
        QCOMPARE(command.at(1), QStringLiteral("--run"));
        QCOMPARE(command.at(2), path);
        saveSandbox(path, {});
        QVERIFY(!loadSandbox(path).enabled);
    }

    void sandbox_followsSandboxAll()
    {
        if (!sandboxAvailable())
            QSKIP("Sandbox all needs bwrap");
        KConfigGroup security = KSharedConfig::openConfig(QStringLiteral("appimagemanagerrc"))->group(QStringLiteral("Security"));
        security.writeEntry("sandboxAll", true);
        security.writeEntry("sandboxFolders", QStringList{ QStringLiteral("/home/u/Downloads") });
        security.sync();
        const QString path = QStringLiteral("/apps/All-1.0.AppImage");
        QVERIFY(sandboxAllEnabled());
        QVERIFY(loadSandbox(path).enabled);                 // follows Sandbox all
        QCOMPARE(sharedSandboxFolders(), QStringList{ QStringLiteral("/home/u/Downloads") });

        saveSandbox(path, { false, {} });                   // left out: stored
        QVERIFY(!loadSandbox(path).enabled);
        saveSandbox(path, { true, {} });                    // same as Sandbox all: follows again
        QVERIFY(!KSharedConfig::openConfig(QStringLiteral("appimagemanagerrc"))->group(QStringLiteral("Sandbox"))
                     .group(QStringLiteral("All-1.0.AppImage")).exists());
        security.deleteEntry("sandboxAll");
        security.deleteEntry("sandboxFolders");
        security.sync();
        QVERIFY(!loadSandbox(path).enabled);
    }

    // Runs bwrap with the real arguments, a fake mount and an AppRun script that reports
    // what it sees. Skipped without bwrap (or where user namespaces are off).
    void sandbox_isolatesHome()
    {
        if (!sandboxAvailable())
            QSKIP("bwrap not installed");
        QTemporaryDir root;
        const QString home = root.filePath(QStringLiteral("home"));
        const QString settings = home + QStringLiteral("/Applications/.config/box");
        const QString given = home + QStringLiteral("/Music");
        const QString mount = root.filePath(QStringLiteral("mnt"));
        for (const QString &dir : QStringList{ settings, given, mount, home + QStringLiteral("/Secret") })
            QVERIFY(QDir().mkpath(dir));
        const QString appImage = home + QStringLiteral("/Applications/Box.AppImage");
        QFile(appImage).open(QIODevice::WriteOnly);
        QFile run(mount + QStringLiteral("/AppRun"));
        QVERIFY(run.open(QIODevice::WriteOnly));
        run.write("#!/bin/sh\nout=\"$1/report\"\nls -A \"$2\" > \"$out\"\n"
                  "touch \"$3/written\" && echo music-ok >> \"$out\"\n"
                  "touch \"$2/escape\" 2>/dev/null && echo home-writable >> \"$out\"\n"
                  "test -e /mnt/Storage && echo mnt-visible >> \"$out\"\nexit 0\n");
        run.close();
        run.setPermissions(run.permissions() | QFileDevice::ExeOwner);

        QProcess bwrap;
        bwrap.setProgram(QStandardPaths::findExecutable(QStringLiteral("bwrap")));
        bwrap.setArguments(sandboxArguments(appImage, mount, settings, { given }, home,
                                            qEnvironmentVariable("XDG_RUNTIME_DIR", QStringLiteral("/tmp")),
                                            { settings, home, given }));
        bwrap.start();
        QVERIFY(bwrap.waitForFinished(20000));
        if (bwrap.exitCode() != 0 && !QFile::exists(settings + QStringLiteral("/report")))
            QSKIP(qPrintable(QStringLiteral("bwrap could not run here: ") + QString::fromLocal8Bit(bwrap.readAllStandardError())));

        QFile report(settings + QStringLiteral("/report"));
        QVERIFY(report.open(QIODevice::ReadOnly));
        const QString seen = QString::fromUtf8(report.readAll());
        QVERIFY2(!seen.contains(QStringLiteral("Secret")), qPrintable(seen));      // the rest of home is hidden
        QVERIFY2(seen.contains(QStringLiteral("music-ok")), qPrintable(seen));      // a given folder is writable
        QVERIFY(QFile::exists(given + QStringLiteral("/written")));
        QVERIFY2(!seen.contains(QStringLiteral("mnt-visible")), qPrintable(seen));  // other drives hidden
        QVERIFY(!QFile::exists(home + QStringLiteral("/escape")));                  // writes to home vanish
    }

    // Network off: the app's own network namespace has only loopback; on: the host's interfaces.
    void sandbox_network()
    {
        if (!sandboxAvailable())
            QSKIP("bwrap not installed");
        // Saved only when off; absent means allowed.
        const QString file = QStringLiteral("/x/NetBox.AppImage");
        saveSandbox(file, { true, {}, false });
        QVERIFY(!loadSandbox(file).network);
        saveSandbox(file, { true, {}, true });
        QVERIFY(loadSandbox(file).network);

        QTemporaryDir root;
        const QString home = root.filePath(QStringLiteral("home"));
        const QString settings = home + QStringLiteral("/Applications/.config/net");
        const QString mount = root.filePath(QStringLiteral("mnt"));
        QVERIFY(QDir().mkpath(settings) && QDir().mkpath(mount));
        const QString appImage = home + QStringLiteral("/Applications/Net.AppImage");
        QFile(appImage).open(QIODevice::WriteOnly);
        QFile run(mount + QStringLiteral("/AppRun"));
        QVERIFY(run.open(QIODevice::WriteOnly));
        // Interface names from the sandbox's own /proc (not /sys, which is the host's).
        run.write("#!/bin/sh\ntail -n +3 /proc/net/dev | cut -d: -f1 | tr -d ' ' > \"$1/$2\"\n");
        run.close();
        run.setPermissions(run.permissions() | QFileDevice::ExeOwner);

        auto interfaces = [&](bool network, const QString &name) {
            QProcess bwrap;
            bwrap.setProgram(QStandardPaths::findExecutable(QStringLiteral("bwrap")));
            bwrap.setArguments(sandboxArguments(appImage, mount, settings, {}, home,
                                                qEnvironmentVariable("XDG_RUNTIME_DIR", QStringLiteral("/tmp")),
                                                { settings, name }, network));
            bwrap.start();
            bwrap.waitForFinished(20000);
            QFile out(settings + QLatin1Char('/') + name);
            return out.open(QIODevice::ReadOnly) ? QString::fromUtf8(out.readAll()).split(QLatin1Char('\n'), Qt::SkipEmptyParts)
                                                 : QStringList{ QStringLiteral("<no report>") };
        };
        const QStringList offline = interfaces(false, QStringLiteral("off"));
        if (offline == QStringList{ QStringLiteral("<no report>") })
            QSKIP("bwrap could not run here");
        QCOMPARE(offline, QStringList{ QStringLiteral("lo") });
        QVERIFY2(interfaces(true, QStringLiteral("on")).size() > 1, "the host's interfaces with network on");
    }

    // "Don't Update": keyed by file name, independent of the update source.
    void pinned()
    {
        const QString path = QStringLiteral("/somewhere/Pinned-1.0-x86_64.AppImage");
        QVERIFY(!isPinned(path));
        setPinned(path, true);
        QVERIFY(isPinned(path));
        QVERIFY(isPinned(QStringLiteral("/elsewhere/Pinned-1.0-x86_64.AppImage")));
        // Clearing the update source keeps the pin.
        saveUpdateSource(path, {});
        QVERIFY(isPinned(path));
        setPinned(path, false);
        QVERIFY(!isPinned(path));
    }

    // GitHub's releases.atom: the tag comes from the entry link (titles are release names).
    void feedNewestTag()
    {
        const QByteArray head = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<feed xmlns=\"http://www.w3.org/2005/Atom\">\n"
            "  <id>tag:github.com,2008:https://github.com/o/r/releases</id>\n"
            "  <link type=\"text/html\" rel=\"alternate\" href=\"https://github.com/o/r/releases\"/>\n"
            "  <title>Release notes from r</title>\n";
        const QByteArray full = head +
            "  <entry>\n"
            "    <id>tag:github.com,2008:Repository/1/v1.18.2</id>\n"
            "    <link rel=\"alternate\" type=\"text/html\" href=\"https://github.com/o/r/releases/tag/v1.18.2\"/>\n"
            "    <title>LocalSend 1.18.2 &amp; fixes</title>\n"
            "    <content type=\"html\">&lt;p&gt;notes&lt;/p&gt;</content>\n"
            "  </entry>\n"
            "  <entry><id>tag:github.com,2008:Repository/1/v1.18.1</id><title>v1.18.1</title></entry>\n"
            "</feed>\n";
        QCOMPARE(ReleaseChecker::feedNewestTag(full), QStringLiteral("v1.18.2"));
        // Percent-encoded tag in the link.
        QCOMPARE(ReleaseChecker::feedNewestTag(head + "<entry><link href=\"https://github.com/o/r/releases/tag/release%2F2.0\"/>"
                                                      "</entry></feed>"),
                 QStringLiteral("release/2.0"));
        // No link: the id's last part, then the title.
        QCOMPARE(ReleaseChecker::feedNewestTag(head + "<entry><id>tag:github.com,2008:Repository/1/2.0</id></entry></feed>"),
                 QStringLiteral("2.0"));
        QCOMPARE(ReleaseChecker::feedNewestTag(head + "<entry><title>3.0</title></entry></feed>"), QStringLiteral("3.0"));
        QVERIFY(ReleaseChecker::feedNewestTag(head + "</feed>").isEmpty());
        QVERIFY(ReleaseChecker::feedNewestTag("not xml").isEmpty());
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
