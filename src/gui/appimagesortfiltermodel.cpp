// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#include "appimagesortfiltermodel.h"
#include "appimagelistmodel.h"


AppImageSortFilterModel::AppImageSortFilterModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    setDynamicSortFilter(true);
    sort(0, Qt::AscendingOrder);
}

void AppImageSortFilterModel::setFilterText(const QString &text)
{
    if (m_filterText == text)
        return;
    m_filterText = text;
    Q_EMIT filterTextChanged();
    invalidateFilter();
}

void AppImageSortFilterModel::setSortField(int field)
{
    if (m_sortField == field)
        return;
    m_sortField = field;
    Q_EMIT sortFieldChanged();
    invalidate();
}

void AppImageSortFilterModel::setSortOrder(Qt::SortOrder order)
{
    if (m_sortOrder == order)
        return;
    m_sortOrder = order;
    Q_EMIT sortOrderChanged();
    sort(0, m_sortOrder);
}

bool AppImageSortFilterModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    if (m_filterText.isEmpty())
        return true;
    const QAbstractItemModel *src = sourceModel();
    const QModelIndex idx = src->index(sourceRow, 0, sourceParent);
    const QString cleanName = src->data(idx, AppImageListModel::CleanNameRole).toString();
    const QString appName   = src->data(idx, AppImageListModel::AppNameRole).toString();
    return cleanName.contains(m_filterText, Qt::CaseInsensitive)
        || appName.contains(m_filterText, Qt::CaseInsensitive);
}

bool AppImageSortFilterModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    auto *src = qobject_cast<AppImageListModel *>(sourceModel());
    if (!src)
        return false;

    const int lrow = left.row();
    const int rrow = right.row();

    // Skip the role dispatch in data() — pull primary keys directly off the Item.
    if (m_sortField != SortByName) {
        const QVariant a = src->sortKey(lrow, m_sortField);
        const QVariant b = src->sortKey(rrow, m_sortField);
        switch (m_sortField) {
        case SortBySize: {
            const qint64 aa = a.toLongLong();
            const qint64 bb = b.toLongLong();
            if (aa != bb) return aa > bb; // largest first
            break;
        }
        case SortByCategory: {
            const int cmp = a.toString().compare(b.toString(), Qt::CaseInsensitive);
            if (cmp != 0) return cmp < 0;
            break;
        }
        case SortByDate: {
            const QDateTime aa = a.toDateTime();
            const QDateTime bb = b.toDateTime();
            if (aa != bb) return aa > bb; // newest first
            break;
        }
        default:
            break;
        }
    }

    return src->displayNameForRow(lrow)
              .compare(src->displayNameForRow(rrow), Qt::CaseInsensitive) < 0;
}

int AppImageSortFilterModel::sourceRowFor(int proxyRow) const
{
    return mapToSource(index(proxyRow, 0)).row();
}

void AppImageSortFilterModel::toggleDesktopLink(int proxyRow, bool enable)
{
    auto *src = qobject_cast<AppImageListModel *>(sourceModel());
    if (src)
        src->toggleDesktopLink(sourceRowFor(proxyRow), enable);
}

void AppImageSortFilterModel::requestRemoveAt(int proxyRow)
{
    auto *src = qobject_cast<AppImageListModel *>(sourceModel());
    if (src)
        src->requestRemoveAt(sourceRowFor(proxyRow));
}

void AppImageSortFilterModel::requestLaunch(int proxyRow)
{
    auto *src = qobject_cast<AppImageListModel *>(sourceModel());
    if (src)
        src->requestLaunch(sourceRowFor(proxyRow));
}

void AppImageSortFilterModel::downloadUpdate(int proxyRow)
{
    auto *src = qobject_cast<AppImageListModel *>(sourceModel());
    if (src)
        src->downloadUpdate(sourceRowFor(proxyRow));
}

QVariantMap AppImageSortFilterModel::itemData(int proxyRow) const
{
    auto *src = qobject_cast<AppImageListModel *>(sourceModel());
    if (!src) return {};
    return src->itemData(sourceRowFor(proxyRow));
}
