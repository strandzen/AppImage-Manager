// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#include "corpsemodel.h"

CorpseModel::CorpseModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int CorpseModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_items.size());
}

QVariant CorpseModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount())
        return {};

    const Item &item = m_items.at(index.row());
    switch (role) {
    case FilePathRole:  return item.path;
    case FileSizeRole:  return item.size;
    case IsCheckedRole: return item.checked;
    default:            return {};
    }
}

bool CorpseModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid() || role != IsCheckedRole)
        return false;

    m_items[index.row()].checked = value.toBool();
    Q_EMIT dataChanged(index, index, {IsCheckedRole});
    return true;
}

QHash<int, QByteArray> CorpseModel::roleNames() const
{
    return {
        {FilePathRole,  "filePath"},
        {FileSizeRole,  "fileSize"},
        {IsCheckedRole, "isChecked"},
    };
}

Qt::ItemFlags CorpseModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable;
}

void CorpseModel::setCorpses(const QList<QPair<QString, qint64>> &corpses)
{
    beginResetModel();
    m_items.clear();
    m_items.reserve(corpses.size());
    for (const auto &[path, size] : corpses)
        m_items.push_back({path, size, false});
    endResetModel();
}

void CorpseModel::clear()
{
    beginResetModel();
    m_items.clear();
    endResetModel();
}

qint64 CorpseModel::checkedSize() const
{
    qint64 total = 0;
    for (const Item &item : m_items)
        if (item.checked)
            total += item.size;
    return total;
}

QStringList CorpseModel::checkedPaths() const
{
    QStringList result;
    for (const Item &item : m_items)
        if (item.checked)
            result << item.path;
    return result;
}
