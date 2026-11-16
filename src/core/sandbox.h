// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#pragma once

#include <QStringList>
#include "appimagemanager_qml_export.h"

// Running an AppImage in a bubblewrap sandbox. The system folders are read-only, the home
// folder is empty except for the app's settings folder (Applications/.config/<app>, also its
// data and cache) and the folders the user gave it; display, sound, network and the session
// bus stay available. The AppImage is mounted outside (its own --appimage-mount) and its
// AppRun started inside: `appimagemanager --run <file>` does both and waits.
// Stored in appimagemanagerrc [Sandbox][<file name>]: Enabled (absent = follow sandboxAll), Folders.

struct SandboxSettings {
    bool enabled = false;
    QStringList folders;  // extra folders it may read and write
};

APPIMAGEMANAGER_EXPORT SandboxSettings loadSandbox(const QString &filePath);
APPIMAGEMANAGER_EXPORT void saveSandbox(const QString &filePath, const SandboxSettings &settings);
// bwrap is installed.
APPIMAGEMANAGER_EXPORT bool sandboxAvailable();
// Settings → Security "Sandbox all AppImages" (only with bwrap installed). An app without its
// own choice follows it; saveSandbox() stores a choice only where it differs.
APPIMAGEMANAGER_EXPORT bool sandboxAllEnabled();
// Settings → Security: folders every sandboxed app may use.
APPIMAGEMANAGER_EXPORT QStringList sharedSandboxFolders();

// bwrap's arguments, up to and including the command (mountPoint/AppRun) and args.
// home, runtimeDir: the user's; settingsFolder: bound read-write.
APPIMAGEMANAGER_EXPORT QStringList sandboxArguments(const QString &appImage, const QString &mountPoint,
                                                    const QString &settingsFolder, const QStringList &folders,
                                                    const QString &home, const QString &runtimeDir,
                                                    const QStringList &args = {});

// `appimagemanager --run <file> [args]`: starts the AppImage the way its settings say
// (sandboxed or not, with its environment) and waits for it. Returns its exit code.
APPIMAGEMANAGER_EXPORT int runAppImage(const QString &filePath, const QStringList &args);

// The command that starts the AppImage the way its settings say, for launches and the app
// menu entry: the AppImage itself, or `appimagemanager --run <file>` when sandboxed.
APPIMAGEMANAGER_EXPORT QStringList launchCommand(const QString &filePath);
