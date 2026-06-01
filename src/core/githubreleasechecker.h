// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#pragma once

#include <QObject>
#include <QString>

class QNetworkAccessManager;

// One-shot GitHub release checker. Construct, connect checkFinished, call check(), then deleteLater().
// Parses `gh-releases-zsync|owner|repo|...` update info, hits the GitHub Releases API, and emits
// checkFinished exactly once with one of four Result values:
//   UpdateAvailable — remote tag_name is strictly newer than currentVersion
//   UpToDate        — remote version is equal to or older than currentVersion
//   NetworkFailed   — QNetworkReply reported an error (no connectivity, timeout, HTTP error)
//   Failed          — successful reply but unusable: bad JSON, missing tag_name, or malformed updateInfo
class GitHubReleaseChecker : public QObject
{
    Q_OBJECT
public:
    enum class Result { UpdateAvailable, UpToDate, NetworkFailed, Failed };
    Q_ENUM(Result)

    explicit GitHubReleaseChecker(QNetworkAccessManager *nam, QObject *parent = nullptr);

    void check(const QString &updateInfo, const QString &currentVersion);

Q_SIGNALS:
    void checkFinished(GitHubReleaseChecker::Result result,
                       const QString &newVersion, const QString &zsyncUrl);

private:
    void checkAtomFeed();

    QNetworkAccessManager *m_nam;
    QString m_updateInfo;
    QString m_currentVersion;
};
