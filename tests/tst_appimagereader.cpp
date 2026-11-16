// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors

#include "../src/core/appimagereader.h"

#include <QTest>
#include <QTemporaryDir>
#include <QStandardPaths>
#include <QProcess>
#include <QFile>
#include <QDir>
#include <QCryptographicHash>

class TstAppImageReader : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    // ── cleanName ─────────────────────────────────────────────────────────────
    // Exercises the public static helper that the UI shows as the display name.

    void cleanName_stripsExtension() {
        QCOMPARE(AppImageReader::cleanName(QStringLiteral("MyApp.AppImage")),
                 QStringLiteral("MyApp.AppImage")); // extension replaced by canonical form
    }
    void cleanName_x86_64Suffix() {
        QVERIFY(!AppImageReader::cleanName(QStringLiteral("ProtonUp-Qt-2.9.1-x86_64.AppImage"))
                     .contains(QStringLiteral("x86_64")));
    }
    void cleanName_aarch64Suffix() {
        QVERIFY(!AppImageReader::cleanName(QStringLiteral("App-1.0-aarch64.AppImage"))
                     .contains(QStringLiteral("aarch64")));
    }
    void cleanName_arm64Suffix() {
        QVERIFY(!AppImageReader::cleanName(QStringLiteral("App-1.0-arm64.AppImage"))
                     .contains(QStringLiteral("arm64")));
    }
    void cleanName_capitalisation() {
        const QString result = AppImageReader::cleanName(QStringLiteral("my-cool-app.AppImage"));
        QVERIFY(result.startsWith(QLatin1Char('M')));
    }
    void cleanName_nonAppImage_passthrough() {
        const QString in = QStringLiteral("document.pdf");
        QCOMPARE(AppImageReader::cleanName(in), in);
    }
    void cleanName_endsWithAppImage() {
        const QString result = AppImageReader::cleanName(QStringLiteral("Foo-1.2.AppImage"));
        QVERIFY(result.endsWith(QStringLiteral(".AppImage")));
    }
    void cleanName_uppercaseExtension() {
        // ".APPIMAGE" matches case-insensitively and is normalised to ".AppImage"
        QCOMPARE(AppImageReader::cleanName(QStringLiteral("Foo.APPIMAGE")),
                 QStringLiteral("Foo.AppImage"));
    }
    void cleanName_dashesToSpaces() {
        const QString result = AppImageReader::cleanName(QStringLiteral("My-App.AppImage"));
        QVERIFY(result.contains(QLatin1Char(' ')));
    }
    void cleanName_underscoresToSpaces() {
        const QString result = AppImageReader::cleanName(QStringLiteral("My_App.AppImage"));
        QVERIFY(result.contains(QLatin1Char(' ')));
    }
    void cleanName_amd64Suffix() {
        QVERIFY(!AppImageReader::cleanName(QStringLiteral("App-1.0-amd64.AppImage"))
                     .contains(QStringLiteral("amd64")));
    }

    // ── verifySignature ───────────────────────────────────────────────────────
    // A copy of /usr/bin/true with the runtime's three sections, signed the way
    // appimagetool --sign does (hex SHA-256 with the sections zeroed, key embedded).
    void verifySignature_signedUnsignedTampered()
    {
        const QString gpg = QStandardPaths::findExecutable(QStringLiteral("gpg"));
        const QString objcopy = QStandardPaths::findExecutable(QStringLiteral("objcopy"));
        if (gpg.isEmpty() || objcopy.isEmpty())
            QSKIP("needs gpg and objcopy");
        QTemporaryDir dir;
        const auto run = [](const QString &program, const QStringList &args) {
            QProcess p;
            p.start(program, args);
            p.waitForFinished(60000);
            return p.exitCode() == 0 && p.exitStatus() == QProcess::NormalExit;
        };
        const auto writeFile = [](const QString &path, const QByteArray &data) {
            QFile f(path);
            return f.open(QIODevice::WriteOnly) && f.write(data) == data.size();
        };
        const QString app = dir.filePath(QStringLiteral("App.AppImage"));
        QVERIFY(writeFile(dir.filePath(QStringLiteral("z16")), QByteArray(16, '\0')));
        QVERIFY(writeFile(dir.filePath(QStringLiteral("z1k")), QByteArray(1024, '\0')));
        QVERIFY(writeFile(dir.filePath(QStringLiteral("z8k")), QByteArray(8192, '\0')));
        QVERIFY(run(objcopy, { QStringLiteral("--add-section"), QStringLiteral(".digest_md5=") + dir.filePath(QStringLiteral("z16")),
                               QStringLiteral("--add-section"), QStringLiteral(".sha256_sig=") + dir.filePath(QStringLiteral("z1k")),
                               QStringLiteral("--add-section"), QStringLiteral(".sig_key=") + dir.filePath(QStringLiteral("z8k")),
                               QStringLiteral("/usr/bin/true"), app }));
        QCOMPARE(AppImageReader::verifySignature(app), SignatureState::Unsigned);

        // Sign: the hex SHA-256 of the file as it is now (sections zeroed).
        QFile in(app);
        QVERIFY(in.open(QIODevice::ReadOnly));
        const QByteArray digest = QCryptographicHash::hash(in.readAll(), QCryptographicHash::Sha256).toHex();
        in.close();
        const QString home = dir.filePath(QStringLiteral("gnupg"));
        QDir().mkpath(home);
        QFile::setPermissions(home, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
        const QStringList h = { QStringLiteral("--homedir"), home, QStringLiteral("--batch"), QStringLiteral("--pinentry-mode"),
                                QStringLiteral("loopback"), QStringLiteral("--passphrase"), QString() };
        QVERIFY(run(gpg, h + QStringList{ QStringLiteral("--quick-gen-key"), QStringLiteral("test@example.invalid"),
                                          QStringLiteral("ed25519"), QStringLiteral("sign"), QStringLiteral("never") }));
        QVERIFY(writeFile(dir.filePath(QStringLiteral("digest")), digest));
        QVERIFY(run(gpg, h + QStringList{ QStringLiteral("--armor"), QStringLiteral("--detach-sign"), QStringLiteral("-o"),
                                          dir.filePath(QStringLiteral("sig.asc")), dir.filePath(QStringLiteral("digest")) }));
        QVERIFY(run(gpg, h + QStringList{ QStringLiteral("--armor"), QStringLiteral("--export"), QStringLiteral("-o"),
                                          dir.filePath(QStringLiteral("key.asc")) }));
        const auto padded = [&](const QString &name, int size) {
            QFile f(dir.filePath(name));
            f.open(QIODevice::ReadOnly);
            QByteArray data = f.readAll();
            data.append(QByteArray(size - data.size(), '\0'));
            writeFile(dir.filePath(name + QStringLiteral(".pad")), data);
            return dir.filePath(name + QStringLiteral(".pad"));
        };
        const QString signedApp = dir.filePath(QStringLiteral("Signed.AppImage"));
        QVERIFY(run(objcopy, { QStringLiteral("--update-section"), QStringLiteral(".sha256_sig=") + padded(QStringLiteral("sig.asc"), 1024),
                               QStringLiteral("--update-section"), QStringLiteral(".sig_key=") + padded(QStringLiteral("key.asc"), 8192),
                               app, signedApp }));

        QString fingerprint;
        QCOMPARE(AppImageReader::verifySignature(signedApp, &fingerprint), SignatureState::Valid);
        QCOMPARE(fingerprint.size(), 40);

        // One byte changed outside the sections: the signature no longer matches.
        QFile tamper(signedApp);
        QVERIFY(tamper.open(QIODevice::ReadWrite));
        tamper.seek(100);
        const char c = tamper.read(1).at(0);
        tamper.seek(100);
        tamper.write(QByteArray(1, char(c ^ 0x01)));
        tamper.close();
        QCOMPARE(AppImageReader::verifySignature(signedApp), SignatureState::Invalid);
    }
};

QTEST_GUILESS_MAIN(TstAppImageReader)
#include "tst_appimagereader.moc"
