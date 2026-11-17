// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#include "updatesource.h"

#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>

#include <QFileInfo>
#include <QRegularExpression>
#include <QUrl>

static KConfigGroup sourceGroup(const QString &filePath)
{
    KSharedConfig::Ptr config = KSharedConfig::openConfig(QStringLiteral("appimagemanagerrc"));
    config->reparseConfiguration();
    return config->group(QStringLiteral("UpdateSources")).group(QFileInfo(filePath).fileName());
}

QString updateSourceTypeName(UpdateSourceType type)
{
    switch (type) {
    case UpdateSourceType::GitHub:  return QStringLiteral("github");
    case UpdateSourceType::Forgejo: return QStringLiteral("forgejo");
    case UpdateSourceType::Zsync:   return QStringLiteral("zsync");
    case UpdateSourceType::Url:     return QStringLiteral("url");
    case UpdateSourceType::None:    break;
    }
    return {};
}

UpdateSourceType updateSourceTypeFromName(const QString &name)
{
    for (UpdateSourceType type : { UpdateSourceType::GitHub, UpdateSourceType::Forgejo,
                                   UpdateSourceType::Zsync, UpdateSourceType::Url }) {
        if (name == updateSourceTypeName(type))
            return type;
    }
    return UpdateSourceType::None;
}

UpdateSource loadUpdateSource(const QString &filePath)
{
    const KConfigGroup group = sourceGroup(filePath);
    UpdateSource source;
    source.type = updateSourceTypeFromName(group.readEntry("Type", QString()));
    source.value = group.readEntry("Value", QString());
    // Sources saved before the Type key existed were GitHub repositories.
    const QString legacyRepo = group.readEntry("Repo", QString());
    if (source.type == UpdateSourceType::None && !legacyRepo.isEmpty()) {
        source.type = UpdateSourceType::GitHub;
        source.value = legacyRepo;
    }
    if (source.value.isEmpty())
        source.type = UpdateSourceType::None;
    source.prerelease = group.readEntry("Prerelease", false);
    source.installedTag = group.readEntry("InstalledTag", QString());
    return source;
}

void saveUpdateSource(const QString &filePath, const UpdateSource &source)
{
    KConfigGroup group = sourceGroup(filePath);
    const UpdateSource old = loadUpdateSource(filePath);
    if (old.type != source.type || old.value != source.value)
        group.deleteEntry("InstalledTag");
    group.deleteEntry("Repo");
    if (source.type == UpdateSourceType::None || source.value.isEmpty()) {
        group.deleteEntry("Type");
        group.deleteEntry("Value");
        group.deleteEntry("Prerelease");
    } else {
        group.writeEntry("Type", updateSourceTypeName(source.type));
        group.writeEntry("Value", source.value);
        group.writeEntry("Prerelease", source.prerelease);
    }
    group.sync();
}

void saveInstalledTag(const QString &filePath, const QString &tag)
{
    KConfigGroup group = sourceGroup(filePath);
    group.writeEntry("InstalledTag", tag);
    group.sync();
}

static bool validRepoPart(const QString &part)
{
    static const QRegularExpression valid(QStringLiteral("^[A-Za-z0-9_.-]+$"));
    return valid.match(part).hasMatch() && part != QLatin1String(".") && part != QLatin1String("..");
}

// "owner/repo" from the first two parts of a path, or empty.
static QString repoFromParts(const QStringList &parts)
{
    if (parts.size() < 2)
        return {};
    QString repo = parts.at(1);
    if (repo.endsWith(QLatin1String(".git")))
        repo.chop(4);
    if (!validRepoPart(parts.at(0)) || !validRepoPart(repo))
        return {};
    return parts.at(0) + QLatin1Char('/') + repo;
}

QString parseGitHubRepo(const QString &input)
{
    QString path = input.trimmed();
    if (path.contains(QLatin1String("://"))) {
        const QUrl url(path);
        const QString host = url.host().toLower();
        if (host != QLatin1String("github.com") && host != QLatin1String("www.github.com"))
            return {};
        path = url.path();
    }
    const QStringList parts = path.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    // A bare "owner/repo" must be exactly two parts; a URL may continue
    // (e.g. /owner/repo/releases), so only its first two parts count.
    if (!input.contains(QLatin1String("://")) && parts.size() != 2)
        return {};
    return repoFromParts(parts);
}

UpdateSource parseUpdateSourceInput(const QString &input, UpdateSourceType forced)
{
    UpdateSource result;
    const QString text = input.trimmed();
    if (text.isEmpty())
        return result;

    auto make = [&result](UpdateSourceType type, const QString &value) {
        if (!value.isEmpty()) {
            result.type = type;
            result.value = value;
        }
        return result;
    };

    if (!text.contains(QLatin1String("://"))) {
        // Only a GitHub repository can be given without a host.
        if (forced == UpdateSourceType::None || forced == UpdateSourceType::GitHub)
            return make(UpdateSourceType::GitHub, parseGitHubRepo(text));
        return result;
    }

    const QUrl url(text);
    const QString scheme = url.scheme().toLower();
    if (!url.isValid() || url.host().isEmpty()
        || (scheme != QLatin1String("https") && scheme != QLatin1String("http")))
        return result;

    const QString host = url.host().toLower();
    const QString path = url.path();
    const QStringList parts = path.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    const QString forgejoRepo = repoFromParts(parts);
    const QString forgejoValue = forgejoRepo.isEmpty() ? QString() : host + QLatin1Char('/') + forgejoRepo;

    switch (forced) {
    case UpdateSourceType::GitHub:  return make(UpdateSourceType::GitHub, parseGitHubRepo(text));
    case UpdateSourceType::Forgejo: return make(UpdateSourceType::Forgejo, forgejoValue);
    case UpdateSourceType::Zsync:   return make(UpdateSourceType::Zsync, text);
    case UpdateSourceType::Url:     return make(UpdateSourceType::Url, text);
    case UpdateSourceType::None:    break;
    }

    if (path.endsWith(QLatin1String(".zsync"), Qt::CaseInsensitive))
        return make(UpdateSourceType::Zsync, text);
    if (path.endsWith(QLatin1String(".appimage"), Qt::CaseInsensitive))
        return make(UpdateSourceType::Url, text);
    if (host == QLatin1String("github.com") || host == QLatin1String("www.github.com"))
        return make(UpdateSourceType::GitHub, parseGitHubRepo(text));
    if (!url.hasQuery() && !forgejoValue.isEmpty())
        return make(UpdateSourceType::Forgejo, forgejoValue);
    return make(UpdateSourceType::Url, text);
}

QString effectiveUpdateInfo(const QString &embedded, const UpdateSource &source)
{
    switch (source.type) {
    case UpdateSourceType::GitHub:
        return QStringLiteral("gh-releases-zsync|%1|%2|latest|")
            .arg(source.value.section(QLatin1Char('/'), 0, 0), source.value.section(QLatin1Char('/'), 1, 1));
    case UpdateSourceType::Forgejo:
        return QStringLiteral("forgejo-releases|%1|%2|%3")
            .arg(source.value.section(QLatin1Char('/'), 0, 0), source.value.section(QLatin1Char('/'), 1, 1),
                 source.value.section(QLatin1Char('/'), 2, 2));
    case UpdateSourceType::Zsync:
        return QStringLiteral("zsync|") + source.value;
    case UpdateSourceType::Url:
        return QStringLiteral("direct|") + source.value;
    case UpdateSourceType::None:
        break;
    }
    return embedded;
}

bool isReleaseUpdateInfo(const QString &updateInfo)
{
    return updateInfo.startsWith(QLatin1String("gh-releases-zsync|"))
        || updateInfo.startsWith(QLatin1String("forgejo-releases|"))
        || updateInfo.startsWith(QLatin1String("direct|"));
}

bool isSupportedUpdateInfo(const QString &updateInfo)
{
    return isReleaseUpdateInfo(updateInfo) || updateInfo.startsWith(QLatin1String("zsync|"));
}

QString describeUpdateInfo(const QString &updateInfo)
{
    const QStringList parts = updateInfo.split(QLatin1Char('|'));
    const QString kind = parts.value(0);
    if (kind == QLatin1String("gh-releases-zsync") && parts.size() >= 3)
        return i18nc("update source", "GitHub · %1/%2", parts.at(1), parts.at(2));
    if (kind == QLatin1String("forgejo-releases") && parts.size() >= 4) {
        if (parts.at(1) == QLatin1String("codeberg.org"))
            return i18nc("update source", "Codeberg · %1/%2", parts.at(2), parts.at(3));
        return i18nc("update source", "Forgejo · %1/%2/%3", parts.at(1), parts.at(2), parts.at(3));
    }
    const QString host = QUrl(updateInfo.section(QLatin1Char('|'), 1)).host();
    if (kind == QLatin1String("zsync") && !host.isEmpty())
        return i18nc("update source", "zsync · %1", host);
    if (kind == QLatin1String("direct") && !host.isEmpty())
        return i18nc("update source", "Direct download · %1", host);
    return {};
}

QString updateSourceWebUrl(const QString &updateInfo)
{
    const QStringList parts = updateInfo.split(QLatin1Char('|'));
    const QString kind = parts.value(0);
    if (kind == QLatin1String("gh-releases-zsync") && parts.size() >= 3)
        return QStringLiteral("https://github.com/%1/%2").arg(parts.at(1), parts.at(2));
    if (kind == QLatin1String("forgejo-releases") && parts.size() >= 4)
        return QStringLiteral("https://%1/%2/%3").arg(parts.at(1), parts.at(2), parts.at(3));
    return {};
}

bool isPinned(const QString &filePath)
{
    KSharedConfig::Ptr config = KSharedConfig::openConfig(QStringLiteral("appimagemanagerrc"));
    config->reparseConfiguration();
    return config->group(QStringLiteral("Pinned")).readEntry(QFileInfo(filePath).fileName(), false);
}

void setPinned(const QString &filePath, bool pinned)
{
    KSharedConfig::Ptr config = KSharedConfig::openConfig(QStringLiteral("appimagemanagerrc"));
    KConfigGroup group = config->group(QStringLiteral("Pinned"));
    if (pinned)
        group.writeEntry(QFileInfo(filePath).fileName(), true);
    else
        group.deleteEntry(QFileInfo(filePath).fileName());
    config->sync();
}
