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
    {
        QWriteLocker locker(&m_iconsLock);
        m_icons.insert(id, {data, ext});
    }
    // Drop any rendered pixmaps for this id — the underlying bytes just changed.
    {
        QMutexLocker cacheLocker(&m_cacheMutex);
        const QString prefix = id + QLatin1Char(':');
        const auto keys = m_renderCache.keys();
        for (const QString &k : keys) {
            if (k.startsWith(prefix))
                m_renderCache.remove(k);
        }
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
    // Read icon bytes under a shared (read) lock — concurrent requestPixmap calls
    // don't block each other here. setIconData uses a write lock when mutating.
    IconEntry entry;
    {
        QReadLocker iconsLocker(&m_iconsLock);
        const auto it = m_icons.constFind(id);
        if (it == m_icons.constEnd() || it->data.isEmpty()) {
            if (size) *size = QSize(0, 0);
            return {};
        }
        entry = *it;
    }

    // Check render cache before doing any rendering work.
    // QMutex needed: QCache::object() mutates LRU even on read.
    auto checkCache = [&](const QSize &actual) -> QPixmap {
        QMutexLocker cacheLocker(&m_cacheMutex);
        const QString key = cacheKey(id, actual);
        if (QPixmap *p = m_renderCache.object(key)) {
            if (size) *size = p->size();
            return *p;
        }
        return {};
    };

    auto insertCache = [&](const QSize &actual, const QPixmap &px) {
        QMutexLocker cacheLocker(&m_cacheMutex);
        const int cost = std::max(1, px.width() * px.height() * px.depth() / 8);
        m_renderCache.insert(cacheKey(id, actual), new QPixmap(px), cost);
    };

    QPixmap pixmap;
    if (entry.ext.compare(QStringLiteral(".svg"), Qt::CaseInsensitive) == 0 ||
        entry.ext.compare(QStringLiteral(".svgz"), Qt::CaseInsensitive) == 0) {

        QSvgRenderer renderer(entry.data);
        if (!renderer.isValid()) {
            if (size) *size = QSize(0, 0);
            return {};
        }

        const QSize target = (requestedSize.isValid() && !requestedSize.isEmpty())
                           ? requestedSize
                           : renderer.defaultSize();

        if (QPixmap cached = checkCache(target); !cached.isNull())
            return cached;

        pixmap = QPixmap(target);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        renderer.render(&painter);

        if (size) *size = target;
        insertCache(target, pixmap);
        return pixmap;
    }

    pixmap.loadFromData(entry.data);
    if (pixmap.isNull()) {
        if (size) *size = QSize(0, 0);
        return pixmap;
    }

    const QSize target = (requestedSize.isValid() && !requestedSize.isEmpty())
                       ? requestedSize
                       : pixmap.size();

    if (size) *size = pixmap.size();

    if (pixmap.size() == target) {
        insertCache(target, pixmap);
        return pixmap;
    }

    if (QPixmap cached = checkCache(target); !cached.isNull())
        return cached;

    QPixmap scaled = pixmap.scaled(target, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    insertCache(target, scaled);
    return scaled;
}
