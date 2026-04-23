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
class AppImageIconProvider : public QQuickImageProvider
{
public:
    AppImageIconProvider();

    void setIconData(const QByteArray &data, const QString &ext);

    QPixmap requestPixmap(const QString &id,
                          QSize *size,
                          const QSize &requestedSize) override;

private:
    QByteArray m_data;
    QString m_ext;
    mutable QReadWriteLock m_lock;
};
