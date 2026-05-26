// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#pragma once

#include <QObject>
#include <QString>

class QNetworkAccessManager;

// One-shot GitHub release checker. Construct, connect signals, call check(), then deleteLater().
// Parses `gh-releases-zsync|owner|repo|...` update info, hits the GitHub Releases API, and emits
// exactly one of four signals:
//   updateAvailable — remote tag_name is strictly newer than currentVersion
//   upToDate        — remote version is equal to or older than currentVersion
//   networkFailed   — QNetworkReply reported an error (no connectivity, timeout, HTTP error)
//   failed          — successful reply but unusable: bad JSON, missing tag_name, or malformed updateInfo
class GitHubReleaseChecker : public QObject
{
    Q_OBJECT
public:
    explicit GitHubReleaseChecker(QNetworkAccessManager *nam, QObject *parent = nullptr);

    void check(const QString &updateInfo, const QString &currentVersion);

Q_SIGNALS:
    void updateAvailable(const QString &newVersion, const QString &zsyncUrl);
    void upToDate();
    void networkFailed();
    void failed();

private:
    void checkAtomFeed();

    QNetworkAccessManager *m_nam;
    QString m_updateInfo;
    QString m_currentVersion;
};
