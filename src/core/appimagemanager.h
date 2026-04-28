// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#pragma once

#include "appimageinfo.h"

#include <QList>
#include <QObject>
#include <QPair>
#include <QUrl>

namespace KIO { class CopyJob; }
class KJob;

class AppImageManager : public QObject
{
    Q_OBJECT
public:
    explicit AppImageManager(QObject *parent = nullptr);

    // Returns a KIO::CopyJob*. Connect to KJob::result() before calling start().
    KIO::CopyJob *installAppImage(const QUrl &source, const QString &applicationsDir);

    bool createDesktopLink(const QString &appImagePath, const AppImageInfo &info);
    bool removeDesktopLink(const QString &appImagePath, const AppImageInfo &info);
    bool isDesktopLinkEnabled(const QString &appImagePath, const AppImageInfo &info) const;

    // Blocking — call via QtConcurrent::run.
    QList<QPair<QString, qint64>> findCorpses(const AppImageInfo &info) const;

    // Moves appImageUrl and all corpseUrls to Trash. Returns a KIO::CopyJob*.
    KJob *removeItems(const QUrl &appImageUrl,
                      const AppImageInfo &info,
                      const QList<QUrl> &corpseUrls);

    static QString desktopFilePath(const AppImageInfo &info);
    static QString iconFilePath(const AppImageInfo &info);

private:
    static qint64 dirSize(const QString &path);
    void rebuildSycoca();
};
