// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#include "appimageupdate.h"
#include "../core/githubreleasechecker.h"
#include "../core/appimageinfo.h"
#include "logging.h"

#include <KLocalizedString>
#include <KNotification>

#include <QCryptographicHash>
#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QRegularExpression>

static constexpr int kCheckTimeoutMs = 30'000;

AppImageUpdateManager::AppImageUpdateManager(QNetworkAccessManager *nam, QObject *parent)
    : QObject(parent)
    , m_nam(nam)
{
    m_timeoutTimer.setSingleShot(true);
    m_timeoutTimer.setInterval(kCheckTimeoutMs);
    connect(&m_timeoutTimer, &QTimer::timeout, this, [this]() {
        if (m_pendingChecks == 0) return;
        m_pendingChecks = 0;
        Q_EMIT checkingChanged();
        Q_EMIT checkFinished(-1, 0); // -1 = timed out
    });
}

void AppImageUpdateManager::checkForUpdates(const QList<CheckItem> &items)
{
    if (m_pendingChecks > 0)
        return; // batch already in progress

    int checkable = 0;
    for (const CheckItem &item : items) {
        if (item.updateInfo.startsWith(QLatin1String("gh-releases-zsync|")) ||
            item.updateInfo.startsWith(QLatin1String("zsync|")))
            ++checkable;
    }

    if (checkable == 0) {
        Q_EMIT checkFinished(0, 0);
        return;
    }

    m_pendingChecks   = checkable;
    m_updatesFound    = 0;
    m_networkFailures = 0;
    Q_EMIT checkingChanged();
    m_timeoutTimer.start();

    for (const CheckItem &item : items) {
        if (item.updateInfo.startsWith(QLatin1String("gh-releases-zsync|"))) {
            const QString filePath = item.filePath;
            auto *checker = new GitHubReleaseChecker(m_nam, this);
            connect(checker, &GitHubReleaseChecker::updateAvailable, this,
                    [this, filePath, checker](const QString &ver, const QString &url) {
                checker->deleteLater();
                Q_EMIT updateFound(filePath, ver, url);
                finishOneCheck(true);
            });
            connect(checker, &GitHubReleaseChecker::upToDate, this, [this, checker]() {
                checker->deleteLater();
                finishOneCheck(false);
            });
            connect(checker, &GitHubReleaseChecker::networkFailed, this, [this, checker]() {
                checker->deleteLater();
                finishOneCheck(false, true);
            });
            connect(checker, &GitHubReleaseChecker::failed, this, [this, checker]() {
                checker->deleteLater();
                finishOneCheck(false);
            });
            checker->check(item.updateInfo, item.currentVersion);

        } else if (item.updateInfo.startsWith(QLatin1String("zsync|"))) {
            checkZsyncItem(item);
        }
    }
}

void AppImageUpdateManager::checkZsyncItem(const CheckItem &item)
{
    const QString zsyncUrl = item.updateInfo.mid(6); // strip "zsync|"
    const QString filePath = item.filePath;

    QNetworkRequest request((QUrl(zsyncUrl)));
    request.setRawHeader("User-Agent", "AppImageManager-UpdateCheck/1.0");
    request.setRawHeader("Range", "bytes=0-2047");

    QNetworkReply *reply = m_nam->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, zsyncUrl, filePath]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            finishOneCheck(false, true);
            return;
        }

        QString remoteSha1;
        const QByteArray data = reply->readAll();
        for (const QByteArray &line : data.split('\n')) {
            const QByteArray trimmed = line.trimmed();
            if (trimmed.isEmpty()) break;
            if (trimmed.startsWith("SHA-1: "))
                remoteSha1 = QString::fromLatin1(trimmed.mid(7)).trimmed();
        }

        if (!remoteSha1.isEmpty()) {
            QFile f(filePath);
            QString localSha1;
            if (f.open(QIODevice::ReadOnly)) {
                QCryptographicHash hash(QCryptographicHash::Sha1);
                hash.addData(&f);
                localSha1 = QString::fromLatin1(hash.result().toHex());
            }
            if (localSha1 == remoteSha1) { finishOneCheck(false); return; }
        }

        const QString ver = remoteSha1.isEmpty()
            ? i18n("New version available") : remoteSha1.left(8);
        Q_EMIT updateFound(filePath, ver, zsyncUrl);
        finishOneCheck(true);
    });
}

void AppImageUpdateManager::finishOneCheck(bool foundUpdate, bool networkFailed)
{
    if (foundUpdate)    ++m_updatesFound;
    if (networkFailed)  ++m_networkFailures;
    if (--m_pendingChecks == 0) {
        m_timeoutTimer.stop();
        Q_EMIT checkingChanged();
        Q_EMIT checkFinished(m_updatesFound, m_networkFailures);
    }
}

void AppImageUpdateManager::downloadUpdate(const QString &filePath,
                                           const QString &zsyncUrl,
                                           const QString &displayName,
                                           const QString &appIconId)
{
    const QString newFile = filePath + QStringLiteral(".new");

    auto *process = new QProcess(this);
    process->setProgram(QStringLiteral("zsync2"));
    process->setArguments({ QStringLiteral("-i"), filePath,
                            QStringLiteral("-o"), newFile,
                            zsyncUrl });

    connect(process, &QProcess::readyReadStandardOutput, this, [this, process, filePath]() {
        const QString output = QString::fromUtf8(process->readAllStandardOutput());
        static const QRegularExpression re(QStringLiteral(R"((\d+\.\d+)%)"));
        const auto match = re.match(output);
        if (match.hasMatch())
            Q_EMIT downloadProgress(filePath, static_cast<int>(match.captured(1).toDouble()));
    });

    connect(process, &QProcess::finished, this,
            [this, process, filePath, newFile, displayName, appIconId]
            (int exitCode, QProcess::ExitStatus) {
        process->deleteLater();

        if (exitCode == 0 && QFile::exists(newFile)) {
            if (!QFile::remove(filePath) || !QFile::rename(newFile, filePath)) {
                QFile::remove(newFile);
                qCWarning(AIM_LOG) << "Failed to swap updated AppImage:" << filePath;
                Q_EMIT downloadFinished(filePath, false);
                return;
            }
            QFile file(filePath);
            file.setPermissions(file.permissions()
                | QFileDevice::ExeUser | QFileDevice::ExeGroup | QFileDevice::ExeOther);

            auto *n = new KNotification(QStringLiteral("updateDownloaded"),
                                        KNotification::CloseOnTimeout, this);
            n->setTitle(i18n("Update Completed"));
            n->setText(i18n("%1 has been updated successfully.", displayName));
            n->setIconName(appIconId.isEmpty() ? QStringLiteral("application-x-executable") : appIconId);
            n->sendEvent();

            Q_EMIT downloadFinished(filePath, true);
        } else {
            if (QFile::exists(newFile))
                QFile::remove(newFile);
            Q_EMIT downloadFinished(filePath, false);
        }
    });

    process->start();
}
