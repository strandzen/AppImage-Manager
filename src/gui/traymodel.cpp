// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#include "traymodel.h"

#include <KLocalizedString>

#include <algorithm>

TrayModel::TrayModel(QObject *parent)
    : QObject(parent)
{
}

int TrayModel::row(const QString &path) const
{
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items.at(i).path == path)
            return i;
    }
    return -1;
}

const TrayModel::Item *TrayModel::item(const QString &path) const
{
    const int r = row(path);
    return r < 0 ? nullptr : &m_items.at(r);
}

void TrayModel::upsert(const Item &item)
{
    const int existing = row(item.path);
    if (existing >= 0) {
        m_items[existing] = item;
        resort(); // a new name or state can move it
    } else {
        const auto pos = std::lower_bound(m_items.cbegin(), m_items.cend(), item, &TrayModel::lessThan);
        m_items.insert(pos - m_items.cbegin(), item);
    }
    Q_EMIT changed();
    Q_EMIT summaryChanged();
}

void TrayModel::remove(const QString &path)
{
    const int r = row(path);
    if (r < 0)
        return;
    m_items.removeAt(r);
    Q_EMIT changed();
    Q_EMIT summaryChanged();
}

void TrayModel::setState(const QString &path, State state, const QString &newVersion, const QString &url,
                         const QString &reason)
{
    const int r = row(path);
    if (r < 0)
        return;
    Item &item = m_items[r];
    item.state = state;
    item.newVersion = newVersion;
    item.url = url;
    item.reason = state == Failed ? reason : QString();
    if (state != Updating)
        item.progress = 0;
    resort();
    Q_EMIT changed();
    Q_EMIT summaryChanged();
}

// Updates (and running updates) first, apps without an update source last, by name within.
int TrayModel::group(const Item &item)
{
    if (item.state == UpdateAvailable || item.state == Updating)
        return 0;
    return item.state == NoSource || item.state == Pinned ? 2 : 1;
}

bool TrayModel::lessThan(const Item &a, const Item &b)
{
    const int ga = group(a);
    const int gb = group(b);
    if (ga != gb)
        return ga < gb;
    return a.name.localeAwareCompare(b.name) < 0;
}

void TrayModel::resort()
{
    std::stable_sort(m_items.begin(), m_items.end(), &TrayModel::lessThan);
}

void TrayModel::setProgress(const QString &path, int progress)
{
    const int r = row(path);
    if (r < 0 || m_items.at(r).progress == progress)
        return;
    m_items[r].progress = progress;
    Q_EMIT changed();
}

int TrayModel::updateCount() const
{
    return static_cast<int>(std::count_if(m_items.cbegin(), m_items.cend(),
                                          [](const Item &i) { return i.state == UpdateAvailable; }));
}

int TrayModel::updatingCount() const
{
    return static_cast<int>(std::count_if(m_items.cbegin(), m_items.cend(),
                                          [](const Item &i) { return i.state == Updating; }));
}

void TrayModel::setChecking(bool checking)
{
    if (m_checking == checking)
        return;
    m_checking = checking;
    Q_EMIT summaryChanged();
}

void TrayModel::setSchedule(const QDateTime &lastCheck, const QDateTime &nextCheck)
{
    m_lastCheck = lastCheck;
    m_nextCheck = nextCheck;
    Q_EMIT summaryChanged();
}

QString TrayModel::lastCheckText() const
{
    return m_lastCheck.isValid() ? i18n("Checked %1", formatCheckTime(m_lastCheck))
                                 : i18n("Not checked yet");
}

QString TrayModel::scheduleText() const
{
    QStringList parts{ lastCheckText() };
    if (m_nextCheck.isValid())
        parts << i18n("Next check %1", formatCheckTime(m_nextCheck));
    return parts.join(QStringLiteral(" · "));
}
