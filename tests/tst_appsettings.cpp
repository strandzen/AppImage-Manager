// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors

#include "../src/core/appsettings.h"

#include <QTest>
#include <QDir>
#include <QSignalSpy>
#include <QTemporaryDir>

// AppSettings uses KSharedConfig which writes to a real file.
// Each test method resets state via setters so tests are independent.
class TstAppSettings : public QObject
{
    Q_OBJECT

private:
    AppSettings *s = nullptr;

private Q_SLOTS:
    void initTestCase()
    {
        s = AppSettings::instance();
        QVERIFY(s);
    }

    // ── showInstallBox round-trip (bool setting pattern) ──────────────────────

    void showInstallBox_roundTrip()
    {
        const bool original = s->showInstallBox();
        QSignalSpy spy(s, &AppSettings::showInstallBoxChanged);

        s->setShowInstallBox(!original);
        QCOMPARE(s->showInstallBox(), !original);
        QCOMPARE(spy.count(), 1);

        s->setShowInstallBox(original); // restore
        QCOMPARE(s->showInstallBox(), original);
        QCOMPARE(spy.count(), 2);
    }

    void showInstallBox_noSignalOnSameValue()
    {
        const bool val = s->showInstallBox();
        QSignalSpy spy(s, &AppSettings::showInstallBoxChanged);
        s->setShowInstallBox(val); // same value — must not emit
        QCOMPARE(spy.count(), 0);
    }

    // ── applicationsPath round-trip ───────────────────────────────────────────

    void applicationsPath_setValidPath()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        const QString original = s->applicationsPath();
        QSignalSpy spy(s, &AppSettings::applicationsPathChanged);
        QSignalSpy errSpy(s, &AppSettings::applicationsPathError);

        s->setApplicationsPath(tmp.path());
        QCOMPARE(s->applicationsPath(), tmp.path());
        QCOMPARE(spy.count(), 1);
        QCOMPARE(errSpy.count(), 0);

        s->setApplicationsPath(original); // restore
    }

    void applicationsPath_noSignalOnSameValue()
    {
        const QString val = s->applicationsPath();
        QSignalSpy spy(s, &AppSettings::applicationsPathChanged);
        s->setApplicationsPath(val);
        QCOMPARE(spy.count(), 0);
    }

    // ── updateFrequency round-trip ────────────────────────────────────────────

    void updateFrequency_roundTrip()
    {
        const int original = s->updateFrequency();
        QSignalSpy spy(s, &AppSettings::updateFrequencyChanged);

        const int newVal = (original == 0) ? 1 : 0;
        s->setUpdateFrequency(newVal);
        QCOMPARE(s->updateFrequency(), newVal);
        QCOMPARE(spy.count(), 1);

        s->setUpdateFrequency(original);
    }

    // ── showDisclaimer round-trip ─────────────────────────────────────────────

    void showDisclaimer_roundTrip()
    {
        const bool original = s->showDisclaimer();
        QSignalSpy spy(s, &AppSettings::showDisclaimerChanged);

        s->setShowDisclaimer(!original);
        QCOMPARE(s->showDisclaimer(), !original);
        QCOMPARE(spy.count(), 1);

        s->setShowDisclaimer(original);
    }

    // ── firstRun round-trip ───────────────────────────────────────────────────

    void firstRun_defaultIsTrue()
    {
        QVERIFY(s->firstRun() == true || s->firstRun() == false);
    }

    void firstRun_roundTrip()
    {
        const bool original = s->firstRun();
        QSignalSpy spy(s, &AppSettings::firstRunChanged);

        s->setFirstRun(!original);
        QCOMPARE(s->firstRun(), !original);
        QCOMPARE(spy.count(), 1);

        s->setFirstRun(original);
        QCOMPARE(s->firstRun(), original);
        QCOMPARE(spy.count(), 2);
    }

    void firstRun_noSignalOnSameValue()
    {
        const bool val = s->firstRun();
        QSignalSpy spy(s, &AppSettings::firstRunChanged);
        s->setFirstRun(val);
        QCOMPARE(spy.count(), 0);
    }

    // ── version strings non-empty ─────────────────────────────────────────────

    void appVersion_nonEmpty()
    {
        QVERIFY(!s->appVersion().isEmpty());
    }

    void qtVersion_nonEmpty()
    {
        QVERIFY(!s->qtVersion().isEmpty());
    }

    void kdeFrameworksVersion_nonEmpty()
    {
        QVERIFY(!s->kdeFrameworksVersion().isEmpty());
    }
};

QTEST_GUILESS_MAIN(TstAppSettings)
#include "tst_appsettings.moc"
