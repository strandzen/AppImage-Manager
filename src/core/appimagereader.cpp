// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#include "appimagereader.h"
#include "logging.h"

#include <KDesktopFile>
#include <KConfigGroup>
#include <KShell>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTemporaryDir>

#ifdef HAVE_LIBAPPIMAGE
#include <appimage/appimage.h>
#endif

AppImageReader::AppImageReader(const QString &path)
    : m_path(path)
{
}

AppImageInfo AppImageReader::read()
{
    if (!QFile::exists(m_path)) {
        qCWarning(AIM_LOG) << "AppImage not found:" << m_path;
        return {};
    }

#ifdef HAVE_LIBAPPIMAGE
    return readWithLibappimage();
#else
    return readWithSquashfuse();
#endif
}

// ──────────────────────────────────────────────────────────────────────────────
// Shared helpers
// ──────────────────────────────────────────────────────────────────────────────

// static
QString AppImageReader::cleanName(const QString &filename)
{
    if (!filename.toLower().endsWith(QLatin1String(".appimage")))
        return filename;

    QString base = filename.left(filename.length() - 9);

    static const QStringList archSuffixes = {
        QStringLiteral("-x86_64"), QStringLiteral("_x86_64"),
        QStringLiteral("-aarch64"), QStringLiteral("-arm64"),
        QStringLiteral("-amd64"),  QStringLiteral("_amd64"),
    };
    for (const QString &arch : archSuffixes) {
        if (base.endsWith(arch, Qt::CaseInsensitive)) {
            base.chop(arch.length());
            break;
        }
    }

    base.replace(QLatin1Char('-'), QLatin1Char(' '));
    base.replace(QLatin1Char('_'), QLatin1Char(' '));

    const QStringList words = base.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    QStringList result;
    result.reserve(words.size());
    for (const QString &word : words)
        result << (word == word.toUpper() ? word : word.at(0).toUpper() + word.mid(1));

    return result.join(QLatin1Char(' ')) + QLatin1String(".AppImage");
}

// static
QString AppImageReader::extractAppId(const QString &filename)
{
    const QString lower = filename.toLower().remove(QLatin1String(".appimage"));
    static const QRegularExpression re(QStringLiteral("^([a-z0-9]+)"));
    const QRegularExpressionMatch m = re.match(lower);
    if (m.hasMatch())
        return m.captured(1);
    return lower.split(QRegularExpression(QStringLiteral("[-_.]"))).value(0);
}

// static
QString AppImageReader::versionFromFilename(const QString &filename)
{
    // Match patterns like App-1.2.3, App_2.0.1, App-v1.4 in the filename.
    static const QRegularExpression re(QStringLiteral(R"([-_][vV]?(\d+\.\d+[\d.]*))"));
    const auto m = re.match(filename);
    return m.hasMatch() ? m.captured(1) : QString();
}

// ──────────────────────────────────────────────────────────────────────────────
// libappimage path (in-process, no FUSE, no subprocess)
// ──────────────────────────────────────────────────────────────────────────────
#ifdef HAVE_LIBAPPIMAGE

AppImageInfo AppImageReader::readWithLibappimage()
{
    AppImageInfo info;
    info.originalName = QFileInfo(m_path).fileName();
    info.cleanName    = cleanName(info.originalName);
    info.appId        = extractAppId(info.originalName);
    info.appName      = info.cleanName;
    info.appName.remove(QLatin1String(".AppImage"), Qt::CaseInsensitive);
    info.version      = QStringLiteral("Unknown");
    info.fileSize     = QFileInfo(m_path).size();

    const QByteArray pathBytes = m_path.toUtf8();

    const int type = appimage_get_type(pathBytes.constData(), false);
    if (type < 1 || type > 2) {
        qCWarning(AIM_LOG) << m_path << "is not a valid AppImage (type" << type << ")";
        return info;
    }

    // Find .desktop file by listing the AppImage contents
    const QString desktopInner = findDesktopFile();
    if (desktopInner.isEmpty()) {
        qCWarning(AIM_LOG) << "No .desktop file found in" << m_path;
        info.isValid = true; // still valid, just no metadata
        return info;
    }

    // Read .desktop file into a temporary file for KDesktopFile parsing
    QString desktopExt;
    const QByteArray desktopData = readFileFromAppImage(desktopInner, desktopExt);
    if (!desktopData.isEmpty()) {
        QTemporaryDir tmp;
        if (tmp.isValid()) {
            const QString tmpDesktop = tmp.filePath(QStringLiteral("app.desktop"));
            QFile f(tmpDesktop);
            if (f.open(QIODevice::WriteOnly)) {
                f.write(desktopData);
                f.close();

                KDesktopFile df(tmpDesktop);
                const KConfigGroup entry = df.desktopGroup();

                info.appName    = entry.readEntry(QStringLiteral("Name"), info.appName);
                info.categories = entry.readEntry(QStringLiteral("Categories"));

                info.updateInfo = entry.readEntry(QStringLiteral("X-AppImage-Update-Information"));

                // Version priority: X-AppImage-Version → X-AppImage-Update-Information
                // prefix → filename regex → give up
                QString ver = entry.readEntry(QStringLiteral("X-AppImage-Version"));
                if (ver.isEmpty() && !info.updateInfo.isEmpty()) {
                    // Some AppImages embed version in the update URL: "zsync|url/App-1.2.3..."
                    static const QRegularExpression verInUrl(QStringLiteral(R"([-_](\d+\.\d+[\d.]*))"));
                    const auto m = verInUrl.match(info.updateInfo);
                    if (m.hasMatch())
                        ver = m.captured(1);
                }
                if (ver.isEmpty())
                    ver = versionFromFilename(info.originalName);
                info.version = ver.isEmpty() ? QStringLiteral("Unknown") : ver;

                const QString execFull = entry.readEntry(QStringLiteral("Exec"));
                const QStringList execParts = KShell::splitArgs(execFull);
                if (execParts.size() > 1) {
                    QStringList args;
                    for (int i = 1; i < execParts.size(); ++i)
                        if (!execParts.at(i).startsWith(QLatin1Char('%')))
                            args << KShell::quoteArg(execParts.at(i));
                    info.execArgs = args.join(QLatin1Char(' '));
                }

                // Find and read icon
                const QString iconName = entry.readEntry(QStringLiteral("Icon"));
                const QString iconInner = findIconFile(iconName);
                if (!iconInner.isEmpty()) {
                    QString iconExt;
                    info.iconData = readFileFromAppImage(iconInner, iconExt);
                    info.iconExt = iconExt;
                }
            }
        }
    }

    info.isValid = true;
    return info;
}

QString AppImageReader::findDesktopFile()
{
    const QByteArray pathBytes = m_path.toUtf8();
    char **files = appimage_list_files(pathBytes.constData());
    if (!files)
        return {};

    QString found;
    for (int i = 0; files[i] != nullptr; ++i) {
        const QString f = QString::fromUtf8(files[i]);
        // Root-level .desktop files only (no subdirectories)
        if (f.endsWith(QLatin1String(".desktop")) && !f.contains(QLatin1Char('/'))) {
            found = f;
            break;
        }
        // Accept ./ prefix (some AppImages prefix root entries)
        if (f.startsWith(QLatin1String("./")) && f.endsWith(QLatin1String(".desktop"))
                && f.count(QLatin1Char('/')) == 1) {
            found = f;
            break;
        }
    }
    appimage_string_list_free(files);
    return found;
}

QString AppImageReader::findIconFile(const QString &iconName)
{
    const QByteArray pathBytes = m_path.toUtf8();
    char **files = appimage_list_files(pathBytes.constData());
    if (!files)
        return {};

    // Build priority candidate list (same order as Python logic)
    QStringList candidates;
    if (!iconName.isEmpty()) {
        candidates << iconName + QLatin1String(".png");
        candidates << iconName + QLatin1String(".svg");
        candidates << QLatin1String("./") + iconName + QLatin1String(".png");
        candidates << QLatin1String("./") + iconName + QLatin1String(".svg");
        candidates << QStringLiteral("usr/share/icons/hicolor/256x256/apps/") + iconName + QLatin1String(".png");
        candidates << QStringLiteral("usr/share/icons/hicolor/scalable/apps/")  + iconName + QLatin1String(".svg");
    }
    candidates << QStringLiteral(".DirIcon");

    // Build a set of all listed paths for O(1) lookup
    QSet<QString> listed;
    for (int i = 0; files[i] != nullptr; ++i)
        listed.insert(QString::fromUtf8(files[i]));
    appimage_string_list_free(files);

    for (const QString &c : std::as_const(candidates)) {
        if (listed.contains(c))
            return c;
        // Try with and without leading "./"
        const QString alt = c.startsWith(QLatin1String("./")) ? c.mid(2) : (QLatin1String("./") + c);
        if (listed.contains(alt))
            return alt;
    }

    // Last resort: first .png or .svg at root level
    for (const QString &f : std::as_const(listed)) {
        const QString stripped = f.startsWith(QLatin1String("./")) ? f.mid(2) : f;
        const bool isRoot = !stripped.contains(QLatin1Char('/'));
        if (isRoot && (f.endsWith(QLatin1String(".png")) || f.endsWith(QLatin1String(".svg"))))
            return f;
    }

    return {};
}

QByteArray AppImageReader::readFileFromAppImage(const QString &innerPath, QString &outExt)
{
    const QByteArray pathBytes = m_path.toUtf8();
    const QByteArray innerBytes = innerPath.toUtf8();

    char *buf = nullptr;
    unsigned long bufSize = 0;

    const bool ok = appimage_read_file_into_buffer_following_symlinks(
        pathBytes.constData(), innerBytes.constData(), &buf, &bufSize);

    if (!ok || buf == nullptr || bufSize == 0) {
        if (buf) free(buf);
        return {};
    }

    QByteArray result(buf, static_cast<int>(bufSize));
    free(buf);

    outExt = QLatin1Char('.') + QFileInfo(innerPath).suffix();
    if (outExt == QLatin1String(".")) // .DirIcon has no extension
        outExt = QStringLiteral(".png");

    return result;
}

#endif // HAVE_LIBAPPIMAGE

// ──────────────────────────────────────────────────────────────────────────────
// squashfuse fallback path
// ──────────────────────────────────────────────────────────────────────────────

AppImageInfo AppImageReader::readWithSquashfuse()
{
    AppImageInfo info;
    info.originalName = QFileInfo(m_path).fileName();
    info.cleanName    = cleanName(info.originalName);
    info.appId        = extractAppId(info.originalName);
    info.appName      = info.cleanName;
    info.appName.remove(QLatin1String(".AppImage"), Qt::CaseInsensitive);
    info.version      = QStringLiteral("Unknown");
    info.fileSize     = QFileInfo(m_path).size();

    const int appImageType = detectType();
    if (appImageType == 1)
        qCWarning(AIM_LOG) << m_path << "is Type 1 (ISO9660); falling back to --appimage-extract";

    QTemporaryDir tempDir;
    if (!tempDir.isValid()) {
        qCWarning(AIM_LOG) << "Failed to create temp dir";
        return info;
    }

    const QString squashRoot = tempDir.filePath(QStringLiteral("squashfs-root"));
    bool mounted = false;

    if (appImageType == 2) {
        QDir().mkpath(squashRoot);
        mounted = mountWithSquashfuse(squashRoot);
    }

    if (!mounted) {
        if (appImageType == 2) {
            // squashfuse unavailable for a Type 2 AppImage — returning partial info immediately.
            // Install squashfuse or build with libappimage for full metadata extraction.
            qCWarning(AIM_LOG) << "squashfuse unavailable for" << m_path
                               << "— install squashfuse or libappimage to enable metadata/icon extraction";
            return info;
        }
        // Type 1 (ISO9660): --appimage-extract is the only option.
        QProcess extract;
        extract.setWorkingDirectory(tempDir.path());
        extract.start(m_path, {QStringLiteral("--appimage-extract")});
        if (!extract.waitForFinished(30000) || extract.exitCode() != 0) {
            qCWarning(AIM_LOG) << "AppImage extraction failed for" << m_path;
            return info;
        }
    }

    const AppImageInfo parsed = parseSquashfsRoot(squashRoot);
    if (parsed.isValid) {
        info = parsed;
        info.fileSize     = QFileInfo(m_path).size();
        info.originalName = QFileInfo(m_path).fileName();
        info.cleanName    = cleanName(info.originalName);
        info.appId        = extractAppId(info.originalName);
        if (info.appName.isEmpty())
            info.appName = info.cleanName;
        info.appName.remove(QLatin1String(".AppImage"), Qt::CaseInsensitive);
    }

    if (mounted)
        unmount(squashRoot);

    return info;
}

bool AppImageReader::mountWithSquashfuse(const QString &mountPoint)
{
    const QString squashfuse = QStandardPaths::findExecutable(QStringLiteral("squashfuse"));
    if (squashfuse.isEmpty())
        return false;

    QProcess p;
    p.start(squashfuse, {m_path, mountPoint});
    if (!p.waitForFinished(5000) || p.exitCode() != 0) {
        qCWarning(AIM_LOG) << "squashfuse failed for" << m_path;
        return false;
    }
    return true;
}

void AppImageReader::unmount(const QString &mountPoint)
{
    if (!QProcess::startDetached(QStringLiteral("fusermount3"), {QStringLiteral("-u"), mountPoint}))
        QProcess::startDetached(QStringLiteral("fusermount"), {QStringLiteral("-u"), mountPoint});
}

AppImageInfo AppImageReader::parseSquashfsRoot(const QString &squashRoot)
{
    AppImageInfo info;

    const QStringList desktopFiles = QDir(squashRoot).entryList(
        {QStringLiteral("*.desktop")}, QDir::Files);
    if (desktopFiles.isEmpty())
        return info;

    const QString desktopPath = squashRoot + QLatin1Char('/') + desktopFiles.first();
    KDesktopFile df(desktopPath);
    const KConfigGroup entry = df.desktopGroup();

    info.appName    = entry.readEntry(QStringLiteral("Name"));
    info.categories = entry.readEntry(QStringLiteral("Categories"));

    info.updateInfo = entry.readEntry(QStringLiteral("X-AppImage-Update-Information"));

    QString ver = entry.readEntry(QStringLiteral("X-AppImage-Version"));
    if (ver.isEmpty() && !info.updateInfo.isEmpty()) {
        static const QRegularExpression verInUrl(QStringLiteral(R"([-_](\d+\.\d+[\d.]*))"));
        const auto m = verInUrl.match(info.updateInfo);
        if (m.hasMatch())
            ver = m.captured(1);
    }
    info.version = ver.isEmpty() ? QStringLiteral("Unknown") : ver;

    const QString execFull = entry.readEntry(QStringLiteral("Exec"));
    const QStringList execParts = KShell::splitArgs(execFull);
    if (execParts.size() > 1) {
        QStringList args;
        for (int i = 1; i < execParts.size(); ++i)
            if (!execParts.at(i).startsWith(QLatin1Char('%')))
                args << KShell::quoteArg(execParts.at(i));
        info.execArgs = args.join(QLatin1Char(' '));
    }

    const QString iconName = entry.readEntry(QStringLiteral("Icon"));
    info.iconData = findIcon(squashRoot, iconName, info.iconExt);
    info.isValid  = true;
    return info;
}

QByteArray AppImageReader::findIcon(const QString &squashRoot,
                                    const QString &iconName,
                                    QString &outExt)
{
    QStringList candidates;
    if (!iconName.isEmpty()) {
        candidates << squashRoot + QLatin1Char('/') + iconName + QLatin1String(".png");
        candidates << squashRoot + QLatin1Char('/') + iconName + QLatin1String(".svg");
        candidates << squashRoot + QLatin1String("/usr/share/icons/hicolor/256x256/apps/") + iconName + QLatin1String(".png");
        candidates << squashRoot + QLatin1String("/usr/share/icons/hicolor/scalable/apps/")  + iconName + QLatin1String(".svg");
    }
    candidates << squashRoot + QLatin1String("/.DirIcon");

    if (iconName.isEmpty()) {
        const QStringList rootFiles = QDir(squashRoot).entryList(
            {QStringLiteral("*.png"), QStringLiteral("*.svg")}, QDir::Files);
        for (const QString &f : rootFiles)
            candidates << squashRoot + QLatin1Char('/') + f;
    }

    for (const QString &path : std::as_const(candidates)) {
        if (!QFile::exists(path))
            continue;
        outExt = QLatin1Char('.') + QFileInfo(path).suffix();
        if (outExt == QLatin1String("."))
            outExt = QStringLiteral(".png");
        QFile f(path);
        if (f.open(QIODevice::ReadOnly))
            return f.readAll();
    }
    return {};
}

int AppImageReader::detectType()
{
    QFile f(m_path);
    if (!f.open(QIODevice::ReadOnly))
        return 2;
    if (!f.seek(8))
        return 2;
    char byte = 0;
    f.read(&byte, 1);
    return (byte == 1) ? 1 : 2;
}
