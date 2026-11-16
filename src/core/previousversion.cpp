// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#include "previousversion.h"
#include "updatesource.h"

#include <KConfigGroup>
#include <KSharedConfig>

#include <QDir>
#include <QFile>
#include <QFileInfo>

static KConfigGroup previousGroup(const QString &filePath)
{
    KSharedConfig::Ptr config = KSharedConfig::openConfig(QStringLiteral("appimagemanagerrc"));
    config->reparseConfiguration(); // the dashboard and the daemon both update
    return config->group(QStringLiteral("Previous")).group(QFileInfo(filePath).fileName());
}

static void makeExecutable(const QString &path)
{
    QFile file(path);
    file.setPermissions(file.permissions() | QFileDevice::ExeUser | QFileDevice::ExeGroup | QFileDevice::ExeOther);
}

QString previousFilePath(const QString &filePath)
{
    const QFileInfo info(filePath);
    return info.absolutePath() + QStringLiteral("/.previous/") + info.fileName();
}

bool hasPrevious(const QString &filePath)
{
    return QFileInfo::exists(previousFilePath(filePath));
}

QString previousVersion(const QString &filePath)
{
    if (!hasPrevious(filePath))
        return {};
    return previousGroup(filePath).readEntry("Version", QString());
}

bool keepPrevious(const QString &filePath, const QString &oldFile, const QString &version)
{
    const QString kept = previousFilePath(filePath);
    QDir().mkpath(QFileInfo(kept).absolutePath());
    QFile::remove(kept); // one per AppImage: the older one goes
    if (!QFile::rename(oldFile, kept))
        return false;
    KConfigGroup group = previousGroup(filePath);
    group.writeEntry("Version", version);
    group.writeEntry("Tag", loadUpdateSource(filePath).installedTag);
    group.sync();
    return true;
}

bool restorePrevious(const QString &filePath, const QString &currentVersion)
{
    const QString kept = previousFilePath(filePath);
    const QString aside = kept + QStringLiteral(".swap");
    if (!QFileInfo::exists(kept))
        return false;
    QFile::remove(aside);
    if (!QFile::rename(filePath, aside))
        return false;
    if (!QFile::rename(kept, filePath)) {
        QFile::rename(aside, filePath);
        return false;
    }
    QFile::rename(aside, kept);
    makeExecutable(filePath);

    KConfigGroup group = previousGroup(filePath);
    const QString keptTag = group.readEntry("Tag", QString());
    const QString currentTag = loadUpdateSource(filePath).installedTag;
    group.writeEntry("Version", currentVersion);
    group.writeEntry("Tag", currentTag);
    group.sync();
    saveInstalledTag(filePath, keptTag);
    return true;
}

void forgetPrevious(const QString &filePath)
{
    KConfigGroup group = previousGroup(filePath);
    group.deleteGroup();
    group.sync();
}
