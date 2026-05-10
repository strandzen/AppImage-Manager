// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#pragma once

#include <QObject>
#include <QString>

class QNetworkAccessManager;

// One-shot GitHub release checker. Construct, connect signals, call check(), then deleteLater().
// Parses gh-releases-zsync|owner|repo|... update info, hits the GitHub Releases API, and emits
// one of three signals depending on the result.
class GitHubReleaseChecker : public QObject
{
    Q_OBJECT
public:
    explicit GitHubReleaseChecker(QNetworkAccessManager *nam, QObject *parent = nullptr);

    void check(const QString &updateInfo, const QString &currentVersion);

Q_SIGNALS:
    void updateAvailable(const QString &newVersion, const QString &zsyncUrl);
    void upToDate();
    void failed();

private:
    QNetworkAccessManager *m_nam;
};
