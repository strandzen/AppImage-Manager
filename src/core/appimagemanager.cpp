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
#include <QProcess>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>
#include <QUrl>

AppImageManager::AppImageManager(QObject *parent)
    : QObject(parent)
{
}

KIO::CopyJob *AppImageManager::installAppImage(const QUrl &source, const QString &applicationsDir)
{
    QDir().mkpath(applicationsDir);

    const QUrl dest = QUrl::fromLocalFile(
        applicationsDir + QLatin1Char('/') + source.fileName());

    auto *job = KIO::move(source, dest, KIO::HideProgressInfo);
    connect(job, &KJob::result, this, [dest](KJob *j) {
        if (j->error()) {
            qCWarning(AIM_LOG) << "Install failed:" << j->errorString();
            return;
        }
        // Make executable
        const QString path = dest.toLocalFile();
        QFile::setPermissions(path,
            QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner |
            QFile::ReadGroup | QFile::ExeGroup |
            QFile::ReadOther | QFile::ExeOther);
    });
    return job;
}

bool AppImageManager::createDesktopLink(const QString &appImagePath,
                                        const AppImageInfo &info)
{
    const QString desktopPath = desktopFilePath(info);
    const QString iconPath = iconFilePath(info);

    QDir().mkpath(QFileInfo(desktopPath).absolutePath());
    QDir().mkpath(QFileInfo(iconPath).absolutePath());

    // Save extracted icon
    if (!info.iconData.isEmpty()) {
        QFile iconFile(iconPath);
        if (iconFile.open(QIODevice::WriteOnly))
            iconFile.write(info.iconData);
    }

    // Prefer system theme icon (lets icon themes override it)
    const QString iconField = QIcon::hasThemeIcon(info.appId) ? info.appId : iconPath;

    // Build Exec string
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

    rebuildSycoca();
    return true;
}

bool AppImageManager::removeDesktopLink(const QString & /*appImagePath*/,
                                        const AppImageInfo &info)
{
    const QString desktopPath = desktopFilePath(info);
    const QString iconPath = iconFilePath(info);

    if (QFile::exists(desktopPath))
        QFile::remove(desktopPath);
    if (QFile::exists(iconPath))
        QFile::remove(iconPath);

    rebuildSycoca();
    return true;
}

bool AppImageManager::isDesktopLinkEnabled(const QString & /*appImagePath*/,
                                           const AppImageInfo &info) const
{
    return QFile::exists(desktopFilePath(info));
}

// Directories that must never be proposed for deletion regardless of name match.
// These are system-level or cross-application config directories whose removal
// would cause data loss far beyond the AppImage being uninstalled.
static const QSet<QString> &corpseBlacklist()
{
    static const QSet<QString> s = {
        // Shells and terminal config
        QStringLiteral("bash"), QStringLiteral("fish"), QStringLiteral("zsh"),
        QStringLiteral("sh"), QStringLiteral("ksh"), QStringLiteral("dash"),
        // Version control
        QStringLiteral("git"), QStringLiteral("svn"),
        QStringLiteral("mercurial"), QStringLiteral("hg"),
        // KDE / Plasma core
        QStringLiteral("plasma"), QStringLiteral("plasma-nm"),
        QStringLiteral("plasma-workspace"), QStringLiteral("plasma-wayland-session"),
        QStringLiteral("kwin"), QStringLiteral("kscreen"), QStringLiteral("kded"),
        QStringLiteral("kde"), QStringLiteral("baloo"), QStringLiteral("kdeconnect"),
        QStringLiteral("knotifications4"), QStringLiteral("knotifications5"),
        QStringLiteral("knotifications6"), QStringLiteral("kfileplacesmodel"),
        QStringLiteral("kglobalaccel"), QStringLiteral("ksmserver"),
        // Qt
        QStringLiteral("qt"), QStringLiteral("qml"),
        // System services
        QStringLiteral("systemd"), QStringLiteral("dbus"), QStringLiteral("dbus-1"),
        // Audio / video
        QStringLiteral("pulse"), QStringLiteral("pipewire"),
        QStringLiteral("alsa"), QStringLiteral("wireplumber"),
        // Graphics / fonts
        QStringLiteral("fontconfig"), QStringLiteral("mesa"),
        QStringLiteral("vulkan"), QStringLiteral("nvidia"),
        // GTK
        QStringLiteral("gtk-2.0"), QStringLiteral("gtk-3.0"),
        QStringLiteral("gtk-4.0"), QStringLiteral("gconf"), QStringLiteral("dconf"),
        // Package managers and sandboxes
        QStringLiteral("flatpak"), QStringLiteral("snap"),
        QStringLiteral("docker"), QStringLiteral("podman"),
        // Common runtimes that would false-positive heavily
        QStringLiteral("python"), QStringLiteral("python3"),
        QStringLiteral("node"), QStringLiteral("npm"),
        QStringLiteral("cargo"), QStringLiteral("rustup"),
        // Security / auth
        QStringLiteral("gpg"), QStringLiteral("gnupg"),
        QStringLiteral("ssh"), QStringLiteral("keyring"),
        // Input methods
        QStringLiteral("ibus"), QStringLiteral("fcitx"), QStringLiteral("fcitx5"),
        // Browsers (own profiles, unrelated to AppImages)
        QStringLiteral("mozilla"), QStringLiteral("firefox"),
        QStringLiteral("chromium"), QStringLiteral("google-chrome"),
        // Misc desktop infrastructure
        QStringLiteral("autostart"), QStringLiteral("menus"),
        QStringLiteral("icons"), QStringLiteral("themes"),
    };
    return s;
}

QList<QPair<QString, qint64>> AppImageManager::findCorpses(const AppImageInfo &info) const
{
    QStringList terms;
    const QString nameToken = info.appName.toLower();
    const QString cleanToken = info.cleanName.toLower().remove(QLatin1String(".appimage"));
    if (nameToken.length() > 2)
        terms << nameToken;
    if (cleanToken != nameToken && cleanToken.length() > 2)
        terms << cleanToken;
    terms.removeDuplicates();

    const QStringList searchDirs = {
        QStandardPaths::writableLocation(QStandardPaths::ConfigLocation),
        QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation),
        QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation),
    };

    // Pre-compile word-boundary regexes outside the directory loop
    QList<QRegularExpression> regexes;
    regexes.reserve(terms.size());
    for (const QString &term : std::as_const(terms)) {
        regexes << QRegularExpression(
            QStringLiteral("(?:^|[^a-z0-9])") +
            QRegularExpression::escape(term) +
            QStringLiteral("(?:[^a-z0-9]|$)"));
    }

    const QSet<QString> &blacklist = corpseBlacklist();
    QList<QPair<QString, qint64>> corpses;

    for (const QString &baseDir : searchDirs) {
        if (!QDir(baseDir).exists())
            continue;
        const QStringList entries = QDir(baseDir).entryList(QDir::AllEntries | QDir::NoDotAndDotDot);
        for (const QString &entry : entries) {
            const QString entryLower = entry.toLower();
            if (blacklist.contains(entryLower))
                continue;
            for (const QRegularExpression &re : std::as_const(regexes)) {
                if (re.match(entryLower).hasMatch()) {
                    const QString fullPath = baseDir + QLatin1Char('/') + entry;
                    const qint64 size = QFileInfo(fullPath).isDir()
                        ? dirSize(fullPath)
                        : QFileInfo(fullPath).size();
                    corpses.append({fullPath, size});
                    break;
                }
            }
        }
    }

    return corpses;
}

KJob *AppImageManager::removeItems(const QUrl &appImageUrl,
                                   const AppImageInfo &info,
                                   const QList<QUrl> &corpseUrls)
{
    // Remove desktop link synchronously first (it's just file deletion)
    removeDesktopLink(appImageUrl.toLocalFile(), info);

    QList<QUrl> allUrls = corpseUrls;
    allUrls.prepend(appImageUrl);

    return KIO::trash(allUrls, KIO::HideProgressInfo);
}

// static
QString AppImageManager::desktopFilePath(const AppImageInfo &info)
{
    const QString slug = info.cleanName.toLower()
                             .remove(QLatin1String(".appimage"))
                             .replace(QLatin1Char(' '), QLatin1Char('_'));
    const QString appsDir = QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation);
    return appsDir + QStringLiteral("/appimage_") + slug + QStringLiteral(".desktop");
}

// static
QString AppImageManager::iconFilePath(const AppImageInfo &info)
{
    const QString slug = info.cleanName.toLower()
                             .remove(QLatin1String(".appimage"))
                             .replace(QLatin1Char(' '), QLatin1Char('_'));
    const QString iconsBase = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
                              + QLatin1String("/icons/AppImages");
    return iconsBase + QStringLiteral("/appimage_") + slug + info.iconExt;
}

// static
qint64 AppImageManager::dirSize(const QString &path)
{
    qint64 total = 0;
    QDirIterator it(path, QDir::Files | QDir::NoSymLinks, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        total += it.fileInfo().size();
    }
    return total;
}

void AppImageManager::rebuildSycoca()
{
    QProcess::startDetached(QStringLiteral("kbuildsycoca6"), {});
}
