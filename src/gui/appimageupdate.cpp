// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#include "appimageupdate.h"
#include "../core/releasechecker.h"
#include "../core/appimageinfo.h"
#include "../core/updatesource.h"
#include "logging.h"

#include <KLocalizedString>
#include <KNotification>

#include <QCryptographicHash>
#include <QFile>
#include <QUrl>
#include <memory>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrent>

// Watchdog per checked AppImage (checks run one at a time; each request also has its own
// 10 s transfer timeout, so this only fires when something hangs, e.g. hashing a huge file).
static constexpr int kCheckTimeoutMs = 30'000;

AppImageUpdateManager::AppImageUpdateManager(QNetworkAccessManager *nam, QObject *parent)
    : QObject(parent)
    , m_nam(nam)
{
    m_timeoutTimer.setSingleShot(true);
    m_timeoutTimer.setInterval(kCheckTimeoutMs);
    connect(&m_timeoutTimer, &QTimer::timeout, this, [this]() {
        if (m_pendingChecks == 0) return;
        ++m_batch;              // late results of this batch are ignored
        m_checkQueue.clear();
        m_pendingChecks = 0;
        Q_EMIT checkingChanged();
        Q_EMIT checkFinished(-1, 0); // -1 = timed out
    });
}

void AppImageUpdateManager::checkForUpdates(const QList<CheckItem> &items)
{
    if (m_pendingChecks > 0)
        return; // batch already in progress

    m_checkQueue.clear();
    for (CheckItem item : items) {
        item.updateInfo = effectiveUpdateInfo(item.updateInfo, loadUpdateSource(item.filePath));
        if (isSupportedUpdateInfo(item.updateInfo))
            m_checkQueue.enqueue(item);
    }

    if (m_checkQueue.isEmpty()) {
        Q_EMIT checkFinished(0, 0);
        return;
    }

    ++m_batch;
    m_pendingChecks   = m_checkQueue.size();
    m_updatesFound    = 0;
    m_networkFailures = 0;
    Q_EMIT checkingChanged();

    checkNextItem();
}

void AppImageUpdateManager::checkNextItem()
{
    if (m_checkQueue.isEmpty())
        return;

    const CheckItem item = m_checkQueue.dequeue();
    const quint64 batch = m_batch;
    m_timeoutTimer.start();

    if (isReleaseUpdateInfo(item.updateInfo)) {
        const QString filePath = item.filePath;
        auto *checker = new ReleaseChecker(m_nam, this);
        connect(checker, &ReleaseChecker::checkFinished, this,
                [this, filePath, checker, batch](ReleaseChecker::Result result,
                                                 const QString &ver, const QString &url) {
            checker->deleteLater();
            if (batch != m_batch)
                return;
            const bool networkFailed = (result == ReleaseChecker::Result::NetworkFailed);
            if (result == ReleaseChecker::Result::UpdateAvailable)
                Q_EMIT updateFound(filePath, ver, url);
            finishOneCheck(result == ReleaseChecker::Result::UpdateAvailable,
                           networkFailed);
        });
        checker->check(filePath, item.updateInfo, item.currentVersion);

    } else if (item.updateInfo.startsWith(QLatin1String("zsync|"))) {
        checkZsyncItem(item, batch);
    }
}

void AppImageUpdateManager::checkZsyncItem(const CheckItem &item, quint64 batch)
{
    const QString zsyncUrl = item.updateInfo.mid(6); // strip "zsync|"
    const QString filePath = item.filePath;

    QNetworkRequest request((QUrl(zsyncUrl)));
    request.setRawHeader("User-Agent", "AppImageManager-UpdateCheck/1.0");
    request.setRawHeader("Range", "bytes=0-8191");

    QNetworkReply *reply = m_nam->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, zsyncUrl, filePath, batch]() {
        reply->deleteLater();
        if (batch != m_batch)
            return;

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

        if (remoteSha1.isEmpty()) {
            const QString ver = i18n("New version available");
            Q_EMIT updateFound(filePath, ver, zsyncUrl);
            finishOneCheck(true);
            return;
        }

        // Calculate local file hash asynchronously off the main GUI thread
        auto *watcher = new QFutureWatcher<QString>(this);
        connect(watcher, &QFutureWatcher<QString>::finished, this, [this, watcher, filePath, zsyncUrl, remoteSha1, batch]() {
            watcher->deleteLater();
            if (batch != m_batch)
                return;
            const QString localSha1 = watcher->result();
            if (localSha1 == remoteSha1) {
                finishOneCheck(false);
            } else {
                const QString ver = remoteSha1.left(8);
                Q_EMIT updateFound(filePath, ver, zsyncUrl);
                finishOneCheck(true);
            }
        });

        watcher->setFuture(QtConcurrent::run(&AppImageUpdateManager::calculateFileSha1, filePath));
    });
}

QString AppImageUpdateManager::calculateFileSha1(const QString &filePath)
{
    QFile f(filePath);
    if (f.open(QIODevice::ReadOnly)) {
        QCryptographicHash hash(QCryptographicHash::Sha1);
        char buffer[4096];
        while (!f.atEnd()) {
            const qint64 read = f.read(buffer, sizeof(buffer));
            if (read > 0)
                hash.addData(QByteArrayView(buffer, read));
        }
        return QString::fromLatin1(hash.result().toHex());
    }
    qCWarning(AIM_LOG) << "calculateFileSha1: cannot open" << filePath << "-" << f.errorString();
    return QString();
}

void AppImageUpdateManager::finishOneCheck(bool foundUpdate, bool networkFailed)
{
    if (foundUpdate)    ++m_updatesFound;
    if (networkFailed)  ++m_networkFailures;
    if (--m_pendingChecks == 0) {
        m_timeoutTimer.stop();
        Q_EMIT checkingChanged();
        Q_EMIT checkFinished(m_updatesFound, m_networkFailures);
    } else {
        checkNextItem();
    }
}

void AppImageUpdateManager::downloadUpdate(const QString &filePath,
                                           const QString &url,
                                           const QString &displayName,
                                           const QString &appIconId)
{
    if (!QUrl(url).path().endsWith(QLatin1String(".zsync"), Qt::CaseInsensitive)) {
        // Direct .AppImage download (release asset without a .zsync file, or a direct URL source).
        downloadBinary(filePath, QUrl(url), {}, displayName, appIconId);
        return;
    }
    const QString &zsyncUrl = url;
    const QString zsync2Path = QStandardPaths::findExecutable(QStringLiteral("zsync2"));
    if (zsync2Path.isEmpty()) {
        qCDebug(AIM_LOG) << "zsync2 not found. Falling back to full HTTP download via zsync URL:" << zsyncUrl;
        startFullHttpDownload(filePath, zsyncUrl, displayName, appIconId);
        return;
    }

    const QString newFile = filePath + QStringLiteral(".new");

    auto *process = new QProcess(this);
    process->setProgram(zsync2Path);
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
            swapAndFinalizeDownload(filePath, newFile, displayName, appIconId);
        } else {
            if (QFile::exists(newFile))
                QFile::remove(newFile);
            Q_EMIT downloadFinished(filePath, false);
        }
    });

    process->start();
    if (process->state() == QProcess::NotRunning) {
        qCWarning(AIM_LOG) << "Failed to start zsync2 process";
        process->deleteLater();
        Q_EMIT downloadFinished(filePath, false);
        return;
    }
}

void AppImageUpdateManager::startFullHttpDownload(const QString &filePath,
                                                  const QString &zsyncUrl,
                                                  const QString &displayName,
                                                  const QString &appIconId)
{
    // Step 1: Download the .zsync control metadata file to extract the target URL
    QNetworkRequest request((QUrl(zsyncUrl)));
    request.setRawHeader("User-Agent", "AppImageManager-UpdateCheck/1.0");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply *reply = m_nam->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, filePath, zsyncUrl, displayName, appIconId]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            qCWarning(AIM_LOG) << "Fallback: Failed to fetch .zsync control file:" << reply->errorString();
            Q_EMIT downloadFinished(filePath, false);
            return;
        }

        const QByteArray data = reply->readAll();
        QString binaryUrlStr;
        QString remoteSha1;
        for (const QByteArray &line : data.split('\n')) {
            const QByteArray trimmed = line.trimmed();
            if (trimmed.isEmpty()) break; // headers end on first empty line
            if (trimmed.startsWith("URL:")) {
                binaryUrlStr = QString::fromUtf8(trimmed.mid(4)).trimmed();
                if (binaryUrlStr.startsWith(QLatin1Char(' ')))
                    binaryUrlStr = binaryUrlStr.mid(1).trimmed();
            } else if (trimmed.startsWith("SHA-1:")) {
                remoteSha1 = QString::fromLatin1(trimmed.mid(6)).trimmed();
            }
        }

        if (binaryUrlStr.isEmpty()) {
            qCWarning(AIM_LOG) << "Fallback: Could not find URL header in .zsync control file";
            Q_EMIT downloadFinished(filePath, false);
            return;
        }

        // Resolve absolute URL
        const QUrl binaryUrl = QUrl(zsyncUrl).resolved(QUrl(binaryUrlStr));
        qCDebug(AIM_LOG) << "Fallback: resolved binary URL to" << binaryUrl.toString();

        downloadBinary(filePath, binaryUrl, remoteSha1, displayName, appIconId);
    });
}

void AppImageUpdateManager::downloadBinary(const QString &filePath,
                                           const QUrl &binaryUrl,
                                           const QString &remoteSha1,
                                           const QString &displayName,
                                           const QString &appIconId)
{
    // Stream the AppImage to <file>.new, verify SHA-1 when known, then swap.
    const QString newFile = filePath + QStringLiteral(".new");
    auto *file = new QFile(newFile, this);
    if (!file->open(QIODevice::WriteOnly)) {
        qCWarning(AIM_LOG) << "Fallback: Failed to open temp file for writing:" << newFile;
        file->deleteLater();
        Q_EMIT downloadFinished(filePath, false);
        return;
    }

    QNetworkRequest binRequest(binaryUrl);
    binRequest.setRawHeader("User-Agent", "AppImageManager-UpdateCheck/1.0");
    binRequest.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply *binReply = m_nam->get(binRequest);
    // Set when the server answers with a web page (login, error or landing page) instead of a
    // file, so it never replaces a working AppImage.
    auto notAnAppImage = std::make_shared<bool>(false);

    // Write chunks sequentially as they arrive to keep RAM footprint low
    connect(binReply, &QNetworkReply::readyRead, this, [binReply, file, notAnAppImage]() {
        if (file->pos() == 0 && ReleaseChecker::isNonBinaryContentType(
                binReply->header(QNetworkRequest::ContentTypeHeader).toString())) {
            *notAnAppImage = true;
            binReply->abort();
            return;
        }
        if (file->write(binReply->readAll()) < 0)
            binReply->abort(); // disk full or I/O error — abort early; finished() checks file->error()
    });

    connect(binReply, &QNetworkReply::downloadProgress, this, [this, filePath](qint64 bytesReceived, qint64 bytesTotal) {
        if (bytesTotal > 0) {
            int percent = static_cast<int>((bytesReceived * 100) / bytesTotal);
            Q_EMIT downloadProgress(filePath, percent);
        }
    });

    connect(binReply, &QNetworkReply::finished, this, [this, binReply, file, filePath, newFile, displayName, appIconId, remoteSha1, notAnAppImage]() {
        binReply->deleteLater();
        file->close();
        const bool fileWriteOk = (file->error() == QFile::NoError);
        file->deleteLater();

        if (*notAnAppImage) {
            qCWarning(AIM_LOG) << "Download for" << filePath << "returned"
                               << binReply->header(QNetworkRequest::ContentTypeHeader).toString()
                               << "instead of an AppImage; keeping the installed file.";
            QFile::remove(newFile);
            Q_EMIT downloadFinished(filePath, false);
            return;
        }

        if (binReply->error() != QNetworkReply::NoError || !fileWriteOk) {
            qCWarning(AIM_LOG) << "Fallback: Binary download failed or write error:" << binReply->errorString();
            QFile::remove(newFile);
            Q_EMIT downloadFinished(filePath, false);
            return;
        }

        // Verify SHA-1 hash if available in .zsync
        if (!remoteSha1.isEmpty()) {
            auto *watcher = new QFutureWatcher<QString>(this);
            connect(watcher, &QFutureWatcher<QString>::finished, this, [this, watcher, filePath, newFile, displayName, appIconId, remoteSha1]() {
                watcher->deleteLater();
                const QString localSha1 = watcher->result();
                if (localSha1 != remoteSha1) {
                    qCWarning(AIM_LOG) << "Fallback: Hash mismatch on downloaded file. Expected:" << remoteSha1 << "Got:" << localSha1;
                    QFile::remove(newFile);
                    Q_EMIT downloadFinished(filePath, false);
                    return;
                }
                swapAndFinalizeDownload(filePath, newFile, displayName, appIconId);
            });
            watcher->setFuture(QtConcurrent::run(&AppImageUpdateManager::calculateFileSha1, newFile));
        } else {
            swapAndFinalizeDownload(filePath, newFile, displayName, appIconId);
        }
    });
}

void AppImageUpdateManager::swapAndFinalizeDownload(const QString &filePath,
                                                   const QString &newFile,
                                                   const QString &displayName,
                                                   const QString &appIconId)
{
    // Successfully downloaded — rename old → .bak before swap so it
    // can be recovered if the process is killed between the two renames.
    const QString bakFile = filePath + QStringLiteral(".bak");
    QFile::remove(bakFile); // remove stale bak from a previous crashed update
    if (!QFile::rename(filePath, bakFile) || !QFile::rename(newFile, filePath)) {
        QFile::rename(bakFile, filePath); // restore original if possible
        QFile::remove(newFile);
        qCWarning(AIM_LOG) << "Failed to swap updated AppImage:" << filePath;
        Q_EMIT downloadFinished(filePath, false);
        return;
    }
    QFile::remove(bakFile);

    QFile fileObj(filePath);
    fileObj.setPermissions(fileObj.permissions()
        | QFileDevice::ExeUser | QFileDevice::ExeGroup | QFileDevice::ExeOther);

    auto *n = new KNotification(QStringLiteral("updateDownloaded"),
                                KNotification::CloseOnTimeout, this);
    n->setTitle(i18n("Update Completed"));
    n->setText(i18n("%1 has been updated successfully.", displayName));
    n->setIconName(appIconId.isEmpty() ? QStringLiteral("application-x-executable") : appIconId);
    n->sendEvent();

    Q_EMIT downloadFinished(filePath, true);
}
