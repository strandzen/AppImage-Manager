// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#pragma once

#include <QByteArray>
#include <QList>
#include <QMetaType>
#include <QString>
#include <QStringList>

struct AppImageInfo {
    QString originalName;
    QString cleanName;
    QString appId;
    QString appName;
    QString version;
    QString categories;
    QString comment;
    QString description;
    QString developerName;
    QString homepage;
    QString execArgs;
    qint64 fileSize = 0;
    QByteArray iconData;
    QString iconExt;
    QString updateInfo;
    bool isValid = false;
};

Q_DECLARE_METATYPE(AppImageInfo)

// ── Version utilities ─────────────────────────────────────────────────────────

inline QString normalizeVersion(const QString &v)
{
    if (!v.isEmpty() && (v.front() == QLatin1Char('v') || v.front() == QLatin1Char('V')))
        return v.mid(1);
    return v;
}

inline bool isNewerVersion(const QString &remote, const QString &local)
{
    if (remote == local)
        return false;
    auto toInts = [](const QString &v) {
        QList<int> parts;
        for (const QString &p : v.split(QLatin1Char('.')))
            parts.append(p.toInt());
        while (parts.size() < 3) parts.append(0);
        return parts;
    };
    const QList<int> r = toInts(remote);
    const QList<int> l = toInts(local);
    for (int i = 0; i < 3; ++i) {
        if (r[i] > l[i]) return true;
        if (r[i] < l[i]) return false;
    }
    return false;
}

// ── File filter ───────────────────────────────────────────────────────────────

inline const QStringList &kAppImageFilters()
{
    static const QStringList filters = { QStringLiteral("*.AppImage"), QStringLiteral("*.appimage") };
    return filters;
}
