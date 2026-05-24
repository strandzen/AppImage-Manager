// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#pragma once

#include "../core/appimagemanager.h"

#include <QAbstractListModel>
#include <QList>

// QML_ELEMENT is intentionally absent: Qt 6.11's constexpr metaobject generation
// calls QMetaType::fromType<CorpseModel>() (value type) for QObject subclasses,
// triggering a static_assert. The type is exposed to QML only through the
// AppImageBackend::corpseModel property; the checkedRole property below
// replaces enum access that would otherwise require QML type registration.
class CorpseModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int checkedRole READ checkedRole CONSTANT)

public:
    enum Roles {
        FilePathRole = Qt::UserRole + 1,
        FileSizeRole,
        IsCheckedRole,
        ConfidenceRole,
    };
    Q_ENUM(Roles)

    int checkedRole() const { return static_cast<int>(IsCheckedRole); }

    explicit CorpseModel(QObject *parent = nullptr);

    // QAbstractListModel interface
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role) override;
    QHash<int, QByteArray> roleNames() const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

    // Called from AppImageBackend after the corpse scan finishes
    void setCorpses(const QList<AppImageManager::CorpseEntry> &corpses);
    void clear();

    // Convenience: total size / count of all checked items
    Q_INVOKABLE qint64 checkedSize() const;
    Q_INVOKABLE int checkedCount() const;

    // Counts by confidence level — used by QML to decide section rendering.
    Q_INVOKABLE int highConfidenceCount() const;
    Q_INVOKABLE int lowConfidenceCount() const;

    // Returns the file paths of all checked items
    Q_INVOKABLE QStringList checkedPaths() const;

    Q_INVOKABLE void setAllChecked(bool checked);
    // Check/uncheck only High-confidence items; used by "Select All" in UI.
    Q_INVOKABLE void setHighConfidenceChecked(bool checked);

private:
    struct Item {
        QString path;
        qint64 size = 0;
        bool checked = false;
        AppImageManager::CorpseConfidence confidence = AppImageManager::CorpseConfidence::Low;
    };
    QList<Item> m_items;
};
