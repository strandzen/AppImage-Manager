// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#pragma once

#include <QString>
#include "appimagemanager_qml_export.h"

// Per-AppImage update source chosen by the user, overriding (or supplying) the
// update information embedded in the AppImage. Stored in appimagemanagerrc under
// [UpdateSources][<file name>], keyed by file name so moving the Applications
// folder keeps the settings. Read fresh on every load because the dashboard
// writes and the daemon (a separate process) reads.
enum class UpdateSourceType {
    None,     // use the embedded update information
    GitHub,   // value: "owner/repo"
    Forgejo,  // value: "host/owner/repo" (Codeberg, Forgejo, Gitea: same release API)
    Zsync,    // value: URL of a .zsync control file
    Url,      // value: direct download URL of the AppImage
};

struct UpdateSource {
    UpdateSourceType type = UpdateSourceType::None;
    QString value;
    bool prerelease = false;  // GitHub and Forgejo only
    QString installedTag;     // release tag (or direct-download build marker) installed by the last update
};

APPIMAGEMANAGER_EXPORT UpdateSource loadUpdateSource(const QString &filePath);
// Saves type, value and prerelease. Type None clears the override. Changing the
// source forgets installedTag, since it belonged to the old source.
APPIMAGEMANAGER_EXPORT void saveUpdateSource(const QString &filePath, const UpdateSource &source);
APPIMAGEMANAGER_EXPORT void saveInstalledTag(const QString &filePath, const QString &tag);

// Turns what the user typed into a source. `forced` None auto-detects:
//   "owner/repo" or a github.com URL        → GitHub
//   URL ending in .zsync                     → Zsync
//   URL ending in .AppImage                  → Url
//   https://host/owner/repo[/...] (no query) → Forgejo
//   any other http(s) URL                    → Url
// Returns type None when the input cannot be used as (or as the forced) type.
APPIMAGEMANAGER_EXPORT UpdateSource parseUpdateSourceInput(const QString &input,
                                                           UpdateSourceType forced = UpdateSourceType::None);

// Accepts "owner/repo" or a github.com URL ("https://github.com/owner/repo[/...]").
// Returns "owner/repo", or an empty string when the input is not a repository.
APPIMAGEMANAGER_EXPORT QString parseGitHubRepo(const QString &input);

// The update information to act on: the user's override when set, otherwise the
// embedded string. Overrides use the AppImage formats where one exists
// (gh-releases-zsync|, zsync|) and two internal ones:
//   forgejo-releases|host|owner|repo     and     direct|url
APPIMAGEMANAGER_EXPORT QString effectiveUpdateInfo(const QString &embedded, const UpdateSource &source);

// True when the update information is a kind this app can check.
APPIMAGEMANAGER_EXPORT bool isSupportedUpdateInfo(const QString &updateInfo);
// True for kinds checked by ReleaseChecker (everything except plain zsync).
APPIMAGEMANAGER_EXPORT bool isReleaseUpdateInfo(const QString &updateInfo);

// Human-readable source, e.g. "GitHub · owner/repo", "Codeberg · owner/repo",
// "Direct download · example.com". Empty for no or unsupported information.
APPIMAGEMANAGER_EXPORT QString describeUpdateInfo(const QString &updateInfo);
// The repository's web page for GitHub and Forgejo/Codeberg/Gitea sources ("" otherwise).
APPIMAGEMANAGER_EXPORT QString updateSourceWebUrl(const QString &updateInfo);

// Stable names used by QML: "github", "forgejo", "zsync", "url" ("" = None).
APPIMAGEMANAGER_EXPORT QString updateSourceTypeName(UpdateSourceType type);
APPIMAGEMANAGER_EXPORT UpdateSourceType updateSourceTypeFromName(const QString &name);
