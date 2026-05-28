// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#pragma once

#include "appimageinfo.h"

#include <QString>

// Blocking metadata extractor for AppImage files.
// Always call from a worker thread via QtConcurrent::run — never on the main thread.
//
// Requires libappimage for in-process SquashFS metadata extraction.
class AppImageReader
{
public:
    explicit AppImageReader(const QString &path);

    AppImageInfo read();

    static QString cleanName(const QString &filename);

private:
    static QString extractAppId(const QString &filename);
    static QString versionFromFilename(const QString &filename);
    void extractMetadataFromXml(const QByteArray &xmlData, AppImageInfo &info);

    AppImageInfo readInternal();
    QByteArray readFileFromAppImage(const QString &innerPath, QString *outExt = nullptr);
    QString findDesktopFile();
    QString findIconFile(const QString &iconName);
    QString findAppStreamFile();

    QString m_path;
};
