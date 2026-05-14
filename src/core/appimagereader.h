// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#pragma once

#include "appimageinfo.h"

#include <QString>

// Blocking metadata extractor for AppImage files.
// Always call from a worker thread via QtConcurrent::run — never on the main thread.
//
// Two read paths, selected at build time:
//   HAVE_LIBAPPIMAGE — in-process SquashFS via libappimage (preferred; faster, no subprocess).
//   fallback         — mounts via squashfuse subprocess, parses, then unmounts with fusermount3.
//
// Both paths populate the same AppImageInfo fields; the libappimage path additionally
// extracts AppStream XML (description, developerName, homepage) from the embedded metainfo file.
class AppImageReader
{
public:
    explicit AppImageReader(const QString &path);

    AppImageInfo read();

    static QString cleanName(const QString &filename);

private:
    static QString extractAppId(const QString &filename);
    static QString versionFromFilename(const QString &filename);

#ifdef HAVE_LIBAPPIMAGE
    AppImageInfo readWithLibappimage();
    QByteArray readFileFromAppImage(const QString &innerPath, QString *outExt = nullptr);
    QString findDesktopFile();
    QString findIconFile(const QString &iconName);
    QString findAppStreamFile();
    void extractMetadataFromXml(const QByteArray &xmlData, AppImageInfo &info);
#endif

    QString m_path;
};
