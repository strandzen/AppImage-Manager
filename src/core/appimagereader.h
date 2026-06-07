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

    // Check for an embedded GPG signature in a Type 2 AppImage.
    // Parses the ELF section table for .sha256_sig / .sha256_hash sections,
    // then invokes gpg to verify the detached signature against the stored hash.
    // Safe to call from a worker thread (all I/O is synchronous; gpg is a subprocess).
    static SignatureState verifySignature(const QString &path);

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
