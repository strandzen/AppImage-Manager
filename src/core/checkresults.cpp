// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#include "checkresults.h"

#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>

#include <QLocale>

static KSharedConfig::Ptr stateConfig()
{
    KSharedConfig::Ptr config = KSharedConfig::openStateConfig(QStringLiteral("appimagemanagerstaterc"));
    config->reparseConfiguration(); // the other process may have written
    return config;
}

static bool isStored(UpdateState state)
{
    return state == UpdateState::UpToDate || state == UpdateState::UpdateAvailable
        || state == UpdateState::Failed;
}

static QStringList toEntry(const CheckResult &result)
{
    return { QString::number(static_cast<int>(result.state)), result.newVersion, result.url,
             result.versionAtCheck, result.reason };
}

QHash<QString, CheckResult> loadCheckResults()
{
    QHash<QString, CheckResult> results;
    const KConfigGroup group = stateConfig()->group(QStringLiteral("TrayResults"));
    for (const QString &key : group.keyList()) {
        const QStringList entry = group.readEntry(key, QStringList());
        if (entry.size() < 4) // 2.1.0 wrote four fields, without the reason
            continue;
        CheckResult result;
        result.state = static_cast<UpdateState>(entry.at(0).toInt());
        result.newVersion = entry.at(1);
        result.url = entry.at(2);
        result.versionAtCheck = entry.at(3);
        result.reason = entry.value(4);
        if (isStored(result.state))
            results.insert(key, result);
    }
    return results;
}

void saveCheckResults(const QHash<QString, CheckResult> &results)
{
    KSharedConfig::Ptr config = stateConfig();
    KConfigGroup group = config->group(QStringLiteral("TrayResults"));
    group.deleteGroup();
    for (auto it = results.cbegin(); it != results.cend(); ++it) {
        if (isStored(it->state))
            group.writeEntry(it.key(), toEntry(*it));
    }
    config->sync();
}

void saveCheckResult(const QString &fileName, const CheckResult &result)
{
    KSharedConfig::Ptr config = stateConfig();
    KConfigGroup group = config->group(QStringLiteral("TrayResults"));
    if (isStored(result.state))
        group.writeEntry(fileName, toEntry(result));
    else
        group.deleteEntry(fileName);
    config->sync();
}

QDateTime loadLastCheck()
{
    return stateConfig()->group(QStringLiteral("Tray")).readEntry("LastCheck", QDateTime());
}

void saveLastCheck(const QDateTime &when)
{
    KSharedConfig::Ptr config = stateConfig();
    config->group(QStringLiteral("Tray")).writeEntry("LastCheck", when);
    config->sync();
}

QString formatCheckTime(const QDateTime &when)
{
    if (!when.isValid())
        return {};
    const QDateTime local = when.toLocalTime();
    const QLocale locale;
    const QString clock = locale.toString(local.time(), QLocale::ShortFormat);
    const qint64 days = QDate::currentDate().daysTo(local.date());
    if (days == 0)
        return clock;
    if (days == -1)
        return i18nc("time", "yesterday %1", clock);
    if (days == 1)
        return i18nc("time", "tomorrow %1", clock);
    if (days > -7 && days < 7)
        return locale.toString(local, QStringLiteral("dddd ")) + clock;
    return locale.toString(local.date(), QLocale::ShortFormat);
}
