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

Q_SIGNALS:
    void applicationsPathChanged();
    void showDisclaimerChanged();
    void showNotificationsChanged();
    void applicationsPathError(const QString &message);

private:
    explicit AppSettings(QObject *parent = nullptr);
    KSharedConfig::Ptr m_config;
};
