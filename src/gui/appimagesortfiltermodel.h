// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#pragma once

#include "appimagemanager_qml_export.h"

#include <QSortFilterProxyModel>
#include <QtQml/qqmlregistration.h>

class APPIMAGEMANAGER_EXPORT AppImageSortFilterModel : public QSortFilterProxyModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Use the 'proxyModel' context property")

    Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY filterTextChanged)
    Q_PROPERTY(int sortField READ sortField WRITE setSortField NOTIFY sortFieldChanged)
    // Shows only rows in this UpdateState (checkresults.h); -1 shows all. UpdateAvailable also
    // shows rows being updated. Set from the overview.
    Q_PROPERTY(int statusFilter READ statusFilter WRITE setStatusFilter NOTIFY statusFilterChanged)

public:
    enum SortField { SortByName = 0, SortBySize = 1, SortByCategory = 5, SortByDate = 9, SortByStatus = 10 };
    Q_ENUM(SortField)

    explicit AppImageSortFilterModel(QObject *parent = nullptr);

    QString filterText() const { return m_filterText; }
    int sortField() const { return m_sortField; }
    int statusFilter() const { return m_statusFilter; }
    void setStatusFilter(int state);

    void setFilterText(const QString &text);
    void setSortField(int field);

    Q_INVOKABLE void toggleDesktopLink(int proxyRow, bool enable);
    Q_INVOKABLE void requestRemoveAt(int proxyRow);
    Q_INVOKABLE void requestLaunch(int proxyRow);
    Q_INVOKABLE void downloadUpdate(int proxyRow);
    Q_INVOKABLE QVariantMap itemData(int proxyRow) const;
    // Proxy row of the AppImage at `path`, or -1 (not loaded yet, or filtered out).
    Q_INVOKABLE int rowForPath(const QString &path) const;
    Q_INVOKABLE QVariantMap updateSource(int proxyRow) const;
    Q_INVOKABLE bool setUpdateSource(int proxyRow, const QString &input, const QString &type, bool prerelease);
    Q_INVOKABLE QString environment(int proxyRow) const;
    Q_INVOKABLE QVariantList setEnvironment(int proxyRow, const QString &text);
    Q_INVOKABLE QVariantMap configFolder(int proxyRow) const;
    Q_INVOKABLE void setConfigFolder(int proxyRow, bool enable, const QStringList &moveDirs);
    Q_INVOKABLE bool restorePrevious(int proxyRow);
    Q_INVOKABLE void checkSignature(int proxyRow);
    Q_INVOKABLE void fetchReleaseNotes(int proxyRow);
    Q_INVOKABLE QVariantMap sandbox(int proxyRow) const;
    Q_INVOKABLE void setSandbox(int proxyRow, bool enabled, const QStringList &folders, bool network = true);
    Q_INVOKABLE void setPinned(int proxyRow, bool pinned);

Q_SIGNALS:
    void filterTextChanged();
    void sortFieldChanged();
    void statusFilterChanged();

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;

private:
    int sourceRowFor(int proxyRow) const;

    QString m_filterText;
    int m_sortField = SortByName;
    int m_statusFilter = -1;
};
