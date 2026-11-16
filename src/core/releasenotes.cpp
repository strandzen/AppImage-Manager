// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#include "releasenotes.h"
#include "appimageinfo.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

QUrl releaseListUrl(const QString &updateInfo)
{
    const QStringList parts = updateInfo.split(QLatin1Char('|'));
    const QString kind = parts.value(0);
    if (kind == QLatin1String("gh-releases-zsync") && parts.size() >= 3)
        return QUrl(QStringLiteral("https://api.github.com/repos/%1/%2/releases?per_page=20").arg(parts.at(1), parts.at(2)));
    if (kind == QLatin1String("forgejo-releases") && parts.size() >= 4)
        return QUrl(QStringLiteral("https://%1/api/v1/repos/%2/%3/releases?limit=20").arg(parts.at(1), parts.at(2), parts.at(3)));
    return {};
}

QString cleanReleaseBody(const QString &markdown)
{
    static const QRegularExpression image(QStringLiteral("!\\[[^\\]]*\\]\\([^)]*\\)"));
    static const QRegularExpression htmlImage(QStringLiteral("<img[^>]*>"), QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression comment(QStringLiteral("<!--.*?-->"), QRegularExpression::DotMatchesEverythingOption);
    static const QRegularExpression blankLines(QStringLiteral("\\n{3,}"));
    QString text = markdown;
    text.remove(QLatin1Char('\r'));
    text.remove(comment);
    text.remove(image);
    text.remove(htmlImage);
    text.replace(blankLines, QStringLiteral("\n\n"));
    return text.trimmed();
}

QList<ReleaseNote> pickReleaseNotes(const QByteArray &json, const QString &installedVersion,
                                    const QString &installedTag, bool prereleases)
{
    QList<ReleaseNote> all;
    for (const QJsonValue &value : QJsonDocument::fromJson(json).array()) {
        const QJsonObject release = value.toObject();
        if (release.value(QLatin1String("draft")).toBool())
            continue;
        const bool pre = release.value(QLatin1String("prerelease")).toBool();
        if (pre && !prereleases)
            continue;
        all.append({ release.value(QLatin1String("tag_name")).toString(),
                     release.value(QLatin1String("name")).toString(),
                     cleanReleaseBody(release.value(QLatin1String("body")).toString()),
                     release.value(QLatin1String("html_url")).toString(),
                     QDateTime::fromString(release.value(QLatin1String("published_at")).toString(), Qt::ISODate),
                     pre });
    }
    const QString installed = normalizeVersion(installedVersion);
    QList<ReleaseNote> newer;
    for (const ReleaseNote &note : std::as_const(all)) {
        if (!installedTag.isEmpty() && note.tag == installedTag)
            break;
        if (installed.isEmpty() || !isNewerVersion(normalizeVersion(note.tag), installed))
            break;
        newer.append(note);
    }
    if (newer.isEmpty() && !all.isEmpty())
        newer.append(all.first());
    return newer;
}
