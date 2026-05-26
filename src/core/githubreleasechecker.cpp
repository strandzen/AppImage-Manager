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
#include <QXmlStreamReader>
#include "logging.h"

GitHubReleaseChecker::GitHubReleaseChecker(QNetworkAccessManager *nam, QObject *parent)
    : QObject(parent)
    , m_nam(nam)
{
}

void GitHubReleaseChecker::check(const QString &updateInfo, const QString &currentVersion)
{
    m_updateInfo = updateInfo;
    m_currentVersion = currentVersion;

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
            qCDebug(AIM_LOG) << "GitHub API check failed (" << reply->errorString() << "). Falling back to Atom RSS feed...";
            checkAtomFeed();
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

void GitHubReleaseChecker::checkAtomFeed()
{
    const QStringList parts = m_updateInfo.split(QLatin1Char('|'));
    if (parts.size() < 3) {
        Q_EMIT failed();
        return;
    }

    const QString url = QStringLiteral("https://github.com/%1/%2/releases.atom")
                            .arg(parts.at(1), parts.at(2));

    QNetworkRequest request((QUrl(url)));
    request.setRawHeader("User-Agent", "AppImageManager-UpdateCheck/1.0");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(10'000);

    QNetworkReply *reply = m_nam->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, parts]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            qCWarning(AIM_LOG) << "GitHub Atom feed fallback also failed:" << reply->errorString();
            Q_EMIT networkFailed();
            return;
        }

        const QByteArray xmlData = reply->readAll();
        QXmlStreamReader xml(xmlData);

        QString latestTag;
        bool inEntry = false;

        while (!xml.atEnd() && !xml.hasError()) {
            const auto token = xml.readNext();

            if (token == QXmlStreamReader::StartElement) {
                const auto name = xml.name();
                if (name == QLatin1String("entry")) {
                    inEntry = true;
                } else if (name == QLatin1String("title") && inEntry) {
                    latestTag = xml.readElementText().trimmed();
                    break; // the first entry is the latest release!
                }
            } else if (token == QXmlStreamReader::EndElement) {
                if (xml.name() == QLatin1String("entry")) {
                    inEntry = false;
                }
            }
        }

        if (latestTag.isEmpty()) {
            qCWarning(AIM_LOG) << "GitHub Atom feed fallback: could not find latest release tag in feed";
            Q_EMIT failed();
            return;
        }

        const QString localVer = normalizeVersion(m_currentVersion);
        const QString remoteVer = normalizeVersion(latestTag);

        if (!isNewerVersion(remoteVer, localVer)) {
            Q_EMIT upToDate();
            return;
        }

        // Construct standard zsync URL if filename pattern exists (usually parts.at(4))
        // Format of gh-releases-zsync: gh-releases-zsync|owner|repo|latest|pattern
        QString zsyncUrl;
        if (parts.size() >= 5) {
            QString pattern = parts.at(4);
            if (pattern.contains(QLatin1Char('*'))) {
                pattern.replace(QLatin1Char('*'), latestTag);
            }
            zsyncUrl = QStringLiteral("https://github.com/%1/%2/releases/download/%3/%4")
                           .arg(parts.at(1), parts.at(2), latestTag, pattern);
        }

        qCDebug(AIM_LOG) << "GitHub Atom feed fallback: update available:" << remoteVer << "zsync URL:" << zsyncUrl;
        Q_EMIT updateAvailable(remoteVer, zsyncUrl);
    });
}
