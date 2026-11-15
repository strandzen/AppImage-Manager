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

// Starts the AppImage detached with its saved environment on top of ours.
APPIMAGEMANAGER_EXPORT bool launchWithEnvironment(const QString &filePath);

// "env 'KEY=value' … " for a .desktop Exec line, or "" without variables.
APPIMAGEMANAGER_EXPORT QString environmentExecPrefix(const QStringList &variables);
