// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#pragma once

#include "appimageinfo.h"

#include <QString>

// Blocking metadata extractor for AppImage files.
// Preferred path: libappimage (in-process, no FUSE required).
// Fallback path:  squashfuse subprocess + fusermount3.
// Always call from a worker thread via QtConcurrent::run.
class AppImageReader
{
public:
    explicit AppImageReader(const QString &path);

    AppImageInfo read();

private:
    // Shared helpers
    static QString cleanName(const QString &filename);
    static QString extractAppId(const QString &filename);
    static QString versionFromFilename(const QString &filename);

#ifdef HAVE_LIBAPPIMAGE
    AppImageInfo readWithLibappimage();
    QByteArray readFileFromAppImage(const QString &innerPath, QString &outExt);
    QString findDesktopFile();
    QString findIconFile(const QString &iconName);
#endif

    // squashfuse fallback path
    AppImageInfo readWithSquashfuse();
    bool mountWithSquashfuse(const QString &mountPoint);
    void unmount(const QString &mountPoint);
    AppImageInfo parseSquashfsRoot(const QString &squashRoot);
    QByteArray findIcon(const QString &squashRoot, const QString &iconName, QString &outExt);
    int detectType(); // reads ELF magic at offset 8 → 1 or 2

    QString m_path;
};
