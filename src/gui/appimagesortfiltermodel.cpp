// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#include "appimagesortfiltermodel.h"
#include "appimagelistmodel.h"

#include <QDateTime>

AppImageSortFilterModel::AppImageSortFilterModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    setDynamicSortFilter(true);
    sort(0); // enable proxy sorting
}

void AppImageSortFilterModel::setFilterText(const QString &text)
{
    if (m_filterText == text)
        return;
    m_filterText = text;
    Q_EMIT filterTextChanged();
    beginFilterChange();
    endFilterChange();
}

void AppImageSortFilterModel::setSortRole(int role)
{
    if (m_sortRole == role)
        return;
    m_sortRole = role;
    Q_EMIT sortRoleChanged();
    invalidate();
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
    case SortBySize:
        return left.data(AppImageListModel::AppSizeRole).toLongLong() < right.data(AppImageListModel::AppSizeRole).toLongLong();
    case SortByDate:
        return left.data(AppImageListModel::AddedDateRole).toDateTime() > right.data(AppImageListModel::AddedDateRole).toDateTime();
    default: { // SortByName
        const QString a = left.data(AppImageListModel::CleanNameRole).toString();
        const QString b = right.data(AppImageListModel::CleanNameRole).toString();
        return a.compare(b, Qt::CaseInsensitive) < 0;
    }
    }
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
