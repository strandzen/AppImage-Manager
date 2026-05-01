// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#pragma once

#include <QSortFilterProxyModel>
#include <QtQml/qqmlregistration.h>

class AppImageSortFilterModel : public QSortFilterProxyModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Use the 'proxyModel' context property")

    Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY filterTextChanged)
    Q_PROPERTY(int sortRole READ sortRole WRITE setSortRole NOTIFY sortRoleChanged)
    Q_PROPERTY(Qt::SortOrder sortOrder READ sortOrder WRITE setSortOrder NOTIFY sortOrderChanged)

public:
    enum SortRole { SortByName = 0, SortBySize = 1, SortByDate = 2, SortByVersion = 3, SortByVisible = 4 };
    Q_ENUM(SortRole)

    explicit AppImageSortFilterModel(QObject *parent = nullptr);

    QString filterText() const { return m_filterText; }
    int sortRole() const { return m_sortRole; }
    Qt::SortOrder sortOrder() const { return m_sortOrder; }

    void setFilterText(const QString &text);
    void setSortRole(int role);
    void setSortOrder(Qt::SortOrder order);

    Q_INVOKABLE void toggleDesktopLink(int proxyRow, bool enable);
    Q_INVOKABLE void requestRemoveAt(int proxyRow);
    Q_INVOKABLE void requestLaunch(int proxyRow);

Q_SIGNALS:
    void filterTextChanged();
    void sortRoleChanged();
    void sortOrderChanged();

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;

private:
    int sourceRowFor(int proxyRow) const;

    QString m_filterText;
    int m_sortRole = SortByName;
    Qt::SortOrder m_sortOrder = Qt::AscendingOrder;
};
