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

    // Drop any rendered pixmaps for this id — the underlying bytes just changed.
    const QString prefix = id + QLatin1Char(':');
    const auto keys = m_renderCache.keys();
    for (const QString &k : keys) {
        if (k.startsWith(prefix))
            m_renderCache.remove(k);
    }
}

static QString cacheKey(const QString &id, const QSize &size)
{
    return id + QLatin1Char(':') + QString::number(size.width())
              + QLatin1Char('x') + QString::number(size.height());
}

QPixmap AppImageIconProvider::requestPixmap(const QString &id,
                                             QSize *size,
                                             const QSize &requestedSize)
{
    QWriteLocker locker(&m_lock); // QCache::object mutates LRU

    const auto it = m_icons.constFind(id);
    if (it == m_icons.constEnd() || it->data.isEmpty()) {
        if (size) *size = QSize(0, 0);
        return {};
    }

    auto finishWithCached = [&](const QSize &actual) -> QPixmap {
        const QString key = cacheKey(id, actual);
        if (QPixmap *p = m_renderCache.object(key)) {
            if (size) *size = p->size();
            return *p;
        }
        return {};
    };

    QPixmap pixmap;
    if (it->ext.compare(QStringLiteral(".svg"), Qt::CaseInsensitive) == 0 ||
        it->ext.compare(QStringLiteral(".svgz"), Qt::CaseInsensitive) == 0) {

        QSvgRenderer renderer(it->data);
        if (!renderer.isValid()) {
            if (size) *size = QSize(0, 0);
            return {};
        }

        const QSize target = (requestedSize.isValid() && !requestedSize.isEmpty())
                           ? requestedSize
                           : renderer.defaultSize();

        if (QPixmap cached = finishWithCached(target); !cached.isNull())
            return cached;

        pixmap = QPixmap(target);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        renderer.render(&painter);

        if (size) *size = target;
        const int cost = std::max(1, pixmap.width() * pixmap.height() * pixmap.depth() / 8);
        m_renderCache.insert(cacheKey(id, target), new QPixmap(pixmap), cost);
        return pixmap;
    }

    pixmap.loadFromData(it->data);
    if (pixmap.isNull()) {
        if (size) *size = QSize(0, 0);
        return pixmap;
    }

    const QSize target = (requestedSize.isValid() && !requestedSize.isEmpty())
                       ? requestedSize
                       : pixmap.size();

    if (size) *size = pixmap.size();

    if (pixmap.size() == target) {
        const int cost = std::max(1, pixmap.width() * pixmap.height() * pixmap.depth() / 8);
        m_renderCache.insert(cacheKey(id, target), new QPixmap(pixmap), cost);
        return pixmap;
    }

    if (QPixmap cached = finishWithCached(target); !cached.isNull())
        return cached;

    QPixmap scaled = pixmap.scaled(target, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    const int cost = std::max(1, scaled.width() * scaled.height() * scaled.depth() / 8);
    m_renderCache.insert(cacheKey(id, target), new QPixmap(scaled), cost);
    return scaled;
}
