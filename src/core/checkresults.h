// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#pragma once

#include <QDateTime>
#include <QHash>
#include <QString>

// Results of update checks, shared by the dashboard and the background daemon (separate
// processes) through the state config (~/.local/state/appimagemanagerstaterc):
// [Tray] LastCheck, [TrayResults] <AppImage file name> = state, new version, url, version
// at check, reason. Whoever writes tells the other over D-Bus (daemon ReloadState(),
// dashboard on UpdatesChanged), and both re-read.

// The order matches TrayModel::State and the values QML compares against.
enum class UpdateState { NotChecked = 0, UpToDate, UpdateAvailable, NoSource, Failed, Updating, Pinned };

struct CheckResult {
    UpdateState state = UpdateState::NotChecked;
    QString newVersion;
    QString url;
    QString versionAtCheck; // normalizeVersion() of the file that was checked
    QString reason;         // why the check failed, for people
};

// Keyed by file name. Only UpToDate, UpdateAvailable and Failed are stored.
QHash<QString, CheckResult> loadCheckResults();
void saveCheckResults(const QHash<QString, CheckResult> &results);
// One entry; other states than the stored ones remove it.
void saveCheckResult(const QString &fileName, const CheckResult &result);

QDateTime loadLastCheck();
void saveLastCheck(const QDateTime &when);

// "14:02", "yesterday 14:02", "Tuesday 14:02" or a date, relative to today.
QString formatCheckTime(const QDateTime &when);
