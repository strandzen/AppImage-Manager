// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#include "appsettings.h"

#include <KConfigGroup>

#include <QDir>

AppSettings *AppSettings::instance()
{
    static AppSettings s_instance;
    return &s_instance;
}

AppSettings::AppSettings(QObject *parent)
    : QObject(parent)
    , m_config(KSharedConfig::openConfig(QStringLiteral("appimagemanagerrc")))
{
}

QString AppSettings::applicationsPath() const
{
    // Build default as a proper QString to avoid QStringBuilder type deduction
    // confusing KConfigGroup::readEntry's template parameter.
    QString defaultPath(QDir::homePath());
    defaultPath += QStringLiteral("/Applications");
    return m_config->group(QStringLiteral("General"))
               .readEntry(QStringLiteral("applicationsPath"), defaultPath);
}

void AppSettings::setApplicationsPath(const QString &path)
{
    if (applicationsPath() == path)
        return;
    if (!QDir().mkpath(path)) {
        Q_EMIT applicationsPathError(path);
        return;
    }
    m_config->group(QStringLiteral("General"))
             .writeEntry(QStringLiteral("applicationsPath"), path);
    m_config->sync();
    Q_EMIT applicationsPathChanged();
}

bool AppSettings::showDisclaimer() const
{
    return m_config->group(QStringLiteral("General"))
               .readEntry(QStringLiteral("showDisclaimer"), true);
}

void AppSettings::setShowDisclaimer(bool enabled)
{
    if (showDisclaimer() == enabled)
        return;
    m_config->group(QStringLiteral("General"))
             .writeEntry(QStringLiteral("showDisclaimer"), enabled);
    m_config->sync();
    Q_EMIT showDisclaimerChanged();
}

bool AppSettings::showNotifications() const
{
    return m_config->group(QStringLiteral("General"))
               .readEntry(QStringLiteral("showNotifications"), true);
}

void AppSettings::setShowNotifications(bool enabled)
{
    if (showNotifications() == enabled)
        return;
    m_config->group(QStringLiteral("General"))
             .writeEntry(QStringLiteral("showNotifications"), enabled);
    m_config->sync();
    Q_EMIT showNotificationsChanged();
}

int AppSettings::updateFrequency() const
{
    return m_config->group(QStringLiteral("Updates"))
               .readEntry(QStringLiteral("updateFrequency"), 1); // 1 = Daily
}

void AppSettings::setUpdateFrequency(int frequency)
{
    if (updateFrequency() == frequency)
        return;
    m_config->group(QStringLiteral("Updates"))
             .writeEntry(QStringLiteral("updateFrequency"), frequency);
    m_config->sync();
    Q_EMIT updateFrequencyChanged();
}

int AppSettings::customUpdateDays() const
{
    return m_config->group(QStringLiteral("Updates"))
               .readEntry(QStringLiteral("customUpdateDays"), 7);
}

void AppSettings::setCustomUpdateDays(int days)
{
    if (customUpdateDays() == days)
        return;
    m_config->group(QStringLiteral("Updates"))
             .writeEntry(QStringLiteral("customUpdateDays"), days);
    m_config->sync();
    Q_EMIT customUpdateDaysChanged();
}
