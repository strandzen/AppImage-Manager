// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors

#include "../src/core/appimageinfo.h"

#include <QTest>

class TstAppImageInfo : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    // ── normalizeVersion ──────────────────────────────────────────────────────

    void normalizeVersion_noPrefix()      { QCOMPARE(normalizeVersion(QStringLiteral("1.2.3")),  QStringLiteral("1.2.3")); }
    void normalizeVersion_lowerV()        { QCOMPARE(normalizeVersion(QStringLiteral("v1.2.3")), QStringLiteral("1.2.3")); }
    void normalizeVersion_upperV()        { QCOMPARE(normalizeVersion(QStringLiteral("V2.0")),   QStringLiteral("2.0")); }
    void normalizeVersion_empty()         { QCOMPARE(normalizeVersion(QStringLiteral("")),        QStringLiteral("")); }
    void normalizeVersion_vOnly()         { QCOMPARE(normalizeVersion(QStringLiteral("v")),       QStringLiteral("")); }
    void normalizeVersion_notVersion()    { QCOMPARE(normalizeVersion(QStringLiteral("abc")),     QStringLiteral("abc")); }

    // ── isNewerVersion ────────────────────────────────────────────────────────

    void newer_patchIncrement()   { QVERIFY( isNewerVersion(QStringLiteral("1.0.1"), QStringLiteral("1.0.0"))); }
    void newer_minorIncrement()   { QVERIFY( isNewerVersion(QStringLiteral("1.1.0"), QStringLiteral("1.0.9"))); }
    void newer_majorIncrement()   { QVERIFY( isNewerVersion(QStringLiteral("2.0.0"), QStringLiteral("1.9.9"))); }
    void newer_equalVersions()    { QVERIFY(!isNewerVersion(QStringLiteral("1.0.0"), QStringLiteral("1.0.0"))); }
    void newer_remoteOlder()      { QVERIFY(!isNewerVersion(QStringLiteral("1.0.0"), QStringLiteral("1.0.1"))); }
    void newer_twoComponents()    { QVERIFY( isNewerVersion(QStringLiteral("1.1"),   QStringLiteral("1.0"))); }
    void newer_twoVsThree()       { QVERIFY(!isNewerVersion(QStringLiteral("1.0"),   QStringLiteral("1.0.1"))); }
    void newer_withLeadingV()     {
        // normalizeVersion should be applied before comparison
        QVERIFY(isNewerVersion(normalizeVersion(QStringLiteral("v1.2")),
                               normalizeVersion(QStringLiteral("v1.1"))));
    }
    void newer_identicalStrings() { QVERIFY(!isNewerVersion(QStringLiteral("3.14.159"), QStringLiteral("3.14.159"))); }
    void newer_nonNumericFallsToZero() {
        // Non-numeric components convert to 0 via toInt(); 0 < 1 → remote not newer
        QVERIFY(!isNewerVersion(QStringLiteral("1.0.alpha"), QStringLiteral("1.0.1")));
    }

    // ── kAppImageFilters ──────────────────────────────────────────────────────

    void filters_containsBothCasings() {
        const QStringList &f = kAppImageFilters();
        QVERIFY(f.contains(QStringLiteral("*.AppImage")));
        QVERIFY(f.contains(QStringLiteral("*.appimage")));
    }
    void filters_exactlyTwo() { QCOMPARE(kAppImageFilters().size(), 2); }
    void filters_sameObjectOnRepeatCall() {
        QCOMPARE(&kAppImageFilters(), &kAppImageFilters());
    }
};

QTEST_GUILESS_MAIN(TstAppImageInfo)
#include "tst_appimageinfo.moc"
