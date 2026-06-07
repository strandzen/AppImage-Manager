// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#pragma once

#include <QByteArray>
#include <QList>
#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QVersionNumber>

// Result of a GPG signature check on a Type 2 AppImage.
// Stored in AppImageInfo so the result survives the AppImageReader round-trip.
enum class SignatureState : int {
    Unchecked      = 0, // not yet attempted
    Valid          = 1, // .sha256_sig present, gpg confirms it
    Invalid        = 2, // .sha256_sig present, gpg rejects it (tampered / wrong key)
    Unsigned       = 3, // no .sha256_sig ELF section found
    GpgUnavailable = 4, // section present but gpg/gpg2 not on PATH
};

// Metadata extracted from a single AppImage file.
// `description` holds AppStream XML prose (from usr/share/metainfo/*.appdata.xml).
// Falls back to the .desktop `Comment=` field (stored in `comment`) when no XML is present.
struct AppImageInfo {
    QString originalName;
    QString cleanName;
    QString appId;
    QString appName;
    QString version;
    QString categories;
    QString comment;
    QString description;
    QString developerName;
    QString homepage;
    QString execArgs;
    qint64 fileSize = 0;
    QByteArray iconData;
    QString iconExt;
    QString updateInfo;
    bool isValid = false;
};

Q_DECLARE_METATYPE(AppImageInfo)

// ── Version utilities ─────────────────────────────────────────────────────────

inline QString normalizeVersion(const QString &v)
{
    if (!v.isEmpty() && (v.front() == QLatin1Char('v') || v.front() == QLatin1Char('V')))
        return v.mid(1);
    return v;
}

// Returns true only when remote is *strictly* newer than local.
// Equal versions return false.
inline bool isNewerVersion(const QString &remote, const QString &local)
{
    const QVersionNumber r = QVersionNumber::fromString(remote);
    const QVersionNumber l = QVersionNumber::fromString(local);
    return r > l;
}

// ── File filter ───────────────────────────────────────────────────────────────

inline const QStringList &kAppImageFilters()
{
    static const QStringList filters = { QStringLiteral("*.AppImage"), QStringLiteral("*.appimage") };
    return filters;
}

inline bool isRunningInSandbox()
{
    static const bool s = !qgetenv("FLATPAK_ID").isEmpty()
                       || !qgetenv("SNAP").isEmpty();
    return s;
}
