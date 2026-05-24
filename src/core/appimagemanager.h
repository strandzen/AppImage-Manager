// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#pragma once

#include "appimageinfo.h"

#include <QList>
#include <QPair>
#include <QString>
#include <QUrl>

class KJob;
namespace KIO { class CopyJob; }

namespace AppImageManager {

    enum class CorpseConfidence { Low = 0, High = 1 };

    struct CorpseEntry {
        QString path;
        qint64 size = 0;
        CorpseConfidence confidence = CorpseConfidence::Low;
    };

    // Returns a KIO::CopyJob*. Connect to KJob::result() before calling start().
    KIO::CopyJob *installAppImage(const QUrl &source, const QString &applicationsDir);

    bool createDesktopLink(const QString &appImagePath, const AppImageInfo &info);
    bool removeDesktopLink(const QString &appImagePath, const AppImageInfo &info);
    bool isDesktopLinkEnabled(const QString &appImagePath, const AppImageInfo &info);

    // Blocking — call via QtConcurrent::run.
    // Returns entries sorted High confidence first, Low confidence second.
    QList<CorpseEntry> findCorpses(const AppImageInfo &info);

    // Moves appImageUrl and all corpseUrls to Trash. Returns a KIO::CopyJob*.
    KJob *removeItems(const QUrl &appImageUrl,
                      const AppImageInfo &info,
                      const QList<QUrl> &corpseUrls);

    QString desktopFilePath(const AppImageInfo &info);
    QString iconFilePath(const AppImageInfo &info);

    // Fire-and-forget rebuild of Plasma's service cache so a newly written or
    // removed .desktop file shows up in the launcher within ~1 s.
    void rebuildSycoca();
}
