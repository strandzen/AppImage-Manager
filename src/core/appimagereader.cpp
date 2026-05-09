// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#include "appimagereader.h"
#include "appimagecache.h"
#include "logging.h"

#include <KDesktopFile>
#include <KConfigGroup>
#include <KShell>

#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QXmlStreamReader>

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

#ifndef HAVE_LIBAPPIMAGE
    qCCritical(AIM_LOG) << "libappimage not available — cannot read AppImage metadata for" << m_path
                        << ". Rebuild with libappimage support.";
    return {};
#endif

    const qint64 mtime = QFileInfo(m_path).lastModified().toMSecsSinceEpoch();
    const AppImageInfo cached = AppImageCache::instance().load(m_path, mtime);
    if (cached.isValid)
        return cached;

    const AppImageInfo info = readWithLibappimage();
    if (info.isValid)
        AppImageCache::instance().store(m_path, mtime, info);
    return info;
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
                info.comment    = entry.readEntry(QStringLiteral("Comment"));

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
                // Find and read AppStream XML
                const QString appStreamInner = findAppStreamFile();
                if (!appStreamInner.isEmpty()) {
                    QString ext;
                    const QByteArray appStreamData = readFileFromAppImage(appStreamInner, ext);
                    if (!appStreamData.isEmpty())
                        extractMetadataFromXml(appStreamData, info);
                }

                if (info.description.isEmpty())
                    info.description = info.comment;
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

QString AppImageReader::findAppStreamFile()
{
    const QByteArray pathBytes = m_path.toUtf8();
    char **files = appimage_list_files(pathBytes.constData());
    if (!files)
        return {};

    QString found;
    for (int i = 0; files[i] != nullptr; ++i) {
        QString f = QString::fromUtf8(files[i]);
        QString stripped = f;
        if (stripped.startsWith(QLatin1String("./"))) {
            stripped = stripped.mid(2);
        }
        
        if ((stripped.startsWith(QLatin1String("usr/share/metainfo/")) || 
             stripped.startsWith(QLatin1String("usr/share/appdata/"))) &&
            (stripped.endsWith(QLatin1String(".appdata.xml")) || stripped.endsWith(QLatin1String(".metainfo.xml")))) {
            found = f; // Return original f for readFileFromAppImage
            break;
        }
    }
    appimage_string_list_free(files);
    return found;
}

void AppImageReader::extractMetadataFromXml(const QByteArray &xmlData, AppImageInfo &info)
{
    QXmlStreamReader xml(xmlData);
    QString description;
    bool inDescription = false;
    bool inDeveloper   = false;

    while (!xml.atEnd() && !xml.hasError()) {
        const auto token = xml.readNext();

        if (token == QXmlStreamReader::StartElement) {
            const auto ename = xml.name();

            if (ename == QLatin1String("description") && !inDescription) {
                inDescription = true;
            } else if (ename == QLatin1String("developer")) {
                inDeveloper = true;
            } else if (ename == QLatin1String("developer_name") && !inDescription) {
                if (info.developerName.isEmpty())
                    info.developerName = xml.readElementText().trimmed();
            } else if (ename == QLatin1String("name") && inDeveloper && !inDescription) {
                if (info.developerName.isEmpty())
                    info.developerName = xml.readElementText().trimmed();
            } else if (ename == QLatin1String("url") && !inDescription) {
                if (xml.attributes().value(QLatin1String("type")) == QLatin1String("homepage")
                        && info.homepage.isEmpty())
                    info.homepage = xml.readElementText().trimmed();
            } else if (inDescription) {
                if (ename == QLatin1String("p")) {
                    if (!description.isEmpty() && !description.endsWith(QLatin1String("<br><br>")))
                        description += QLatin1String("<br><br>");
                } else if (ename == QLatin1String("li")) {
                    if (!description.isEmpty() && !description.endsWith(QLatin1String("<br>")))
                        description += QLatin1String("<br>");
                    description += QStringLiteral("• ");
                }
            }
        } else if (token == QXmlStreamReader::EndElement) {
            const auto ename = xml.name();
            if (ename == QLatin1String("description"))
                inDescription = false;
            else if (ename == QLatin1String("developer"))
                inDeveloper = false;
        } else if (token == QXmlStreamReader::Characters && inDescription) {
            QString text = xml.text().toString().trimmed();
            if (!text.isEmpty()) {
                // Escape HTML — this text is injected into StyledText
                text.replace(QLatin1Char('&'), QLatin1String("&amp;"));
                text.replace(QLatin1Char('<'), QLatin1String("&lt;"));
                text.replace(QLatin1Char('>'), QLatin1String("&gt;"));
                description += text + QLatin1Char(' ');
            }
        }
    }
    info.description = description.trimmed();
}

#endif // HAVE_LIBAPPIMAGE
