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
    case FilePathRole:   return item.path;
    case FileSizeRole:   return item.size;
    case IsCheckedRole:  return item.checked;
    case ConfidenceRole: return item.confidence == AppImageManager::CorpseConfidence::High
                                ? QStringLiteral("high") : QStringLiteral("low");
    default:             return {};
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
        {FilePathRole,   "filePath"},
        {FileSizeRole,   "fileSize"},
        {IsCheckedRole,  "isChecked"},
        {ConfidenceRole, "confidence"},
    };
}

Qt::ItemFlags CorpseModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable;
}

void CorpseModel::setCorpses(const QList<AppImageManager::CorpseEntry> &corpses)
{
    beginResetModel();
    m_items.clear();
    m_items.reserve(corpses.size());
    for (const auto &entry : corpses) {
        m_items.push_back({
            entry.path,
            entry.size,
            entry.confidence == AppImageManager::CorpseConfidence::High, // pre-check High
            entry.confidence,
        });
    }
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

int CorpseModel::checkedCount() const
{
    int n = 0;
    for (const Item &item : m_items)
        if (item.checked) ++n;
    return n;
}

QStringList CorpseModel::checkedPaths() const
{
    QStringList result;
    for (const Item &item : m_items)
        if (item.checked)
            result << item.path;
    return result;
}

void CorpseModel::setAllChecked(bool checked)
{
    for (Item &item : m_items)
        item.checked = checked;
    if (!m_items.isEmpty())
        Q_EMIT dataChanged(index(0), index(m_items.size() - 1), {IsCheckedRole});
}

void CorpseModel::setHighConfidenceChecked(bool checked)
{
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items.at(i).confidence == AppImageManager::CorpseConfidence::High) {
            m_items[i].checked = checked;
            Q_EMIT dataChanged(index(i), index(i), {IsCheckedRole});
        }
    }
}

int CorpseModel::highConfidenceCount() const
{
    int n = 0;
    for (const Item &item : m_items)
        if (item.confidence == AppImageManager::CorpseConfidence::High) ++n;
    return n;
}

int CorpseModel::lowConfidenceCount() const
{
    int n = 0;
    for (const Item &item : m_items)
        if (item.confidence == AppImageManager::CorpseConfidence::Low) ++n;
    return n;
}
