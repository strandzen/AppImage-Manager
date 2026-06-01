// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#pragma once

#include "appimagemanager_qml_export.h"

#include <QDBusAbstractAdaptor>
#include <QVariantList>

class AppImageListModel;

// Read-only D-Bus interface for querying installed AppImages.
// Interface: io.github.appimagemanager.Manager1
// Object path: /io/github/appimagemanager/Manager
// Service name: io.github.appimagemanager.Manager1
class APPIMAGEMANAGER_EXPORT AppImageDBusAdaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "io.github.appimagemanager.Manager1")

public:
    explicit AppImageDBusAdaptor(AppImageListModel *model);

public Q_SLOTS:
    // Returns a list of maps with keys: path, name, version, hasDesktopLink
    QVariantList InstalledAppImages() const;

    QString Version() const;

private:
    AppImageListModel *m_model;
};
