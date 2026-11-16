// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#pragma once

#include <QJsonArray>
#include <QObject>
#include <QString>
#include <QUrl>
#include "appimagemanager_qml_export.h"

class QNetworkAccessManager;

// One-shot release checker. Construct, connect checkFinished, call check(), then deleteLater().
// Uses the AppImage's user-chosen update source (see updatesource.h) when set, otherwise its
// embedded update info, and handles:
//   gh-releases-zsync|owner|repo|tag|pattern  GitHub Releases API (Atom feed fallback)
//   forgejo-releases|host|owner|repo           Codeberg/Forgejo/Gitea API (same release JSON)
//   direct|url                                 HEAD request; Last-Modified (else ETag) is the build
// Emits checkFinished exactly once with one of four Result values:
//   UpdateAvailable — newer release/build; url is a .zsync or .AppImage download
//   UpToDate        — remote version is equal to or older than the installed one
//   NetworkFailed   — QNetworkReply reported an error (no connectivity, timeout, HTTP error)
//   Failed          — reply unusable: bad JSON, missing tag, no matching asset, a web page
//                     instead of an AppImage, no build marker, or malformed update info
class APPIMAGEMANAGER_EXPORT ReleaseChecker : public QObject
{
    Q_OBJECT
public:
    enum class Result { UpdateAvailable, UpToDate, NetworkFailed, Failed };
    Q_ENUM(Result)

    explicit ReleaseChecker(QNetworkAccessManager *nam, QObject *parent = nullptr);

    // filePath: the AppImage (its file name keys the update source; a full path also lets the
    // direct-download check compare against the file's modification time).
    void check(const QString &filePath, const QString &embeddedUpdateInfo, const QString &currentVersion);

    // Why the last check failed (Failed/NetworkFailed), for people: "Repository not found" etc.
    QString errorText() const { return m_error; }

    // True for Content-Type values that cannot be an AppImage (web pages, JSON, XML, text).
    static bool isNonBinaryContentType(const QString &contentType);

    // Picks the download URL from a release's assets: a .zsync control file first (delta
    // updates), then an .AppImage. Assets must match `pattern` (glob, may be empty) and this
    // machine's CPU architecture. With several candidates left, the one whose name minus version
    // numbers equals localFileName's wins; otherwise the pick is ambiguous and empty is returned.
    static QString pickAsset(const QJsonArray &assets, const QString &pattern,
                             const QString &localFileName, const QString &cpuArch);
    // Whether an asset name fits the given CPU architecture (QSysInfo::currentCpuArchitecture()).
    static bool matchesArchitecture(const QString &assetName, const QString &cpuArch);

Q_SIGNALS:
    void checkFinished(ReleaseChecker::Result result,
                       const QString &newVersion, const QString &url);

private:
    void checkReleases(const QUrl &url, bool github);
    void checkAtomFeed();
    void checkDirect(const QUrl &url);
    void finishWithTag(const QString &tag, const QString &url);

    QNetworkAccessManager *m_nam;
    QStringList m_parts;      // split update info
    QString m_filePath;
    QString m_fileName;
    QString m_currentVersion;
    QString m_installedTag;
    QString m_error;
};
