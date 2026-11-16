// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#pragma once

#include "appimagemanager_qml_export.h"

#include "../core/checkresults.h"

#include <QDateTime>
#include <QList>
#include <QObject>

// Installed AppImages and their update state, as the tray shows them (updates first, apps
// without an update source last). Filled by UpdateDaemon, which sends the list as JSON
// (State()); QML (`trayModel`) only reads the summary properties and the change signals.
class APPIMAGEMANAGER_EXPORT TrayModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int  updateCount   READ updateCount   NOTIFY summaryChanged)
    Q_PROPERTY(int  updatingCount READ updatingCount NOTIFY summaryChanged)
    Q_PROPERTY(bool checking      READ checking      NOTIFY summaryChanged)
    // "Checked 14:02 · Next check tomorrow 20:30" (or the pieces that apply).
    Q_PROPERTY(QString scheduleText READ scheduleText NOTIFY summaryChanged)
    // Only the first part: "Checked 14:02" or "Not checked yet" (the tray popup's header).
    Q_PROPERTY(QString lastCheckText READ lastCheckText NOTIFY summaryChanged)

public:
    // The values of UpdateState (checkresults.h), which is stored and sent to QML.
    enum State {
        NotChecked = int(UpdateState::NotChecked),
        UpToDate = int(UpdateState::UpToDate),
        UpdateAvailable = int(UpdateState::UpdateAvailable),
        NoSource = int(UpdateState::NoSource),
        Failed = int(UpdateState::Failed),
        Updating = int(UpdateState::Updating),
    };

    struct Item {
        QString path;
        QString name;
        QString icon;        // theme name, or a file (the AppImage's own icon, cached for the views)
        QString version;
        QString appIconId;   // for notifications
        QString updateInfo;  // embedded update info (the user's source is applied when checking)
        State   state = NotChecked;
        QString newVersion;
        QString url;
        QString reason;      // why the check failed
        int     progress = 0;
    };

    explicit TrayModel(QObject *parent = nullptr);

    const QList<Item> &items() const { return m_items; }
    int  row(const QString &path) const;
    const Item *item(const QString &path) const;

    // Inserts or replaces (by path), keeping the list sorted by name.
    void upsert(const Item &item);
    void remove(const QString &path);
    void setState(const QString &path, State state, const QString &newVersion = {}, const QString &url = {},
                  const QString &reason = {});
    void setProgress(const QString &path, int progress);

    int  updateCount() const;
    int  updatingCount() const;
    bool checking() const { return m_checking; }
    void setChecking(bool checking);
    void setSchedule(const QDateTime &lastCheck, const QDateTime &nextCheck);
    QString scheduleText() const;
    QString lastCheckText() const;

Q_SIGNALS:
    void changed();         // the list: rows, states, progress
    void summaryChanged();  // counts, checking, schedule

public:
    // Sort order of the list: updates first, apps without an update source last.
    static int group(const Item &item);
    static bool lessThan(const Item &a, const Item &b);

private:
    void resort();

    QList<Item> m_items;
    bool        m_checking = false;
    QDateTime   m_lastCheck;
    QDateTime   m_nextCheck;
};
