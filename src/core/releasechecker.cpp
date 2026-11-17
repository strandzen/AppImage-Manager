// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#include "releasechecker.h"
#include "appimageinfo.h"
#include "updatesource.h"

#include <KLocalizedString>

#include <QDateTime>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSysInfo>
#include <QVersionNumber>
#include <QXmlStreamReader>
#include "logging.h"

// A failed request in words; `repository`: a release source (else a direct download link).
QString ReleaseChecker::describeReplyError(QNetworkReply *reply, bool repository)
{
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status == 404)
        return repository ? i18n("Repository not found") : i18n("Download link not found");
    if (status == 403 || status == 429)
        return i18n("The server's request limit is reached; try again later");
    switch (reply->error()) {
    case QNetworkReply::HostNotFoundError:
    case QNetworkReply::TimeoutError:
    case QNetworkReply::OperationCanceledError: // transfer timeout
    case QNetworkReply::TemporaryNetworkFailureError:
    case QNetworkReply::NetworkSessionFailedError:
    case QNetworkReply::UnknownNetworkError:
        return i18n("No connection");
    default:
        return reply->errorString();
    }
}

ReleaseChecker::ReleaseChecker(QNetworkAccessManager *nam, QObject *parent)
    : QObject(parent)
    , m_nam(nam)
{
}

void ReleaseChecker::check(const QString &filePath, const QString &embeddedUpdateInfo,
                           const QString &currentVersion)
{
    const UpdateSource source = loadUpdateSource(filePath);
    const QString info = effectiveUpdateInfo(embeddedUpdateInfo, source);
    m_parts = info.split(QLatin1Char('|'));
    m_filePath = filePath;
    m_fileName = QFileInfo(filePath).fileName();
    m_currentVersion = currentVersion;
    m_installedTag = source.installedTag;
    const QString kind = m_parts.value(0);

    if (kind == QLatin1String("gh-releases-zsync") && m_parts.size() >= 3) {
        // /releases/latest never returns a pre-release; the list does, newest first.
        checkGithubFeedFirst(QUrl(source.prerelease
            ? QStringLiteral("https://api.github.com/repos/%1/%2/releases?per_page=10").arg(m_parts.at(1), m_parts.at(2))
            : QStringLiteral("https://api.github.com/repos/%1/%2/releases/latest").arg(m_parts.at(1), m_parts.at(2))));
    } else if (kind == QLatin1String("forgejo-releases") && m_parts.size() >= 4) {
        // Forgejo/Gitea mirror GitHub's release JSON (tag_name, assets[].browser_download_url).
        const QString base = QStringLiteral("https://%1/api/v1/repos/%2/%3/releases").arg(m_parts.at(1), m_parts.at(2), m_parts.at(3));
        checkReleases(QUrl(source.prerelease ? base + QStringLiteral("?limit=10") : base + QStringLiteral("/latest")), false);
    } else if (kind == QLatin1String("direct") && m_parts.size() >= 2) {
        checkDirect(QUrl(info.section(QLatin1Char('|'), 1)));
    } else {
        m_error = i18n("Unsupported update information");
        Q_EMIT checkFinished(Result::Failed, {}, {});
    }
}

void ReleaseChecker::checkReleases(const QUrl &url, bool github)
{
    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", "AppImageManager-UpdateCheck/1.0");
    request.setRawHeader("Accept", github ? "application/vnd.github+json" : "application/json");
    request.setTransferTimeout(10'000);

    QNetworkReply *reply = m_nam->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, github]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            m_error = describeReplyError(reply, true);
            if (github) {
                qCDebug(AIM_LOG) << "GitHub API check failed (" << reply->errorString() << "). Falling back to Atom RSS feed...";
                checkAtomFeed();
            } else {
                qCWarning(AIM_LOG) << "Release check failed for" << m_fileName << ":" << reply->errorString();
                Q_EMIT checkFinished(Result::NetworkFailed, {}, {});
            }
            return;
        }

        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QJsonObject release;
        if (doc.isObject()) {
            release = doc.object();
        } else {
            for (const QJsonValue &val : doc.array()) {
                if (!val.toObject().value(QLatin1String("draft")).toBool()) {
                    release = val.toObject();
                    break;
                }
            }
        }

        const QString tag = release.value(QLatin1String("tag_name")).toString();
        if (tag.isEmpty()) {
            m_error = i18n("No releases found");
            Q_EMIT checkFinished(Result::Failed, {}, {});
            return;
        }

        const QString pattern = github ? m_parts.value(4) : QString();
        const QJsonArray assets = release.value(QLatin1String("assets")).toArray();
        QString url = pickAsset(assets, pattern, m_fileName, QSysInfo::currentCpuArchitecture());
        if (url.isEmpty() && !pattern.isEmpty()) // stale pattern: fall back to name heuristics
            url = pickAsset(assets, {}, m_fileName, QSysInfo::currentCpuArchitecture());
        finishWithTag(tag, url);
    });
}

bool ReleaseChecker::isNonBinaryContentType(const QString &contentType)
{
    const QString type = contentType.section(QLatin1Char(';'), 0, 0).trimmed().toLower();
    return type.startsWith(QLatin1String("text/"))
        || type == QLatin1String("application/json")
        || type == QLatin1String("application/xml")
        || type == QLatin1String("application/xhtml+xml");
}

// A direct download has no version; the server's Last-Modified (or ETag) identifies the build.
// The marker doubles as the shown "version" and is stored as installedTag after an update.
void ReleaseChecker::checkDirect(const QUrl &url)
{
    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", "AppImageManager-UpdateCheck/1.0");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(10'000);

    QNetworkReply *reply = m_nam->head(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, url]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            m_error = describeReplyError(reply, false);
            qCWarning(AIM_LOG) << "Direct download check failed for" << m_fileName << ":" << reply->errorString();
            Q_EMIT checkFinished(Result::NetworkFailed, {}, {});
            return;
        }

        const QString contentType = reply->header(QNetworkRequest::ContentTypeHeader).toString();
        if (isNonBinaryContentType(contentType)) {
            qCWarning(AIM_LOG) << "The update URL for" << m_fileName << "returned" << contentType
                               << "instead of an AppImage. Set a direct download URL.";
            m_error = i18n("The link leads to a web page, not an AppImage");
            Q_EMIT checkFinished(Result::Failed, {}, {});
            return;
        }

        const QDateTime modified = reply->header(QNetworkRequest::LastModifiedHeader).toDateTime().toUTC();
        QString marker;
        if (modified.isValid()) {
            marker = modified.toString(QStringLiteral("yyyy-MM-dd HH:mm"));
        } else {
            QString etag = QString::fromLatin1(reply->rawHeader("ETag"));
            etag.remove(QStringLiteral("W/")).remove(QLatin1Char('"'));
            if (!etag.isEmpty())
                marker = QStringLiteral("build ") + etag.left(8);
        }
        if (marker.isEmpty()) {
            qCWarning(AIM_LOG) << "The update URL for" << m_fileName << "sends neither Last-Modified nor ETag; cannot tell builds apart.";
            m_error = i18n("The server does not say when the file changed");
            Q_EMIT checkFinished(Result::Failed, {}, {});
            return;
        }

        bool newer;
        if (!m_installedTag.isEmpty()) {
            newer = marker != m_installedTag;
        } else {
            // Never updated from this URL: compare with the local file when possible.
            const QFileInfo local(m_filePath);
            newer = !(modified.isValid() && local.exists() && local.lastModified().toUTC() >= modified);
        }

        if (newer)
            Q_EMIT checkFinished(Result::UpdateAvailable, marker, url.toString());
        else
            Q_EMIT checkFinished(Result::UpToDate, {}, {});
    });
}

bool ReleaseChecker::isNewerThanInstalled(const QString &tag) const
{
    const QString remoteVer = normalizeVersion(tag);
    const QString localVer = normalizeVersion(m_currentVersion);
    if (!m_installedTag.isEmpty() && normalizeVersion(m_installedTag) == remoteVer)
        return false; // this exact release was installed by our last update
    if (QVersionNumber::fromString(localVer).isNull())
        return true;  // installed version unknown; installedTag tracks it after one update
    return isNewerVersion(remoteVer, localVer);
}

void ReleaseChecker::checkGithubFeedFirst(const QUrl &apiUrl)
{
    QNetworkRequest request(QUrl(QStringLiteral("https://github.com/%1/%2/releases.atom").arg(m_parts.at(1), m_parts.at(2))));
    request.setRawHeader("User-Agent", "AppImageManager-UpdateCheck/1.0");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(10'000);

    QNetworkReply *reply = m_nam->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, apiUrl]() {
        reply->deleteLater();
        const QString tag = reply->error() == QNetworkReply::NoError ? feedNewestTag(reply->readAll()) : QString();
        // Nothing newer in the feed: up to date without an API request. With pre-releases off,
        // a newer pre-release in the feed still goes to the API, which then answers correctly.
        if (!tag.isEmpty() && !isNewerThanInstalled(tag)) {
            Q_EMIT checkFinished(Result::UpToDate, {}, {});
            return;
        }
        checkReleases(apiUrl, true);
    });
}

void ReleaseChecker::finishWithTag(const QString &tag, const QString &url)
{
    const QString remoteVer = normalizeVersion(tag);
    if (!isNewerThanInstalled(tag)) {
        Q_EMIT checkFinished(Result::UpToDate, {}, {});
        return;
    }
    if (url.isEmpty()) {
        qCDebug(AIM_LOG) << "Release" << tag << "has no matching AppImage asset for" << m_fileName;
        // After a failed API request (e.g. its limit), the Atom fallback knows only the tag:
        // that earlier reason is the useful one.
        if (m_error.isEmpty())
            m_error = i18n("No AppImage for this computer in release %1", tag);
        Q_EMIT checkFinished(Result::Failed, {}, {});
        return;
    }
    Q_EMIT checkFinished(Result::UpdateAvailable, remoteVer, url);
}

void ReleaseChecker::checkAtomFeed()
{
    const QString url = QStringLiteral("https://github.com/%1/%2/releases.atom")
                            .arg(m_parts.at(1), m_parts.at(2));

    QNetworkRequest request((QUrl(url)));
    request.setRawHeader("User-Agent", "AppImageManager-UpdateCheck/1.0");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(10'000);

    QNetworkReply *reply = m_nam->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            m_error = describeReplyError(reply, true);
            qCWarning(AIM_LOG) << "GitHub Atom feed fallback also failed:" << reply->errorString();
            Q_EMIT checkFinished(Result::NetworkFailed, {}, {});
            return;
        }

        const QString latestTag = feedNewestTag(reply->readAll());

        if (latestTag.isEmpty()) {
            m_error = i18n("No releases found");
            qCWarning(AIM_LOG) << "GitHub Atom feed fallback: could not find latest release tag in feed";
            Q_EMIT checkFinished(Result::Failed, {}, {});
            return;
        }

        // Construct standard zsync URL if filename pattern exists (usually parts.at(4))
        // Format of gh-releases-zsync: gh-releases-zsync|owner|repo|latest|pattern
        QString zsyncUrl;
        if (m_parts.size() >= 5 && !m_parts.at(4).isEmpty()) {
            QString pattern = m_parts.at(4);
            if (pattern.contains(QLatin1Char('*'))) {
                pattern.replace(QLatin1Char('*'), latestTag);
            }
            zsyncUrl = QStringLiteral("https://github.com/%1/%2/releases/download/%3/%4")
                           .arg(m_parts.at(1), m_parts.at(2), latestTag, pattern);
        }

        qCDebug(AIM_LOG) << "GitHub Atom feed fallback: latest tag:" << latestTag << "zsync URL:" << zsyncUrl;
        finishWithTag(latestTag, zsyncUrl);
    });
}

QString ReleaseChecker::feedNewestTag(const QByteArray &atom)
{
    QXmlStreamReader xml(atom);
    QString id, title;
    while (!xml.atEnd()) {
        xml.readNext();
        if (!xml.isStartElement() || xml.name() != QLatin1String("entry"))
            continue;
        // The first entry is the newest release.
        while (xml.readNextStartElement()) {
            if (xml.name() == QLatin1String("link")) {
                const QString href = xml.attributes().value(QLatin1String("href")).toString();
                const qsizetype at = href.indexOf(QLatin1String("/releases/tag/"));
                if (at >= 0)
                    return QUrl::fromPercentEncoding(href.mid(at + 14).toUtf8());
                xml.skipCurrentElement();
            } else if (xml.name() == QLatin1String("id")) {
                id = xml.readElementText().trimmed();
            } else if (xml.name() == QLatin1String("title")) {
                title = xml.readElementText().trimmed();
            } else {
                xml.skipCurrentElement();
            }
        }
        if (id.contains(QLatin1Char('/')))
            return id.section(QLatin1Char('/'), -1);
        return title;
    }
    return {};
}

// ── Asset selection (ported from Shelly's selectReleaseAssetUrl) ─────────────

// Lower-cased file name without extension and without version tokens, so
// "Foo-1.2.3-x86_64.AppImage" and "foo-1.3.0-x86_64.AppImage.zsync" compare equal.
static QString nameKey(QString name)
{
    if (name.endsWith(QLatin1String(".zsync"), Qt::CaseInsensitive))
        name.chop(6);
    if (name.endsWith(QLatin1String(".appimage"), Qt::CaseInsensitive))
        name.chop(9);
    static const QRegularExpression separators(QStringLiteral("[-_\\s]+"));
    static const QRegularExpression version(QStringLiteral("^[vV]?\\d+(\\.\\d+)+$"));
    QStringList kept;
    for (const QString &token : name.split(separators, Qt::SkipEmptyParts)) {
        if (!version.match(token).hasMatch())
            kept.append(token.toLower());
    }
    return kept.join(QLatin1Char('-'));
}

bool ReleaseChecker::matchesArchitecture(const QString &assetName, const QString &cpuArch)
{
    static const QList<QStringList> groups = {
        { QStringLiteral("x86_64"), QStringLiteral("amd64"), QStringLiteral("x64") },
        { QStringLiteral("aarch64"), QStringLiteral("arm64"), QStringLiteral("armv8") },
        { QStringLiteral("i386"), QStringLiteral("i686"), QStringLiteral("x86") },
        { QStringLiteral("armhf"), QStringLiteral("armv7l"), QStringLiteral("arm") },
    };
    // Whole tokens only: "arm" must not match "Swarm" or "arm64", nor "x86" match "x86_64".
    const auto mentions = [&assetName](const QString &alias) {
        const QRegularExpression token(QStringLiteral("(?<![a-z0-9])%1(?![a-z0-9]|_64)")
                                           .arg(QRegularExpression::escape(alias)),
                                       QRegularExpression::CaseInsensitiveOption);
        return token.match(assetName).hasMatch();
    };
    QStringList target = { cpuArch };
    for (const QStringList &group : groups) {
        if (group.contains(cpuArch))
            target = group;
    }
    for (const QString &alias : std::as_const(target)) {
        if (mentions(alias))
            return true;
    }
    // Names that mention no architecture at all are assumed to fit.
    for (const QStringList &group : groups) {
        for (const QString &alias : group) {
            if (!target.contains(alias) && mentions(alias))
                return false;
        }
    }
    return true;
}

QString ReleaseChecker::pickAsset(const QJsonArray &assets, const QString &pattern,
                                        const QString &localFileName, const QString &cpuArch)
{
    const QRegularExpression glob = pattern.isEmpty() ? QRegularExpression()
        : QRegularExpression(QRegularExpression::wildcardToRegularExpression(pattern),
                             QRegularExpression::CaseInsensitiveOption);
    const QString localKey = nameKey(localFileName);

    for (const QLatin1String suffix : { QLatin1String(".zsync"), QLatin1String(".appimage") }) {
        QStringList urls;
        QStringList keys;
        for (const QJsonValue &val : assets) {
            const QJsonObject asset = val.toObject();
            const QString name = asset.value(QLatin1String("name")).toString();
            const QString url = asset.value(QLatin1String("browser_download_url")).toString();
            if (url.isEmpty() || !name.endsWith(suffix, Qt::CaseInsensitive))
                continue;
            // A pattern names the .zsync file; its .AppImage sibling matches too.
            if (!pattern.isEmpty() && !glob.match(name).hasMatch()
                && !glob.match(name + QLatin1String(".zsync")).hasMatch())
                continue;
            if (!matchesArchitecture(name, cpuArch))
                continue;
            urls.append(url);
            keys.append(nameKey(name));
        }
        if (urls.size() == 1)
            return urls.first();
        if (urls.size() > 1) {
            const int first = keys.indexOf(localKey);
            if (first >= 0 && keys.lastIndexOf(localKey) == first)
                return urls.at(first);
            return {}; // ambiguous: several builds and none clearly ours
        }
    }
    return {};
}
