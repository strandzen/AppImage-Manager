// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#include "appsettings.h"
#include "version.h"

#include <KConfigGroup>
#include <KCoreAddons>

#include <QDir>
#include <QFileDialog>
#include <QWindow>
#include <QGuiApplication>
#include <QClipboard>
#include <KLocalizedString>

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
    Q_EMIT customUpdateDaysChanged();
}

int AppSettings::manageIconSize() const
{
    return m_config->group(QStringLiteral("Appearance"))
               .readEntry(QStringLiteral("manageIconSize"), 2); // 2 = Large
}

void AppSettings::setManageIconSize(int size)
{
    if (manageIconSize() == size)
        return;
    m_config->group(QStringLiteral("Appearance"))
             .writeEntry(QStringLiteral("manageIconSize"), size);
    Q_EMIT manageIconSizeChanged();
}

bool AppSettings::watchDownloads() const
{
    return m_config->group(QStringLiteral("Behavior"))
               .readEntry(QStringLiteral("watchDownloads"), true);
}

void AppSettings::setWatchDownloads(bool enabled)
{
    if (watchDownloads() == enabled)
        return;
    m_config->group(QStringLiteral("Behavior"))
             .writeEntry(QStringLiteral("watchDownloads"), enabled);
    Q_EMIT watchDownloadsChanged();
}

bool AppSettings::showInstallBox() const
{
    return m_config->group(QStringLiteral("Appearance"))
               .readEntry(QStringLiteral("showInstallBox"), true);
}

void AppSettings::setShowInstallBox(bool enabled)
{
    if (showInstallBox() == enabled)
        return;
    m_config->group(QStringLiteral("Appearance"))
             .writeEntry(QStringLiteral("showInstallBox"), enabled);
    Q_EMIT showInstallBoxChanged();
}

void AppSettings::openFolderPicker(QWindow *parent)
{
    QFileDialog dialog(nullptr, i18n("Select Applications Folder"), applicationsPath());
    dialog.setFileMode(QFileDialog::Directory);
    dialog.setOption(QFileDialog::ShowDirsOnly, true);
    dialog.setOption(QFileDialog::DontResolveSymlinks, true);

    if (parent) {
        // winId() forces the native window backing to exist so windowHandle()
        // returns non-null; QWidget::create() is protected since Qt 6.11.
        dialog.winId();
        if (dialog.windowHandle()) {
            dialog.windowHandle()->setTransientParent(parent);
        }
    }

    if (dialog.exec() == QDialog::Accepted) {
        const QStringList selected = dialog.selectedFiles();
        if (selected.isEmpty())
            return;
        const QString dir = selected.first();
        if (!dir.isEmpty())
            setApplicationsPath(dir);
    }
}

QString AppSettings::qtVersion() const
{
    return QString::fromUtf8(qVersion());
}

QString AppSettings::kdeFrameworksVersion() const
{
    return KCoreAddons::versionString();
}

QString AppSettings::libappimageVersion() const
{
    return QStringLiteral("libappimage");
}

QString AppSettings::appVersion() const
{
    return QStringLiteral(APPIMAGEMANAGER_VERSION_STRING);
}

void AppSettings::copyToClipboard(const QString &text)
{
    QGuiApplication::clipboard()->setText(text);
}

bool AppSettings::accentBorders() const
{
    return m_config->group(QStringLiteral("Appearance"))
               .readEntry(QStringLiteral("accentBorders"), true);
}

void AppSettings::setAccentBorders(bool enabled)
{
    if (accentBorders() == enabled)
        return;
    m_config->group(QStringLiteral("Appearance"))
             .writeEntry(QStringLiteral("accentBorders"), enabled);
    m_config->sync();
    Q_EMIT accentBordersChanged();
}

void AppSettings::resetToDefaults()
{
    QString defaultPath(QDir::homePath());
    defaultPath += QStringLiteral("/Applications");

    setApplicationsPath(defaultPath);
    setShowDisclaimer(true);
    setShowNotifications(true);
    setUpdateFrequency(1);
    setCustomUpdateDays(7);
    setManageIconSize(2);
    setWatchDownloads(true);
    setShowInstallBox(true);
    setAccentBorders(true);
}
