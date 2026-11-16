// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
//
// The daemon's check schedule (pure functions, no D-Bus, tray or network) and the
// tray window's model.

#include "../src/core/updatedaemon.h"
#include "../src/gui/traymodel.h"
#include "../src/core/checkresults.h"

#include <KConfigGroup>
#include <KSharedConfig>
#include <QStandardPaths>

#include <QSignalSpy>
#include <QTest>

class TstUpdateDaemon : public QObject
{
    Q_OBJECT

    static TrayModel::Item item(const QString &path, const QString &name, const QString &version)
    {
        TrayModel::Item item;
        item.path = path;
        item.name = name;
        item.version = version;
        return item;
    }

    const QDateTime now = QDateTime(QDate(2026, 11, 15), QTime(12, 0), QTimeZone::UTC);

private Q_SLOTS:
    void intervals()
    {
        QCOMPARE(UpdateDaemon::intervalDays(0, 7), 0);   // Never
        QCOMPARE(UpdateDaemon::intervalDays(1, 7), 1);   // Daily
        QCOMPARE(UpdateDaemon::intervalDays(2, 7), 7);   // Weekly
        QCOMPARE(UpdateDaemon::intervalDays(3, 7), 30);  // Monthly
        QCOMPARE(UpdateDaemon::intervalDays(4, 45), 45); // Custom
        QCOMPARE(UpdateDaemon::intervalDays(4, 0), 1);   // Custom, never below a day
    }

    void neverChecked_isDueNow()
    {
        QVERIFY(UpdateDaemon::isDue(now, QDateTime(), 1, 7));
        QCOMPARE(UpdateDaemon::nextCheck(now, QDateTime(), 1, 7), now);
    }

    void never_isNeverDue()
    {
        QVERIFY(!UpdateDaemon::isDue(now, QDateTime(), 0, 7));
        QVERIFY(!UpdateDaemon::nextCheck(now, now.addDays(-400), 0, 7).isValid());
    }

    void dueAfterTheInterval()
    {
        // Survives restarts: only the stored last check matters, not the daemon's uptime.
        QVERIFY(!UpdateDaemon::isDue(now, now.addSecs(-3600), 1, 7));
        QVERIFY(UpdateDaemon::isDue(now, now.addDays(-1), 1, 7));
        QVERIFY(!UpdateDaemon::isDue(now, now.addDays(-6), 2, 7));
        QVERIFY(UpdateDaemon::isDue(now, now.addDays(-7), 2, 7));
    }

    void longIntervals()
    {
        // Longer than a QTimer can wait (24.8 days): the schedule is not a timer any more.
        QVERIFY(!UpdateDaemon::isDue(now, now.addDays(-29), 3, 7));
        QVERIFY(UpdateDaemon::isDue(now, now.addDays(-30), 3, 7));
        QCOMPARE(UpdateDaemon::nextCheck(now, now, 4, 90), now.addDays(90));
    }

    void trayModel_sortedCountsAndStates()
    {
        TrayModel model;
        QSignalSpy summary(&model, &TrayModel::summaryChanged);
        model.upsert(item(QStringLiteral("/a/Zed.AppImage"), QStringLiteral("Zed"), QStringLiteral("1.0")));
        model.upsert(item(QStringLiteral("/a/Alpha.AppImage"), QStringLiteral("Alpha"), QStringLiteral("2.0")));
        QCOMPARE(model.items().size(), 2);
        QCOMPARE(model.items().first().name, QStringLiteral("Alpha"));
        QVERIFY(summary.count() >= 2);

        model.setState(QStringLiteral("/a/Zed.AppImage"), TrayModel::UpdateAvailable, QStringLiteral("1.1"), QStringLiteral("https://x/zed"));
        QCOMPARE(model.updateCount(), 1);
        model.setState(QStringLiteral("/a/Zed.AppImage"), TrayModel::Updating, QStringLiteral("1.1"), QStringLiteral("https://x/zed"));
        QCOMPARE(model.updateCount(), 0);
        QCOMPARE(model.updatingCount(), 1);
        model.setProgress(QStringLiteral("/a/Zed.AppImage"), 40);
        QCOMPARE(model.item(QStringLiteral("/a/Zed.AppImage"))->progress, 40);

        model.remove(QStringLiteral("/a/Alpha.AppImage"));
        QCOMPARE(model.items().size(), 1);
        QVERIFY(!model.item(QStringLiteral("/a/Alpha.AppImage")));
    }

    void checkResults_roundTrip()
    {
        QStandardPaths::setTestModeEnabled(true);
        QHash<QString, CheckResult> results;
        results.insert(QStringLiteral("a.AppImage"), { UpdateState::UpdateAvailable, QStringLiteral("2.0"),
                                                       QStringLiteral("https://x/a"), QStringLiteral("1.0"), {} });
        results.insert(QStringLiteral("b.AppImage"), { UpdateState::Failed, {}, {}, QStringLiteral("3.1"),
                                                       QStringLiteral("Repository not found") });
        results.insert(QStringLiteral("c.AppImage"), { UpdateState::NotChecked, {}, {}, QStringLiteral("1"), {} });
        saveCheckResults(results);

        const auto loaded = loadCheckResults();
        QCOMPARE(loaded.size(), 2); // NotChecked is not stored
        QCOMPARE(loaded.value(QStringLiteral("a.AppImage")).url, QStringLiteral("https://x/a"));
        QCOMPARE(loaded.value(QStringLiteral("b.AppImage")).reason, QStringLiteral("Repository not found"));

        // One entry; a state that is not stored removes it.
        saveCheckResult(QStringLiteral("a.AppImage"), {});
        QVERIFY(!loadCheckResults().contains(QStringLiteral("a.AppImage")));

        // 2.1.0 wrote four fields, without the reason.
        KSharedConfig::openStateConfig(QStringLiteral("appimagemanagerstaterc"))->group(QStringLiteral("TrayResults"))
            .writeEntry("old.AppImage", QStringList{ QStringLiteral("1"), {}, {}, QStringLiteral("0.5") });
        QCOMPARE(loadCheckResults().value(QStringLiteral("old.AppImage")).state, UpdateState::UpToDate);

        const QDateTime when = QDateTime::currentDateTimeUtc();
        saveLastCheck(when);
        QCOMPARE(loadLastCheck().toSecsSinceEpoch(), when.toSecsSinceEpoch());
        QVERIFY(!formatCheckTime(when).isEmpty());
        QVERIFY(formatCheckTime(QDateTime()).isEmpty());
    }

    void trayModel_updatesFirst()
    {
        TrayModel model;
        model.upsert(item(QStringLiteral("/a/A.AppImage"), QStringLiteral("A"), QStringLiteral("1")));
        model.upsert(item(QStringLiteral("/a/B.AppImage"), QStringLiteral("B"), QStringLiteral("1")));
        model.upsert(item(QStringLiteral("/a/C.AppImage"), QStringLiteral("C"), QStringLiteral("1")));
        model.setState(QStringLiteral("/a/A.AppImage"), TrayModel::NoSource);
        model.setState(QStringLiteral("/a/C.AppImage"), TrayModel::UpdateAvailable, QStringLiteral("2"), QStringLiteral("u"));
        QCOMPARE(model.items().at(0).name, QStringLiteral("C")); // update first
        QCOMPARE(model.items().at(2).name, QStringLiteral("A")); // no source last
    }

    void trayModel_scheduleText()
    {
        TrayModel model;
        QVERIFY(model.scheduleText().contains(QStringLiteral("Not checked")));
        model.setSchedule(QDateTime::currentDateTimeUtc(), QDateTime::currentDateTimeUtc().addDays(1));
        QVERIFY(model.scheduleText().contains(QStringLiteral("Checked")));
        QVERIFY(model.scheduleText().contains(QStringLiteral("tomorrow")));
    }
};

QTEST_GUILESS_MAIN(TstUpdateDaemon)
#include "tst_updatedaemon.moc"
