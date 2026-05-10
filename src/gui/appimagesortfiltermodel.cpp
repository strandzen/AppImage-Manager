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

void AppImageSortFilterModel::setSortRole(int role)
{
    if (m_sortRole == role)
        return;
    m_sortRole = role;
    Q_EMIT sortRoleChanged();
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
    switch (m_sortRole) {
    case SortBySize: {
        const qint64 a = left.data(AppImageListModel::AppSizeRole).toLongLong();
        const qint64 b = right.data(AppImageListModel::AppSizeRole).toLongLong();
        if (a != b) return a < b;
        break;
    }
    case SortByCategory: {
        const QString a = left.data(AppImageListModel::CategoriesRole).toString();
        const QString b = right.data(AppImageListModel::CategoriesRole).toString();
        const int cmp = a.compare(b, Qt::CaseInsensitive);
        if (cmp != 0) return cmp < 0;
        break;
    }
    case SortByDate: {
        const QDateTime a = left.data(AppImageListModel::AddedDateRole).toDateTime();
        const QDateTime b = right.data(AppImageListModel::AddedDateRole).toDateTime();
        if (a != b) return a > b; // newest first
        break;
    }
    default:
        break;
    }

    const QString aName = left.data(AppImageListModel::DisplayNameRole).toString();
    const QString bName = right.data(AppImageListModel::DisplayNameRole).toString();
    return aName.compare(bName, Qt::CaseInsensitive) < 0;
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
