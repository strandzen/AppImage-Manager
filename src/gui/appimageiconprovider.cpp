// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#include "appimageiconprovider.h"

#include <QPainter>
#include <QPixmap>
#include <QSvgRenderer>

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

    const auto it = m_icons.constFind(id);
    if (it == m_icons.constEnd() || it->data.isEmpty()) {
        if (size) *size = QSize(0, 0);
        return {};
    }

    QPixmap pixmap;
    if (it->ext.compare(QStringLiteral(".svg"), Qt::CaseInsensitive) == 0 ||
        it->ext.compare(QStringLiteral(".svgz"), Qt::CaseInsensitive) == 0) {
        
        QSvgRenderer renderer(it->data);
        if (renderer.isValid()) {
            const QSize target = (requestedSize.isValid() && !requestedSize.isEmpty())
                               ? requestedSize
                               : renderer.defaultSize();
            
            pixmap = QPixmap(target);
            pixmap.fill(Qt::transparent);
            QPainter painter(&pixmap);
            renderer.render(&painter);
            
            if (size)
                *size = target;
            return pixmap;
        }
    }

    pixmap.loadFromData(it->data);
    if (pixmap.isNull()) {
        if (size) *size = QSize(0, 0);
        return pixmap;
    }

    const QSize target = (requestedSize.isValid() && !requestedSize.isEmpty())
                       ? requestedSize
                       : pixmap.size();

    if (size)
        *size = pixmap.size();

    if (pixmap.size() == target)
        return pixmap;

    return pixmap.scaled(target, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}
