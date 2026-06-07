// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#include "appimagereader.h"
#include "appimagecache.h"
#include "logging.h"

#include <KDesktopFile>
#include <KConfigGroup>
#include <KShell>

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLocale>
#include <QProcess>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QXmlStreamReader>

#include <appimage/appimage.h>

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

    const qint64 mtime = QFileInfo(m_path).lastModified().toMSecsSinceEpoch();
    const AppImageInfo cached = AppImageCache::instance().load(m_path, mtime);
    if (cached.isValid)
        return cached;

    const AppImageInfo info = readInternal();

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

AppImageInfo AppImageReader::readInternal()
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

    info.appId = QFileInfo(desktopInner).baseName();

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
                QStringList args;
                QString placeholder = QStringLiteral("%U");
                for (int i = 1; i < execParts.size(); ++i) {
                    if (execParts.at(i).startsWith(QLatin1Char('%'))) {
                        placeholder = execParts.at(i);
                    } else {
                        args << KShell::quoteArg(execParts.at(i));
                    }
                }
                args << placeholder;
                info.execArgs = args.join(QLatin1Char(' '));

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

// ──────────────────────────────────────────────────────────────────────────────
// Signature verification
// ──────────────────────────────────────────────────────────────────────────────

// Read the raw bytes of a named section from an ELF64 little-endian binary.
// Returns empty QByteArray if the section is absent or the file is not ELF64 LE.
static QByteArray readElfSection(const QString &path, const QByteArray &sectionName)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};

    const QByteArray hdr = f.read(64);
    if (hdr.size() < 64)
        return {};
    // ELF magic + ELF64 (class=2) + little-endian (data=1)
    if (hdr[0] != '\x7f' || hdr[1] != 'E' || hdr[2] != 'L' || hdr[3] != 'F'
            || hdr[4] != 2 || hdr[5] != 1)
        return {};

    auto rd16 = [&](int off) -> quint16 {
        return static_cast<quint8>(hdr[off])
             | (static_cast<quint8>(hdr[off + 1]) << 8);
    };
    auto rd64 = [&](const QByteArray &src, int off) -> quint64 {
        quint64 v = 0;
        for (int i = 0; i < 8; ++i)
            v |= static_cast<quint64>(static_cast<quint8>(src[off + i])) << (8 * i);
        return v;
    };

    const quint64 shoff    = rd64(hdr, 40);
    const quint16 shentsz  = rd16(58);
    const quint16 shnum    = rd16(60);
    const quint16 shstrndx = rd16(62);

    if (shoff == 0 || shentsz < 64 || shnum == 0 || shstrndx >= shnum)
        return {};

    if (!f.seek(static_cast<qint64>(shoff)))
        return {};
    const QByteArray shdrs = f.read(static_cast<qint64>(shentsz) * shnum);
    if (shdrs.size() < static_cast<int>(shentsz) * shnum)
        return {};

    // Load the section-name string table
    const int strBase = shstrndx * shentsz;
    const quint64 strOff  = rd64(shdrs, strBase + 24);
    const quint64 strSize = rd64(shdrs, strBase + 32);
    if (!f.seek(static_cast<qint64>(strOff)))
        return {};
    const QByteArray strtab = f.read(static_cast<qint64>(strSize));

    // Walk sections looking for sectionName
    for (int i = 0; i < shnum; ++i) {
        const int base = i * shentsz;
        quint32 nameIdx = 0;
        std::memcpy(&nameIdx, shdrs.constData() + base, 4);
        if (nameIdx >= static_cast<quint32>(strtab.size()))
            continue;
        if (QByteArray(strtab.constData() + nameIdx) != sectionName)
            continue;

        const quint64 secOff  = rd64(shdrs, base + 24);
        const quint64 secSize = rd64(shdrs, base + 32);
        if (!f.seek(static_cast<qint64>(secOff)))
            return {};
        return f.read(static_cast<qint64>(secSize));
    }
    return {};
}

// static
SignatureState AppImageReader::verifySignature(const QString &path)
{
    // Try sha256 first, then sha1 (older appimagetool used sha1)
    QByteArray sigData  = readElfSection(path, ".sha256_sig");
    QByteArray hashData = readElfSection(path, ".sha256_hash");
    if (sigData.isEmpty()) {
        sigData  = readElfSection(path, ".sha1_sig");
        hashData = readElfSection(path, ".sha1_hash");
    }

    if (sigData.isEmpty())
        return SignatureState::Unsigned;

    if (hashData.isEmpty()) {
        qCWarning(AIM_LOG) << "verifySignature:" << path
                           << "has a signature section but no hash section — corrupt?";
        return SignatureState::Invalid;
    }

    // Locate gpg
    QString gpgExe = QStandardPaths::findExecutable(QStringLiteral("gpg2"));
    if (gpgExe.isEmpty())
        gpgExe = QStandardPaths::findExecutable(QStringLiteral("gpg"));
    if (gpgExe.isEmpty())
        return SignatureState::GpgUnavailable;

    QTemporaryDir tmp;
    if (!tmp.isValid())
        return SignatureState::Unchecked;

    auto writeTemp = [&](const QString &name, const QByteArray &data) -> QString {
        const QString fp = tmp.filePath(name);
        QFile f(fp);
        if (!f.open(QIODevice::WriteOnly) || f.write(data) != data.size())
            return {};
        return fp;
    };

    const QString sigFile  = writeTemp(QStringLiteral("app.sig"),  sigData);
    const QString hashFile = writeTemp(QStringLiteral("app.hash"), hashData);
    if (sigFile.isEmpty() || hashFile.isEmpty())
        return SignatureState::Unchecked;

    // gpg --verify <detached-sig> <signed-data>
    QProcess gpg;
    gpg.start(gpgExe, { QStringLiteral("--verify"), sigFile, hashFile });
    if (!gpg.waitForFinished(15000)) {
        gpg.kill();
        return SignatureState::Unchecked;
    }

    return gpg.exitCode() == 0 ? SignatureState::Valid : SignatureState::Invalid;
}

void AppImageReader::extractMetadataFromXml(const QByteArray &xmlData, AppImageInfo &info)
{
    QXmlStreamReader xml(xmlData);
    QMap<QString, QString> langDescriptions;
    QString activeLang;
    QString descLang;
    bool inDescription = false;
    bool inDeveloper   = false;

    while (!xml.atEnd() && !xml.hasError()) {
        const auto token = xml.readNext();

        if (token == QXmlStreamReader::StartElement) {
            const auto ename = xml.name();

            if (ename == QLatin1String("description") && !inDescription) {
                inDescription = true;
                descLang = xml.attributes().value(QStringLiteral("xml:lang")).toString().toLower();
                activeLang = descLang;
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
                const QString itemLang = xml.attributes().value(QStringLiteral("xml:lang")).toString().toLower();
                if (!itemLang.isEmpty())
                    activeLang = itemLang;
                else
                    activeLang = descLang;

                QString &descRef = langDescriptions[activeLang];

                if (ename == QLatin1String("p")) {
                    if (!descRef.isEmpty() && !descRef.endsWith(QLatin1String("<br><br>")))
                        descRef += QLatin1String("<br><br>");
                } else if (ename == QLatin1String("li")) {
                    if (!descRef.isEmpty() && !descRef.endsWith(QLatin1String("<br>")))
                        descRef += QLatin1String("<br>");
                    descRef += QStringLiteral("• ");
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
                langDescriptions[activeLang] += text + QLatin1Char(' ');
            }
        }
    }

    // Determine user's preferred UI languages (e.g. "de", "fr")
    QStringList systemLangs;
    for (const QString &lang : QLocale::system().uiLanguages()) {
        const QString code = lang.split(QLatin1Char('-')).first().toLower();
        if (!systemLangs.contains(code))
            systemLangs.append(code);
    }
    if (!systemLangs.contains(QStringLiteral("en")))
        systemLangs.append(QStringLiteral("en"));

    // Find the best description matching user's locale
    QString selectedDescription;
    for (const QString &lang : systemLangs) {
        if (langDescriptions.contains(lang)) {
            selectedDescription = langDescriptions.value(lang);
            break;
        }
    }
    if (selectedDescription.isEmpty()) {
        // Fallback to default (empty string key)
        selectedDescription = langDescriptions.value(QString());
    }
    if (selectedDescription.isEmpty() && !langDescriptions.isEmpty()) {
        // Fallback to any available language description
        selectedDescription = langDescriptions.first();
    }

    info.description = selectedDescription.trimmed();
}


