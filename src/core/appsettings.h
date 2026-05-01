// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#pragma once

#include <QObject>
#include <QtQml/qqmlregistration.h>

#include <KSharedConfig>

class QQmlEngine;
class QJSEngine;

class AppSettings : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QString applicationsPath  READ applicationsPath  WRITE setApplicationsPath  NOTIFY applicationsPathChanged)
    Q_PROPERTY(bool    showDisclaimer    READ showDisclaimer    WRITE setShowDisclaimer    NOTIFY showDisclaimerChanged)
    Q_PROPERTY(bool    showNotifications READ showNotifications WRITE setShowNotifications NOTIFY showNotificationsChanged)
    Q_PROPERTY(int     updateFrequency   READ updateFrequency   WRITE setUpdateFrequency   NOTIFY updateFrequencyChanged)
    Q_PROPERTY(int     customUpdateDays  READ customUpdateDays  WRITE setCustomUpdateDays  NOTIFY customUpdateDaysChanged)

public:
    // QML singleton factory — always returns the same C++ instance across all engines.
    static AppSettings *create(QQmlEngine *, QJSEngine *) { return instance(); }
    static AppSettings *instance();

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

Q_SIGNALS:
    void applicationsPathChanged();
    void showDisclaimerChanged();
    void showNotificationsChanged();
    void updateFrequencyChanged();
    void customUpdateDaysChanged();
    void applicationsPathError(const QString &message);

private:
    explicit AppSettings(QObject *parent = nullptr);
    KSharedConfig::Ptr m_config;
};
