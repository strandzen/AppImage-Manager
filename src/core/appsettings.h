// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#pragma once

#include "appimagemanager_qml_export.h"

#include <QObject>
#include <QWindow>  // Qt 6.11 moc needs the full type for Q_INVOKABLE pointer args
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;

class APPIMAGEMANAGER_EXPORT AppSettings : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QString applicationsPath  READ applicationsPath  WRITE setApplicationsPath  NOTIFY applicationsPathChanged)
    Q_PROPERTY(bool    showDisclaimer    READ showDisclaimer    WRITE setShowDisclaimer    NOTIFY showDisclaimerChanged)
    Q_PROPERTY(bool    showNotifications READ showNotifications WRITE setShowNotifications NOTIFY showNotificationsChanged)
    Q_PROPERTY(int     updateFrequency   READ updateFrequency   WRITE setUpdateFrequency   NOTIFY updateFrequencyChanged)
    Q_PROPERTY(int     customUpdateDays  READ customUpdateDays  WRITE setCustomUpdateDays  NOTIFY customUpdateDaysChanged)
    Q_PROPERTY(int     manageIconSize    READ manageIconSize    WRITE setManageIconSize    NOTIFY manageIconSizeChanged)
    Q_PROPERTY(bool    watchDownloads    READ watchDownloads    WRITE setWatchDownloads    NOTIFY watchDownloadsChanged)
    Q_PROPERTY(bool    showInstallBox    READ showInstallBox    WRITE setShowInstallBox    NOTIFY showInstallBoxChanged)
    Q_PROPERTY(bool    accentBorders     READ accentBorders     WRITE setAccentBorders     NOTIFY accentBordersChanged)
    Q_PROPERTY(QString qtVersion READ qtVersion CONSTANT)
    Q_PROPERTY(QString kdeFrameworksVersion READ kdeFrameworksVersion CONSTANT)
    Q_PROPERTY(QString libappimageVersion READ libappimageVersion CONSTANT)
    Q_PROPERTY(QString appVersion READ appVersion CONSTANT)

public:
    // QML singleton factory — always returns the same C++ instance across all engines.
    static AppSettings *create(QQmlEngine *, QJSEngine *) { return instance(); }
    static AppSettings *instance();

    QString qtVersion() const;
    QString kdeFrameworksVersion() const;
    QString libappimageVersion() const;
    QString appVersion() const;

    Q_INVOKABLE void copyToClipboard(const QString &text);

    QString applicationsPath() const;
    void setApplicationsPath(const QString &path);

    bool showDisclaimer() const;
    void setShowDisclaimer(bool enabled);

    bool showNotifications() const;
    void setShowNotifications(bool enabled);

    int updateFrequency() const;
    void setUpdateFrequency(int frequency);

    int customUpdateDays() const;
    void setCustomUpdateDays(int days);

    int manageIconSize() const;
    void setManageIconSize(int size);

    bool watchDownloads() const;
    void setWatchDownloads(bool enabled);

    bool showInstallBox() const;
    void setShowInstallBox(bool enabled);

    bool accentBorders() const;
    void setAccentBorders(bool enabled);

    Q_INVOKABLE void openFolderPicker(QWindow *parent = nullptr);
    Q_INVOKABLE void resetToDefaults();

Q_SIGNALS:
    void applicationsPathChanged();
    void showDisclaimerChanged();
    void showNotificationsChanged();
    void updateFrequencyChanged();
    void customUpdateDaysChanged();
    void manageIconSizeChanged();
    void watchDownloadsChanged();
    void showInstallBoxChanged();
    void accentBordersChanged();
    void applicationsPathError(const QString &message);

private:
    explicit AppSettings(QObject *parent = nullptr);
};
