// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#include "appimagemanager.h"
#include "logging.h"
#include "launchenvironment.h"
#include "sandbox.h"

#include <KDesktopFile>
#include <KConfigGroup>
#include <KIO/CopyJob>
#include <KLocalizedString>
#include <KShell>

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QIcon>
#include <QMap>
#include <QMultiHash>
#include <QMutex>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>
#include <QUrl>

#include <algorithm>

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

namespace {

QString applicationsDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation);
}

// The program an Exec= line starts, skipping a leading `env [-opts] KEY=value…`.
QString execTarget(const QString &exec)
{
    KShell::Errors error = KShell::NoError;
    const QStringList args = KShell::splitArgs(exec, KShell::NoOptions, &error);
    if (error != KShell::NoError || args.isEmpty())
        return {};
    qsizetype i = 0;
    if (QFileInfo(args.at(0)).fileName() == QLatin1String("env")) {
        ++i;
        while (i < args.size() && (args.at(i).startsWith(QLatin1Char('-')) || args.at(i).contains(QLatin1Char('='))))
            ++i;
    }
    // A sandboxed app's entry: appimagemanager --run <file>.
    if (i + 2 < args.size() && QFileInfo(args.at(i)).fileName() == QLatin1String("appimagemanager")
        && args.at(i + 1) == QLatin1String("--run"))
        i += 2;
    return i < args.size() ? QDir::cleanPath(args.at(i)) : QString();
}

// Visible menu entries in ~/.local/share/applications, by the program they start. Other
// software (an app registering itself, a vendor launcher) may have written entries for
// our AppImages; this is how we notice them. Rebuilt when the directory changes.
QMutex s_indexMutex;
QDateTime s_indexStamp;
QMultiHash<QString, QString> s_index;

QStringList entriesLaunching(const QString &appImagePath)
{
    QMutexLocker locker(&s_indexMutex);
    const QDateTime stamp = QFileInfo(applicationsDir()).lastModified();
    if (!stamp.isValid() || stamp != s_indexStamp) {
        s_index.clear();
        const QDir dir(applicationsDir());
        for (const QFileInfo &fi : dir.entryInfoList({ QStringLiteral("*.desktop") }, QDir::Files)) {
            KDesktopFile df(fi.absoluteFilePath());
            const KConfigGroup grp = df.desktopGroup();
            if (grp.readEntry("Hidden", false) || grp.readEntry("NoDisplay", false))
                continue;
            const QString target = execTarget(grp.readEntry("Exec", QString()));
            if (!target.isEmpty())
                s_index.insert(target, fi.absoluteFilePath());
        }
        s_indexStamp = stamp;
    }
    return s_index.values(QDir::cleanPath(appImagePath));
}

void invalidateEntryIndex()
{
    QMutexLocker locker(&s_indexMutex);
    s_indexStamp = QDateTime();
}

void refreshMenuCaches()
{
    AppImageManager::rebuildSycoca();
    QProcess::startDetached(QStringLiteral("update-desktop-database"), { applicationsDir() });
}

// The app's own .desktop text, group by group: raw "key=value" lines (escapes untouched,
// so they can be copied into our entry verbatim) in file order.
using RawGroups = QMap<QString, QList<std::pair<QString, QString>>>;

RawGroups parseDesktopText(const QString &text)
{
    RawGroups groups;
    QString group;
    for (const QString &rawLine : text.split(QLatin1Char('\n'))) {
        const QString line = rawLine.trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
            continue;
        if (line.startsWith(QLatin1Char('[')) && line.endsWith(QLatin1Char(']'))) {
            group = line.mid(1, line.size() - 2);
            continue;
        }
        const qsizetype eq = line.indexOf(QLatin1Char('='));
        if (eq <= 0 || group.isEmpty())
            continue;
        groups[group].append({ line.left(eq).trimmed(), line.mid(eq + 1).trimmed() });
    }
    return groups;
}

// "Name[de]" → "Name"
QString baseKey(const QString &key)
{
    const qsizetype bracket = key.indexOf(QLatin1Char('['));
    return bracket < 0 ? key : key.left(bracket);
}

QString rawValue(const QList<std::pair<QString, QString>> &lines, const QString &key)
{
    for (const auto &[k, v] : lines) {
        if (k == key)
            return v;
    }
    return {};
}

// A desktop-entry string value with its escapes (\s \n \t \r \\) resolved.
QString unescapeDesktopValue(const QString &value)
{
    QString out;
    out.reserve(value.size());
    for (qsizetype i = 0; i < value.size(); ++i) {
        const QChar c = value.at(i);
        if (c != QLatin1Char('\\') || i + 1 >= value.size()) {
            out += c;
            continue;
        }
        const QChar n = value.at(++i);
        if (n == QLatin1Char('s')) out += QLatin1Char(' ');
        else if (n == QLatin1Char('n')) out += QLatin1Char('\n');
        else if (n == QLatin1Char('t')) out += QLatin1Char('\t');
        else if (n == QLatin1Char('r')) out += QLatin1Char('\r');
        else if (n == QLatin1Char('\\')) out += QLatin1Char('\\');
        else { out += c; out += n; }
    }
    return out;
}

// The arguments of an Exec= line after the program: quoted again, field codes (%U, %f, …) kept.
QString execArguments(const QString &exec)
{
    const QStringList parts = KShell::splitArgs(unescapeDesktopValue(exec));
    QStringList args;
    for (qsizetype i = 1; i < parts.size(); ++i)
        args << (parts.at(i).startsWith(QLatin1Char('%')) ? parts.at(i) : KShell::quoteArg(parts.at(i)));
    return args.join(QLatin1Char(' '));
}

// Keys of the app's [Desktop Entry] copied into our entry, with their translations.
bool isCopiedEntryKey(const QString &base)
{
    static const QSet<QString> keys = {
        QStringLiteral("Name"), QStringLiteral("GenericName"), QStringLiteral("Comment"),
        QStringLiteral("Keywords"), QStringLiteral("MimeType"), QStringLiteral("StartupWMClass"),
        QStringLiteral("StartupNotify"),
    };
    return keys.contains(base);
}

// Marks entries written with the app's own keys; older ones are rewritten once.
constexpr int kEntryFormat = 2;
const char kEntryFormatKey[] = "X-AppImageManager-Entry";
const char kManageAction[] = "appimagemanager-manage";

} // anonymous namespace

namespace AppImageManager {

// Return a stable, collision-free key for icon and fallback desktop file naming: a slug
// from the clean name, disambiguated by a 6-char hex hash of the original filename so
// that two differently-named AppImages that happen to share a display name don't
// overwrite each other's files.
static QString fileKey(const AppImageInfo &info)
{
    const QString base = info.cleanName.toLower()
                             .remove(QLatin1String(".appimage"))
                             .replace(QLatin1Char(' '), QLatin1Char('_'));
    if (info.originalName.isEmpty())
        return base;
    const QString hash = QStringLiteral("%1")
                             .arg(qHash(info.originalName), 6, 16, QLatin1Char('0'))
                             .left(6);
    return base + QLatin1Char('_') + hash;
}

static QString legacyDesktopFilePath(const AppImageInfo &info);

KIO::CopyJob *installAppImage(const QUrl &source, const QString &applicationsDir)
{
    QDir().mkpath(applicationsDir);

    const QUrl dest = QUrl::fromLocalFile(
        applicationsDir + QLatin1Char('/') + source.fileName());

    // Policy: only consume the source when it sits under the user's
    // Downloads folder (the expected throwaway location). Anywhere else —
    // USB sticks, project directories, network mounts — we copy so the
    // user's original file stays where they put it.
    const QString downloads = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    const QString srcPath = source.toLocalFile();
    const bool fromDownloads =
        !downloads.isEmpty() && !srcPath.isEmpty()
        && (srcPath == downloads || srcPath.startsWith(downloads + QLatin1Char('/')));

    auto *job = fromDownloads ? KIO::move(source, dest) : KIO::copy(source, dest);
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
    // A literal % must be %% in Exec= (else it starts a field code like %U); the app's own
    // execArgs keep theirs.
    // Sandboxed: appimagemanager --run sets everything up itself.
    const QStringList command = launchCommand(appImagePath);
    QString execStr;
    if (command.size() > 1) {
        for (const QString &part : command)
            execStr += (execStr.isEmpty() ? QString() : QStringLiteral(" ")) + KShell::quoteArg(part);
    } else {
        execStr = environmentExecPrefix(launchVariables(appImagePath)) + KShell::quoteArg(appImagePath);
    }
    execStr.replace(QLatin1Char('%'), QStringLiteral("%%"));
    if (!info.execArgs.isEmpty())
        execStr += QLatin1Char(' ') + info.execArgs;

    const QString desktopPath = desktopFilePath(info);
    const QString iconPath = iconFilePath(info);

    // Another entry (written by the app or its launcher under another name) already starts
    // this AppImage: use it instead of adding a second one, only keeping its Exec current.
    if (!QFile::exists(desktopPath)) {
        for (const QString &entry : entriesLaunching(appImagePath)) {
            if (QFileInfo(entry).fileName().startsWith(QLatin1String("appimage_")))
                continue;
            KDesktopFile df(entry);
            KConfigGroup grp = df.desktopGroup();
            grp.writeEntry(QStringLiteral("Exec"), execStr);
            if (grp.hasKey("TryExec"))
                grp.writeEntry(QStringLiteral("TryExec"), appImagePath);
            df.sync();
            invalidateEntryIndex();
            removeDuplicateDesktopLinks(appImagePath, info);
            refreshMenuCaches();
            return true;
        }
    }

    if (!QDir().mkpath(QFileInfo(desktopPath).absolutePath())) {
        qCWarning(AIM_LOG) << "createDesktopLink: cannot create .desktop directory" << QFileInfo(desktopPath).absolutePath();
        return false;
    }
    if (!QDir().mkpath(QFileInfo(iconPath).absolutePath())) {
        qCWarning(AIM_LOG) << "createDesktopLink: cannot create icon directory" << QFileInfo(iconPath).absolutePath();
        return false;
    }

    if (!info.iconData.isEmpty()) {
        QFile iconFile(iconPath);
        if (!iconFile.open(QIODevice::WriteOnly)) {
            qCWarning(AIM_LOG) << "createDesktopLink: cannot open icon for writing" << iconPath << iconFile.errorString();
            return false;
        }
        if (iconFile.write(info.iconData) != info.iconData.size()) {
            qCWarning(AIM_LOG) << "createDesktopLink: short write to icon file" << iconPath;
            return false;
        }
    }

    const QString iconField = QIcon::hasThemeIcon(info.appId) ? info.appId : (QStringLiteral("appimage_") + fileKey(info));

    // An entry we wrote (named appimage_*, or carrying our marker, or our old comment) is
    // rewritten from scratch; one the app wrote itself under the same id keeps its own keys,
    // only Exec and our action change.
    bool ownEntry = !QFile::exists(desktopPath)
                    || QFileInfo(desktopPath).fileName().startsWith(QLatin1String("appimage_"));
    if (!ownEntry) {
        const KDesktopFile existing(desktopPath);
        const KConfigGroup grp = existing.desktopGroup();
        const QString comment = grp.readEntryUntranslated("Comment", QString());
        ownEntry = grp.hasKey(kEntryFormatKey) || comment == QLatin1String("Managed by AppImage Manager")
                   || comment == i18n("Managed by AppImage Manager");
    }
    if (ownEntry)
        QFile::remove(desktopPath);

    const RawGroups app = parseDesktopText(info.desktopEntry);
    const auto appEntry = app.value(QStringLiteral("Desktop Entry"));
    // The app's actions (right-click items in the menu and task manager) with known groups.
    QStringList appActions;
    if (ownEntry) {
        for (const QString &id : rawValue(appEntry, QStringLiteral("Actions")).split(QLatin1Char(';'), Qt::SkipEmptyParts)) {
            if (app.contains(QStringLiteral("Desktop Action ") + id) && id != QLatin1String(kManageAction))
                appActions << id;
        }
    }
    // Exec without the app's own arguments, for the actions.
    QString baseExec = execStr;
    if (!info.execArgs.isEmpty())
        baseExec.chop(info.execArgs.size() + 1);

    KDesktopFile df(desktopPath);
    KConfigGroup grp = df.desktopGroup();
    grp.writeEntry(QStringLiteral("Type"),       QStringLiteral("Application"));
    if (!ownEntry || rawValue(appEntry, QStringLiteral("Name")).isEmpty())
        grp.writeEntry(QStringLiteral("Name"),   info.appName);
    grp.writeEntry(QStringLiteral("Exec"),       execStr);
    grp.writeEntry(QStringLiteral("Icon"),       iconField);
    grp.writeEntry(QStringLiteral("Terminal"),   false);
    grp.writeEntry(QStringLiteral("Categories"),
                   info.categories.isEmpty() ? QStringLiteral("Utility;") : info.categories);
    if (!grp.hasKey("Comment") && rawValue(appEntry, QStringLiteral("Comment")).isEmpty())
        grp.writeEntry(QStringLiteral("Comment"), i18n("Managed by AppImage Manager"));
    if (grp.hasKey("TryExec"))
        grp.writeEntry(QStringLiteral("TryExec"), appImagePath);

    QStringList actions = ownEntry ? appActions
                                   : grp.readEntry("Actions", QString()).split(QLatin1Char(';'), Qt::SkipEmptyParts);
    if (!actions.contains(QLatin1String(kManageAction)))
        actions << QLatin1String(kManageAction);
    grp.writeEntry(QStringLiteral("Actions"), QString(actions.join(QLatin1Char(';')) + QLatin1Char(';')));
    for (const QString &id : std::as_const(appActions)) {
        const auto lines = app.value(QStringLiteral("Desktop Action ") + id);
        KConfigGroup action = df.group(QStringLiteral("Desktop Action ") + id);
        const QString args = execArguments(rawValue(lines, QStringLiteral("Exec")));
        action.writeEntry(QStringLiteral("Exec"), args.isEmpty() ? baseExec : QString(baseExec + QLatin1Char(' ') + args));
        const QString icon = rawValue(lines, QStringLiteral("Icon"));
        action.writeEntry(QStringLiteral("Icon"), !icon.isEmpty() && QIcon::hasThemeIcon(icon) ? icon : iconField);
    }
    KConfigGroup manage = df.group(QStringLiteral("Desktop Action ") + QLatin1String(kManageAction));
    manage.writeEntry(QStringLiteral("Name"), i18n("Manage in AppImage Manager"));
    manage.writeEntry(QStringLiteral("Icon"), QStringLiteral("appimagemanager"));
    manage.writeEntry(QStringLiteral("Exec"), QString(QStringLiteral("appimagemanager --select ")
                                              + KShell::quoteArg(appImagePath).replace(QLatin1Char('%'), QStringLiteral("%%"))));
    if (ownEntry)
        grp.writeEntry(kEntryFormatKey, kEntryFormat);
    df.sync();
    if (!QFile::exists(desktopPath)) {
        qCWarning(AIM_LOG) << "createDesktopLink: KDesktopFile::sync did not produce" << desktopPath;
        return false;
    }

    // The app's own lines, copied verbatim (translations included): KConfig can't write
    // translated keys for other languages. Inserted under their group headers.
    if (ownEntry) {
        QHash<QString, QStringList> extra;
        for (const auto &[key, value] : appEntry) {
            if (isCopiedEntryKey(baseKey(key)))
                extra[QStringLiteral("Desktop Entry")] << key + QLatin1Char('=') + value;
        }
        for (const QString &id : std::as_const(appActions)) {
            const QString group = QStringLiteral("Desktop Action ") + id;
            for (const auto &[key, value] : app.value(group)) {
                if (baseKey(key) == QLatin1String("Name"))
                    extra[group] << key + QLatin1Char('=') + value;
            }
        }
        QFile file(desktopPath);
        if (!extra.isEmpty() && file.open(QIODevice::ReadOnly)) {
            const QStringList lines = QString::fromUtf8(file.readAll()).split(QLatin1Char('\n'));
            file.close();
            QStringList out;
            for (const QString &line : lines) {
                out << line;
                if (line.startsWith(QLatin1Char('[')) && line.endsWith(QLatin1Char(']')))
                    out << extra.value(line.mid(1, line.size() - 2));
            }
            if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
                file.write(out.join(QLatin1Char('\n')).toUtf8());
        }
    }
    invalidateEntryIndex();
    removeDuplicateDesktopLinks(appImagePath, info); // e.g. our pre-2.1.1 appimage_* file

    rebuildSycoca();
    // kbuildsycoca6 covers Plasma's service catalog; also update the XDG
    // caches so non-Plasma launchers (Rofi, wofi, Hyprland launchers, etc.)
    // pick up the new entry. Both tools are optional — ignore exit codes.
    QProcess::startDetached(QStringLiteral("update-desktop-database"),
        { QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation) });
    if (!info.iconData.isEmpty())
        QProcess::startDetached(QStringLiteral("update-icon-caches"),
            { QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
              + QStringLiteral("/icons/hicolor") });
    return true;
}

bool removeDesktopLink(const QString &appImagePath, const AppImageInfo &info)
{
    // Every visible entry that starts the AppImage, so it really leaves the menu. Our own
    // files are deleted; entries other software wrote go to the Trash.
    QStringList entries = entriesLaunching(appImagePath);
    for (const QString &own : { desktopFilePath(info), legacyDesktopFilePath(info) }) {
        if (QFile::exists(own) && !entries.contains(own))
            entries.append(own);
    }
    const QString iconPath = iconFilePath(info);
    bool ok = true;

    for (const QString &entry : std::as_const(entries)) {
        const bool ours = QFileInfo(entry).fileName().startsWith(QLatin1String("appimage_"))
                          || entry == desktopFilePath(info);
        if (!(ours ? QFile::remove(entry) : QFile::moveToTrash(entry))) {
            qCWarning(AIM_LOG) << "removeDesktopLink: cannot remove" << entry;
            ok = false;
        }
    }
    if (QFile::exists(iconPath) && !QFile::remove(iconPath)) {
        qCWarning(AIM_LOG) << "removeDesktopLink: cannot remove" << iconPath;
        ok = false;
    }
    invalidateEntryIndex();

    if (ok) {
        refreshMenuCaches();
        QProcess::startDetached(QStringLiteral("update-icon-caches"),
            { QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
              + QStringLiteral("/icons/hicolor") });
    }
    return ok;
}

void rebuildSycoca()
{
    QProcess::startDetached(QStringLiteral("kbuildsycoca6"), {});
}

bool desktopLinkOutdated(const AppImageInfo &info)
{
    const QString path = desktopFilePath(info);
    if (!QFile::exists(path))
        return false;
    const KDesktopFile df(path);
    const KConfigGroup grp = df.desktopGroup();
    if (grp.readEntry(kEntryFormatKey, 0) >= kEntryFormat)
        return false;
    const QString comment = grp.readEntryUntranslated("Comment", QString());
    return QFileInfo(path).fileName().startsWith(QLatin1String("appimage_"))
        || comment == QLatin1String("Managed by AppImage Manager") || comment == i18n("Managed by AppImage Manager");
}

bool isDesktopLinkEnabled(const QString &appImagePath, const AppImageInfo &info)
{
    // Any visible entry that starts this AppImage counts, whoever wrote it.
    return QFile::exists(desktopFilePath(info)) || QFile::exists(legacyDesktopFilePath(info))
        || !entriesLaunching(appImagePath).isEmpty();
}

static QList<CorpseEntry> matchingDirs(const AppImageInfo &info, const QStringList &baseDirs, bool withSize);

QList<CorpseEntry> findCorpses(const AppImageInfo &info)
{
    return matchingDirs(info, {
        QStandardPaths::writableLocation(QStandardPaths::ConfigLocation),
        QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation),
        QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation),
    }, true);
}

QList<CorpseEntry> findConfigDirs(const AppImageInfo &info)
{
    return matchingDirs(info, { QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) }, false);
}

static QList<CorpseEntry> matchingDirs(const AppImageInfo &info, const QStringList &baseDirs, bool withSize)
{
    if (info.appId.isEmpty() && info.appName.isEmpty())
        return {};

    const QString idLower = info.appId.toLower();

    // Build fuzzy candidates from appName variations
    const QString nameLower = info.appName.toLower().simplified();
    QSet<QString> lowCandidates;
    if (!nameLower.isEmpty()) {
        lowCandidates << nameLower;
        lowCandidates << QString(nameLower).replace(QLatin1Char(' '), QLatin1Char('-'));
        lowCandidates << QString(nameLower).replace(QLatin1Char(' '), QLatin1Char('_'));
        lowCandidates << QString(nameLower).remove(QLatin1Char(' '));
    }
    lowCandidates.remove(idLower); // appId match is High, not Low

    QList<CorpseEntry> result;
    QSet<QString> seen;

    for (const QString &base : baseDirs) {
        const QStringList subdirs = QDir(base).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString &sub : subdirs) {
            const QString fullPath = base + QLatin1Char('/') + sub;
            if (seen.contains(fullPath))
                continue;
            const QString subLower = sub.toLower();
            if (!idLower.isEmpty() && subLower == idLower) {
                seen.insert(fullPath);
                result.append({fullPath, withSize ? dirSize(fullPath) : 0, CorpseConfidence::High});
            } else if (lowCandidates.contains(subLower)) {
                seen.insert(fullPath);
                result.append({fullPath, withSize ? dirSize(fullPath) : 0, CorpseConfidence::Low});
            }
        }
    }

    std::stable_sort(result.begin(), result.end(), [](const CorpseEntry &a, const CorpseEntry &b) {
        return a.confidence > b.confidence;
    });

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

// Our menu entry before 2.1.1, and still the fallback name: appimage_<slug>_<hash>.desktop.
static QString legacyDesktopFilePath(const AppImageInfo &info)
{
    return applicationsDir() + QStringLiteral("/appimage_") + fileKey(info) + QStringLiteral(".desktop");
}

// The app's own desktop id (its bundled .desktop name, e.g. ai.storyteller.figcraft) when it
// is reverse-DNS. Using it means an app that also registers itself writes the same file
// instead of a second entry, and the taskbar matches the app's windows to the entry. Not
// used when an installed system package already has that id: ours would hide it.
static QString ownDesktopId(const AppImageInfo &info)
{
    static const QRegularExpression reverseDns(QStringLiteral("^[A-Za-z0-9_-]+(\\.[A-Za-z0-9_-]+)+$"));
    if (!reverseDns.match(info.appId).hasMatch())
        return {};
    const QString file = info.appId + QStringLiteral(".desktop");
    for (const QString &found : QStandardPaths::locateAll(QStandardPaths::ApplicationsLocation, file)) {
        if (QFileInfo(found).absolutePath() != QFileInfo(applicationsDir()).absoluteFilePath())
            return {};
    }
    return info.appId;
}

QString desktopFilePath(const AppImageInfo &info)
{
    const QString id = ownDesktopId(info);
    return id.isEmpty() ? legacyDesktopFilePath(info)
                        : applicationsDir() + QLatin1Char('/') + id + QStringLiteral(".desktop");
}

bool removeDuplicateDesktopLinks(const QString &appImagePath, const AppImageInfo &info)
{
    // Only our own appimage_* files are ever removed. If an entry someone else wrote (the app
    // itself, a vendor launcher) starts the AppImage, ours go; otherwise one of ours stays.
    const auto isOurs = [](const QString &entry) {
        return QFileInfo(entry).fileName().startsWith(QLatin1String("appimage_"));
    };
    const QStringList launchers = entriesLaunching(appImagePath);
    const bool foreignExists = std::any_of(launchers.cbegin(), launchers.cend(),
                                           [&isOurs](const QString &e) { return !isOurs(e); });
    QStringList ours;
    for (const QString &entry : launchers) {
        if (isOurs(entry))
            ours.append(entry);
    }
    const QString legacy = legacyDesktopFilePath(info);
    if (QFile::exists(legacy) && !ours.contains(legacy))
        ours.append(legacy); // starts an old path (moved folder), still ours for this AppImage

    QStringList toRemove;
    if (foreignExists) {
        toRemove = ours;
    } else if (ours.size() > 1) {
        const QString keep = ours.contains(desktopFilePath(info)) ? desktopFilePath(info) : ours.first();
        for (const QString &entry : std::as_const(ours)) {
            if (entry != keep)
                toRemove.append(entry);
        }
    }

    bool removed = false;
    for (const QString &entry : std::as_const(toRemove)) {
        if (QFile::remove(entry)) {
            qCInfo(AIM_LOG) << "Removed duplicate app menu entry" << entry;
            removed = true;
        }
    }
    if (!removed)
        return false;

    invalidateEntryIndex();
    // The icon is ours too; drop it unless a remaining entry still uses it.
    const QString iconName = QStringLiteral("appimage_") + fileKey(info);
    const QStringList remaining = entriesLaunching(appImagePath);
    const bool iconUsed = std::any_of(remaining.cbegin(), remaining.cend(), [&iconName](const QString &e) {
        return KDesktopFile(e).desktopGroup().readEntry("Icon", QString()) == iconName;
    });
    if (!iconUsed)
        QFile::remove(iconFilePath(info));
    refreshMenuCaches();
    return true;
}

QString iconFilePath(const AppImageInfo &info)
{
    const QString genericData = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    const QString subDir = (info.iconExt.endsWith(QLatin1String(".svg"), Qt::CaseInsensitive) ||
                            info.iconExt.endsWith(QLatin1String(".svgz"), Qt::CaseInsensitive))
                           ? QStringLiteral("/icons/hicolor/scalable/apps")
                           : QStringLiteral("/icons/hicolor/256x256/apps");
    return genericData + subDir + QStringLiteral("/appimage_") + fileKey(info) + info.iconExt;
}

} // namespace AppImageManager
