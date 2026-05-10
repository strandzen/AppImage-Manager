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

    char **files = appimage_list_files(pathBytes.constData());
    if (!files) {
        qCWarning(AIM_LOG) << "Failed to list files in" << m_path;
        return info;
    }

    QStringList fileList;
    for (int i = 0; files[i] != nullptr; ++i)
        fileList << QString::fromUtf8(files[i]);
    appimage_string_list_free(files);

    // Find .desktop file
    QString desktopInner;
    for (const QString &f : fileList) {
        if (f.endsWith(QLatin1String(".desktop")) && !f.contains(QLatin1Char('/'))) {
            desktopInner = f;
            break;
        }
        if (f.startsWith(QLatin1String("./")) && f.endsWith(QLatin1String(".desktop"))
                && f.count(QLatin1Char('/')) == 1) {
            desktopInner = f;
            break;
        }
    }

    if (desktopInner.isEmpty()) {
        qCWarning(AIM_LOG) << "No .desktop file found in" << m_path;
        info.isValid = true;
        return info;
    }

    // Read .desktop file into a temporary file for KDesktopFile parsing
    const QByteArray desktopData = readFileFromAppImage(desktopInner);
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
                QString iconInner;
                
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

                QSet<QString> listedSet(fileList.begin(), fileList.end());
                for (const QString &c : candidates) {
                    if (listedSet.contains(c)) {
                        iconInner = c;
                        break;
                    }
                    const QString alt = c.startsWith(QLatin1String("./")) ? c.mid(2) : (QLatin1String("./") + c);
                    if (listedSet.contains(alt)) {
                        iconInner = alt;
                        break;
                    }
                }
                
                if (iconInner.isEmpty()) {
                    for (const QString &f : fileList) {
                        const QString stripped = f.startsWith(QLatin1String("./")) ? f.mid(2) : f;
                        if (!stripped.contains(QLatin1Char('/')) && (f.endsWith(QLatin1String(".png")) || f.endsWith(QLatin1String(".svg")))) {
                            iconInner = f;
                            break;
                        }
                    }
                }

                if (!iconInner.isEmpty()) {
                    info.iconData = readFileFromAppImage(iconInner, &info.iconExt);
                }

                // Find and read AppStream XML
                QString appStreamInner;
                for (const QString &f : fileList) {
                    QString stripped = f;
                    if (stripped.startsWith(QLatin1String("./"))) stripped = stripped.mid(2);
                    if ((stripped.startsWith(QLatin1String("usr/share/metainfo/")) || 
                         stripped.startsWith(QLatin1String("usr/share/appdata/"))) &&
                        (stripped.endsWith(QLatin1String(".appdata.xml")) || stripped.endsWith(QLatin1String(".metainfo.xml")))) {
                        appStreamInner = f;
                        break;
                    }
                }

                if (!appStreamInner.isEmpty()) {
                    const QByteArray appStreamData = readFileFromAppImage(appStreamInner);
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


QByteArray AppImageReader::readFileFromAppImage(const QString &innerPath, QString *outExt)
{
    const QByteArray pathBytes = m_path.toUtf8();
    const QByteArray innerBytes = innerPath.toUtf8();

    char *buf = nullptr;
    unsigned long bufSize = 0;

    const bool ok = appimage_read_file_into_buffer_following_symlinks(
        pathBytes.constData(), innerBytes.constData(), &buf, &bufSize);

    if (!ok || buf == nullptr || bufSize == 0) {
        if (buf) std::free(buf);
        return {};
    }

    QByteArray result(buf, static_cast<qsizetype>(bufSize));
    std::free(buf);

    if (outExt) {
        *outExt = QLatin1Char('.') + QFileInfo(innerPath).suffix();
        if (*outExt == QLatin1String(".")) // .DirIcon has no extension
            *outExt = QStringLiteral(".png");
    }

    return result;
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
