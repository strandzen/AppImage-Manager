// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#pragma once

#include <QDateTime>
#include <QList>
#include <QString>
#include <QUrl>
#include "appimagemanager_qml_export.h"

// Release notes from GitHub and Forgejo/Codeberg/Gitea: the release list (one request,
// the same JSON the update check reads) holds each release's notes (Markdown "body").

struct ReleaseNote {
    QString tag;
    QString name;      // the release title; may be empty
    QString body;      // Markdown, images removed
    QString url;       // the release's web page
    QDateTime published;
    bool prerelease = false;
};

// The API URL of the release list for an update info ("" when it isn't a repository).
APPIMAGEMANAGER_EXPORT QUrl releaseListUrl(const QString &updateInfo);

// From the release list (newest first): drafts and (unless allowed) pre-releases dropped,
// then every release newer than the installed one, stopping at installedTag. Nothing newer
// (or the installed version unknown): just the newest release.
APPIMAGEMANAGER_EXPORT QList<ReleaseNote> pickReleaseNotes(const QByteArray &json, const QString &installedVersion,
                                                           const QString &installedTag, bool prereleases);

// Remote images would load when shown: they go, as do HTML comments (release templates).
APPIMAGEMANAGER_EXPORT QString cleanReleaseBody(const QString &markdown);
