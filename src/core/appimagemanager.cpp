// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#include "appimagemanager.h"
#include "logging.h"

#include <KDesktopFile>
#include <KConfigGroup>
#include <KIO/CopyJob>
#include <KLocalizedString>
#include <KShell>

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QIcon>
#include <QStandardPaths>
#include <QUrl>

namespace {

qint64 dirSize(const QString &path)
{
    qint64 total = 0;
    QDirIterator it(path, QDir::Files | QDir::NoSymLinks, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        total += it.fileInfo().size();
    }
    return total;
}

} // anonymous namespace

namespace AppImageManager {

KIO::CopyJob *installAppImage(const QUrl &source, const QString &applicationsDir)
{
    QDir().mkpath(applicationsDir);

    const QUrl dest = QUrl::fromLocalFile(
        applicationsDir + QLatin1Char('/') + source.fileName());

    auto *job = KIO::move(source, dest);
    QObject::connect(job, &KJob::result, [dest](KJob *j) {
        if (j->error()) {
            qCWarning(AIM_LOG) << "Install failed:" << j->errorString();
            return;
        }
        const QString path = dest.toLocalFile();
        QFile::setPermissions(path,
            QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner |
            QFile::ReadGroup | QFile::ExeGroup |
            QFile::ReadOther | QFile::ExeOther);
    });
    return job;
}

bool createDesktopLink(const QString &appImagePath, const AppImageInfo &info)
{
    const QString desktopPath = desktopFilePath(info);
    const QString iconPath = iconFilePath(info);

    QDir().mkpath(QFileInfo(desktopPath).absolutePath());
    QDir().mkpath(QFileInfo(iconPath).absolutePath());

    if (!info.iconData.isEmpty()) {
        QFile iconFile(iconPath);
        if (iconFile.open(QIODevice::WriteOnly))
            iconFile.write(info.iconData);
    }

    const QString iconField = QIcon::hasThemeIcon(info.appId) ? info.appId : iconPath;

    QString execStr = KShell::quoteArg(appImagePath);
    if (!info.execArgs.isEmpty())
        execStr += QLatin1Char(' ') + info.execArgs;

    KDesktopFile df(desktopPath);
    KConfigGroup grp = df.desktopGroup();
    grp.writeEntry(QStringLiteral("Type"),       QStringLiteral("Application"));
    grp.writeEntry(QStringLiteral("Name"),       info.appName);
    grp.writeEntry(QStringLiteral("Exec"),       execStr);
    grp.writeEntry(QStringLiteral("Icon"),       iconField);
    grp.writeEntry(QStringLiteral("Terminal"),   false);
    grp.writeEntry(QStringLiteral("Categories"),
                   info.categories.isEmpty() ? QStringLiteral("Utility;") : info.categories);
    grp.writeEntry(QStringLiteral("Comment"),
                   i18n("Managed by AppImage Manager"));
    df.sync();
    return true;
}

bool removeDesktopLink(const QString & /*appImagePath*/, const AppImageInfo &info)
{
    const QString desktopPath = desktopFilePath(info);
    const QString iconPath = iconFilePath(info);

    if (QFile::exists(desktopPath))
        QFile::remove(desktopPath);
    if (QFile::exists(iconPath))
        QFile::remove(iconPath);

    return true;
}

bool isDesktopLinkEnabled(const QString & /*appImagePath*/, const AppImageInfo &info)
{
    return QFile::exists(desktopFilePath(info));
}

QList<QPair<QString, qint64>> findCorpses(const AppImageInfo &info)
{
    if (info.appId.isEmpty())
        return {};

    const QString home = QDir::homePath();
    const QStringList candidates = {
        home + QStringLiteral("/.config/")      + info.appId,
        home + QStringLiteral("/.local/share/") + info.appId,
        home + QStringLiteral("/.cache/")       + info.appId,
    };

    QList<QPair<QString, qint64>> result;
    for (const QString &path : candidates) {
        if (QDir(path).exists())
            result.append({ path, dirSize(path) });
    }
    return result;
}

KJob *removeItems(const QUrl &appImageUrl,
                  const AppImageInfo &info,
                  const QList<QUrl> &corpseUrls)
{
    removeDesktopLink(appImageUrl.toLocalFile(), info);

    QList<QUrl> allUrls = corpseUrls;
    allUrls.prepend(appImageUrl);

    return KIO::trash(allUrls);
}

QString desktopFilePath(const AppImageInfo &info)
{
    const QString slug = info.cleanName.toLower()
                             .remove(QLatin1String(".appimage"))
                             .replace(QLatin1Char(' '), QLatin1Char('_'));
    const QString appsDir = QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation);
    return appsDir + QStringLiteral("/appimage_") + slug + QStringLiteral(".desktop");
}

QString iconFilePath(const AppImageInfo &info)
{
    const QString slug = info.cleanName.toLower()
                             .remove(QLatin1String(".appimage"))
                             .replace(QLatin1Char(' '), QLatin1Char('_'));
    const QString iconsBase = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
                              + QLatin1String("/icons/AppImages");
    return iconsBase + QStringLiteral("/appimage_") + slug + info.iconExt;
}

} // namespace AppImageManager
