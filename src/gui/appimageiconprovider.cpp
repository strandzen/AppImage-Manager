// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#include "appimageiconprovider.h"

#include <QPixmap>

AppImageIconProvider::AppImageIconProvider()
    : QQuickImageProvider(QQuickImageProvider::Pixmap)
{
}

void AppImageIconProvider::setIconData(const QByteArray &data, const QString &ext)
{
    setIconData(QStringLiteral("icon"), data, ext);
}

void AppImageIconProvider::setIconData(const QString &id, const QByteArray &data, const QString &ext)
{
    QWriteLocker locker(&m_lock);
    m_icons.insert(id, {data, ext});
}

QPixmap AppImageIconProvider::requestPixmap(const QString &id,
                                             QSize *size,
                                             const QSize &requestedSize)
{
    QReadLocker locker(&m_lock);

    QPixmap pixmap;
    const auto it = m_icons.constFind(id);
    if (it != m_icons.constEnd() && !it->data.isEmpty())
        pixmap.loadFromData(it->data);

    if (pixmap.isNull())
        return pixmap;

    const QSize target = (requestedSize.isValid() && !requestedSize.isEmpty())
                       ? requestedSize
                       : pixmap.size();

    if (size)
        *size = pixmap.size();

    return pixmap.scaled(target, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}
