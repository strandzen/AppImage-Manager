// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#pragma once

#include <QByteArray>
#include <QMetaType>
#include <QString>

struct AppImageInfo {
    QString originalName;
    QString cleanName;
    QString appId;
    QString appName;
    QString version;
    QString categories;
    QString execArgs;
    qint64 fileSize = 0;
    QByteArray iconData;
    QString iconExt;
    bool isValid = false;
};

Q_DECLARE_METATYPE(AppImageInfo)
