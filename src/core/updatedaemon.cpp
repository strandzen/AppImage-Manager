// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#include "updatedaemon.h"
#include "appsettings.h"
#include "appimagereader.h"
#include <QDir>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <KNotification>
#include <KLocalizedString>
#include <QProcess>

UpdateDaemon::UpdateDaemon(QObject *parent)
    : QObject(parent)
    , m_timer(new QTimer(this))
    , m_networkManager(new QNetworkAccessManager(this))
{
    // Check every hour to see if we've passed the threshold
    m_timer->setInterval(60 * 60 * 1000); 
    connect(m_timer, &QTimer::timeout, this, &UpdateDaemon::checkUpdates);
}

void UpdateDaemon::start()
{
    // Do an initial check
    checkUpdates();
    m_timer->start();
}

void UpdateDaemon::checkUpdates()
{
    int freq = AppSettings::instance()->updateFrequency();
    if (freq == 0) return; // Never
    
    // NOTE: A production version would compare current time against a 
    // "lastCheckTime" stored in AppSettings and return early if not due.
    
    const QString dir = AppSettings::instance()->applicationsPath();
    const QStringList filters = { QStringLiteral("*.AppImage"), QStringLiteral("*.appimage") };
    const QFileInfoList files = QDir(dir).entryInfoList(filters, QDir::Files);

    for (const QFileInfo &fi : files) {
        AppImageInfo info = AppImageReader(fi.absoluteFilePath()).read();
        
        if (info.updateInfo.startsWith(QStringLiteral("gh-releases-zsync|"))) {
            QStringList parts = info.updateInfo.split(QLatin1Char('|'));
            if (parts.size() >= 3) {
                QString owner = parts.at(1);
                QString repo = parts.at(2);
                QString url = QStringLiteral("https://api.github.com/repos/%1/%2/releases/latest").arg(owner, repo);
                
                QNetworkRequest request((QUrl(url)));
                request.setRawHeader("User-Agent", "AppImageManager-UpdateCheck/1.0");
                
                QNetworkReply *reply = m_networkManager->get(request);
                connect(reply, &QNetworkReply::finished, this, [this, reply, info]() mutable {
                    reply->deleteLater();
                    if (reply->error() == QNetworkReply::NoError) {
                        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
                        if (doc.isObject()) {
                            QString tag = doc.object().value(QLatin1String("tag_name")).toString();
                            if (!tag.isEmpty()) {
                                QString versionStr = tag;
                                if (versionStr.startsWith(QLatin1Char('v')) || versionStr.startsWith(QLatin1Char('V'))) {
                                    versionStr.remove(0, 1);
                                }
                                QString currentVer = info.version;
                                if (currentVer.startsWith(QLatin1Char('v')) || currentVer.startsWith(QLatin1Char('V'))) {
                                    currentVer.remove(0, 1);
                                }
                                if (versionStr != currentVer) {
                                    KNotification *notification = new KNotification(QStringLiteral("updateAvailable"), KNotification::Persistent, this);
                                    notification->setTitle(i18n("Update Available"));
                                    notification->setText(i18n("An update is available for %1 (%2 -> %3).", info.cleanName.isEmpty() ? info.originalName : info.cleanName, currentVer, versionStr));
                                    notification->setIconName(info.appId.isEmpty() ? QStringLiteral("application-x-executable") : info.appId);
                                    auto action = notification->addDefaultAction(i18n("Open Manager"));
                                    connect(action, &KNotificationAction::activated, this, []() {
                                        QProcess::startDetached(QStringLiteral("appimagemanager"), {});
                                    });
                                    notification->sendEvent();
                                }
                            }
                        }
                    }
                });
            }
        }
    }
}
