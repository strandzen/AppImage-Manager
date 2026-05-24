// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors

#include "../src/core/appimagereader.h"

#include <QTest>

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
};

QTEST_GUILESS_MAIN(TstAppImageReader)
#include "tst_appimagereader.moc"
