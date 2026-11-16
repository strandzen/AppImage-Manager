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

    // Checks the signature appimagetool --sign embeds: .sha256_sig (armored, over the hex
    // SHA-256 of the file with the signature, key and digest sections zeroed) against the
    // public key in .sig_key, in a keyring of its own. Zero-filled sections = Unsigned.
    // fingerprint: the signing key's, when Valid. Blocking (reads the file, runs gpg).
    static SignatureState verifySignature(const QString &path, QString *fingerprint = nullptr);

private:
    static QString extractAppId(const QString &filename);
    static QString versionFromFilename(const QString &filename);
    void extractMetadataFromXml(const QByteArray &xmlData, AppImageInfo &info);

    AppImageInfo readInternal();
    QByteArray readFileFromAppImage(const QString &innerPath, QString *outExt = nullptr);

    QString m_path;
};
