// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#include "githubreleasechecker.h"
#include "appimageinfo.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

GitHubReleaseChecker::GitHubReleaseChecker(QNetworkAccessManager *nam, QObject *parent)
    : QObject(parent)
    , m_nam(nam)
{
}

void GitHubReleaseChecker::check(const QString &updateInfo, const QString &currentVersion)
{
    const QStringList parts = updateInfo.split(QLatin1Char('|'));
    if (parts.size() < 3) {
        Q_EMIT failed();
        return;
    }

    const QString url = QStringLiteral("https://api.github.com/repos/%1/%2/releases/latest")
                            .arg(parts.at(1), parts.at(2));

    QNetworkRequest request((QUrl(url)));
    request.setRawHeader("User-Agent", "AppImageManager-UpdateCheck/1.0");
    request.setTransferTimeout(10'000);

    const QString localVer = normalizeVersion(currentVersion);

    QNetworkReply *reply = m_nam->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, localVer]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            Q_EMIT networkFailed();
            return;
        }

        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        if (!doc.isObject()) {
            Q_EMIT failed();
            return;
        }

        const QString tag = doc.object().value(QLatin1String("tag_name")).toString();
        if (tag.isEmpty()) {
            Q_EMIT failed();
            return;
        }

        const QString remoteVer = normalizeVersion(tag);
        if (!isNewerVersion(remoteVer, localVer)) {
            Q_EMIT upToDate();
            return;
        }

        QString zsyncUrl;
        const QJsonArray assets = doc.object().value(QLatin1String("assets")).toArray();
        for (const QJsonValue &val : assets) {
            const QJsonObject asset = val.toObject();
            if (asset.value(QLatin1String("name")).toString().endsWith(QLatin1String(".zsync"))) {
                zsyncUrl = asset.value(QLatin1String("browser_download_url")).toString();
                break;
            }
        }

        Q_EMIT updateAvailable(remoteVer, zsyncUrl);
    });
}
