// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#pragma once

#include "appimageinfo.h"

#include <QString>

// Blocking metadata extractor for AppImage files.
// Requires libappimage for in-process SquashFS extraction.
// Always call from a worker thread via QtConcurrent::run.
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
    QByteArray readFileFromAppImage(const QString &innerPath, QString &outExt);
    QString findDesktopFile();
    QString findIconFile(const QString &iconName);
#endif

    QString m_path;
};
