// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#pragma once

#include "appimagemanager_qml_export.h"

#include <QCache>
#include <QMutex>
#include <QPixmap>
#include <QQuickImageProvider>
#include <QString>

// Provides rendered pixmaps for icons extracted from AppImages.
// Registered with each QML engine as "image://appimage/<id>".
//
// Raw icon bytes are stored in the process-wide AppImageIconStore singleton so
// multiple engine instances (dashboard + manage windows) share one copy of the data.
// Each provider instance keeps its own QPixmap render cache — pixmaps cannot be
// shared across engines because they are bound to a specific rendering context.
//
// Thread-safety: setIconData() may be called from worker threads;
// requestPixmap() is called from the QML/render thread.
class APPIMAGEMANAGER_EXPORT AppImageIconProvider : public QQuickImageProvider
{
public:
    AppImageIconProvider();

    // Backward-compatible overload used by AppImageBackend — stores as key "icon".
    void setIconData(const QByteArray &data, const QString &ext);

    // Multi-key overload used by AppImageListModel.
    void setIconData(const QString &id, const QByteArray &data, const QString &ext);

    QPixmap requestPixmap(const QString &id,
                          QSize *size,
                          const QSize &requestedSize) override;

private:
    // Rendered pixmaps keyed by "id:WxH"; capped at ~32 MB so it cannot grow
    // unbounded with hundreds of dashboard delegates requesting many sizes.
    // Per-engine: pixmaps are not safe to share across QML engine instances.
    QCache<QString, QPixmap> m_renderCache { 32 * 1024 * 1024 };
    mutable QMutex           m_cacheMutex;
};
