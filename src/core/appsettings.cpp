// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#include "appsettings.h"
#include "version.h"
#include "appimagemanagersettings.h"

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
{
}

QString AppSettings::applicationsPath() const
{
    return AppImageManagerSettings::self()->applicationsPath();
}

void AppSettings::setApplicationsPath(const QString &path)
{
    if (applicationsPath() == path)
        return;
    if (!QDir().mkpath(path)) {
        Q_EMIT applicationsPathError(path);
        return;
    }
    AppImageManagerSettings::self()->setApplicationsPath(path);
    AppImageManagerSettings::self()->save();
    Q_EMIT applicationsPathChanged();
}

bool AppSettings::showDisclaimer() const
{
    return AppImageManagerSettings::self()->showDisclaimer();
}

void AppSettings::setShowDisclaimer(bool enabled)
{
    if (showDisclaimer() == enabled)
        return;
    AppImageManagerSettings::self()->setShowDisclaimer(enabled);
    AppImageManagerSettings::self()->save();
    Q_EMIT showDisclaimerChanged();
}

bool AppSettings::showNotifications() const
{
    return AppImageManagerSettings::self()->showNotifications();
}

void AppSettings::setShowNotifications(bool enabled)
{
    if (showNotifications() == enabled)
        return;
    AppImageManagerSettings::self()->setShowNotifications(enabled);
    AppImageManagerSettings::self()->save();
    Q_EMIT showNotificationsChanged();
}

int AppSettings::updateFrequency() const
{
    return AppImageManagerSettings::self()->updateFrequency();
}

void AppSettings::setUpdateFrequency(int frequency)
{
    if (updateFrequency() == frequency)
        return;
    AppImageManagerSettings::self()->setUpdateFrequency(frequency);
    AppImageManagerSettings::self()->save();
    Q_EMIT updateFrequencyChanged();
}

int AppSettings::customUpdateDays() const
{
    return AppImageManagerSettings::self()->customUpdateDays();
}

void AppSettings::setCustomUpdateDays(int days)
{
    if (customUpdateDays() == days)
        return;
    AppImageManagerSettings::self()->setCustomUpdateDays(days);
    AppImageManagerSettings::self()->save();
    Q_EMIT customUpdateDaysChanged();
}

int AppSettings::manageIconSize() const
{
    return AppImageManagerSettings::self()->manageIconSize();
}

void AppSettings::setManageIconSize(int size)
{
    if (manageIconSize() == size)
        return;
    AppImageManagerSettings::self()->setManageIconSize(size);
    AppImageManagerSettings::self()->save();
    Q_EMIT manageIconSizeChanged();
}

bool AppSettings::watchDownloads() const
{
    return AppImageManagerSettings::self()->watchDownloads();
}

void AppSettings::setWatchDownloads(bool enabled)
{
    if (watchDownloads() == enabled)
        return;
    AppImageManagerSettings::self()->setWatchDownloads(enabled);
    AppImageManagerSettings::self()->save();
    Q_EMIT watchDownloadsChanged();
}

bool AppSettings::showInstallBox() const
{
    return AppImageManagerSettings::self()->showInstallBox();
}

void AppSettings::setShowInstallBox(bool enabled)
{
    if (showInstallBox() == enabled)
        return;
    AppImageManagerSettings::self()->setShowInstallBox(enabled);
    AppImageManagerSettings::self()->save();
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
    return AppImageManagerSettings::self()->accentBorders();
}

void AppSettings::setAccentBorders(bool enabled)
{
    if (accentBorders() == enabled)
        return;
    AppImageManagerSettings::self()->setAccentBorders(enabled);
    AppImageManagerSettings::self()->save();
    Q_EMIT accentBordersChanged();
}

void AppSettings::resetToDefaults()
{
    AppImageManagerSettings::self()->setDefaults();
    AppImageManagerSettings::self()->save();

    // Emit all signals so QML updates
    Q_EMIT applicationsPathChanged();
    Q_EMIT showDisclaimerChanged();
    Q_EMIT showNotificationsChanged();
    Q_EMIT updateFrequencyChanged();
    Q_EMIT customUpdateDaysChanged();
    Q_EMIT manageIconSizeChanged();
    Q_EMIT watchDownloadsChanged();
    Q_EMIT showInstallBoxChanged();
    Q_EMIT accentBordersChanged();
}
