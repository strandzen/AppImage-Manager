// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#include "appimageiconstore.h"

AppImageIconStore &AppImageIconStore::instance()
{
    static AppImageIconStore s;
    return s;
}

void AppImageIconStore::set(const QString &id, const QByteArray &data, const QString &ext)
{
    QWriteLocker locker(&m_lock);
    m_icons.insert(id, {data, ext});
}

bool AppImageIconStore::get(const QString &id, QByteArray &data, QString &ext) const
{
    QReadLocker locker(&m_lock);
    const auto it = m_icons.constFind(id);
    if (it == m_icons.constEnd() || it->data.isEmpty())
        return false;
    data = it->data;
    ext  = it->ext;
    return true;
}
