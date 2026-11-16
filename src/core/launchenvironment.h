// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#pragma once

#include <QList>
#include <QStringList>
#include "appimagemanager_qml_export.h"

// Per-AppImage launch environment ("KEY=value" assignments), e.g.
// WEBKIT_DISABLE_DMABUF_RENDERER=1 for apps that render blank. Stored in
// appimagemanagerrc under [Environment][<file name>] (keyed by file name like the
// update source, so updates keep it). Applied to launches from this app and to the
// app menu entry's Exec line.

struct EnvironmentParse {
    QStringList variables;    // valid "KEY=value" lines, in order
    QList<int> invalidLines;  // 1-based line numbers that are not KEY=value or repeat a KEY
};

// One assignment per line; blank lines and lines starting with '#' are ignored.
// KEY must match [A-Za-z_][A-Za-z0-9_]*.
APPIMAGEMANAGER_EXPORT EnvironmentParse parseEnvironment(const QString &text);

APPIMAGEMANAGER_EXPORT QStringList loadEnvironment(const QString &filePath);
APPIMAGEMANAGER_EXPORT void saveEnvironment(const QString &filePath, const QStringList &variables);

// Settings folder in Applications: the app keeps its settings in
// <folder of the AppImage>/.config/<name>/ (its XDG_CONFIG_HOME) instead of ~/.config.
// Stored as [ConfigFolder][<file name>] = <name>; no entry = off.
APPIMAGEMANAGER_EXPORT QString configFolderName(const QString &filePath);
APPIMAGEMANAGER_EXPORT void setConfigFolderName(const QString &filePath, const QString &name);
// The folder, or "" when off.
APPIMAGEMANAGER_EXPORT QString configFolderPath(const QString &filePath);
// "mailcraft" for mailcraft-0.13.0-linux-x86_64.AppImage.
APPIMAGEMANAGER_EXPORT QString defaultConfigFolderName(const QString &filePath);

// What a launch adds to the environment: the saved variables, then XDG_CONFIG_HOME and
// XDG_CONFIG_DIRS for the settings folder (unless the user set them). ~/.config stays in
// XDG_CONFIG_DIRS so the desktop's theme and fonts settings are still read.
APPIMAGEMANAGER_EXPORT QStringList launchVariables(const QString &filePath);

// Starts the AppImage detached with launchVariables() on top of our environment.
APPIMAGEMANAGER_EXPORT bool launchWithEnvironment(const QString &filePath);

// "env 'KEY=value' … " for a .desktop Exec line, or "" without variables.
APPIMAGEMANAGER_EXPORT QString environmentExecPrefix(const QStringList &variables);
