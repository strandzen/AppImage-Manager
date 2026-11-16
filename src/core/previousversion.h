// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#pragma once

#include <QString>
#include "appimagemanager_qml_export.h"

// Undo for updates: an update keeps the file it replaced as
// <folder of the AppImage>/.previous/<file name> (one per AppImage, replaced by the next
// update). Its version and installed release tag are in appimagemanagerrc
// [Previous][<file name>] (Version, Tag).

// <folder>/.previous/<file name>, whether or not it exists.
APPIMAGEMANAGER_EXPORT QString previousFilePath(const QString &filePath);
// The kept version ("" when nothing is kept). Unknown versions read "".
APPIMAGEMANAGER_EXPORT QString previousVersion(const QString &filePath);
APPIMAGEMANAGER_EXPORT bool hasPrevious(const QString &filePath);

// Moves oldFile (the replaced AppImage) to the .previous folder, remembering its version
// and the installed tag the AppImage had before the update. Returns false if it could not
// be moved (oldFile is then left as it is).
APPIMAGEMANAGER_EXPORT bool keepPrevious(const QString &filePath, const QString &oldFile, const QString &version);

// Swaps the AppImage with the kept one (so restoring can be undone the same way), along
// with their versions and installed tags. currentVersion: the version being put aside.
APPIMAGEMANAGER_EXPORT bool restorePrevious(const QString &filePath, const QString &currentVersion);

// Forgets the kept copy (the file itself is not touched).
APPIMAGEMANAGER_EXPORT void forgetPrevious(const QString &filePath);
