// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#pragma once

#include <QByteArray>
#include <QQuickImageProvider>
#include <QReadWriteLock>
#include <QString>

// Provides the raw icon bytes extracted from inside an AppImage.
// Registered with the QML engine as "image://appimage/icon".
// Thread-safe: setIconData() is called from a worker thread,
// requestPixmap() is called from the QML/render thread.
//
// Supports multiple icons keyed by an arbitrary string id.
// The single-file manage window uses the "icon" key via the convenience overload.
// The dashboard registers per-file icons using iconIdForPath() keys.
class AppImageIconProvider : public QQuickImageProvider
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
    struct IconEntry { QByteArray data; QString ext; };
    QHash<QString, IconEntry> m_icons;
    mutable QReadWriteLock m_lock;
};
