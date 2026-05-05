// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#include "updatedaemon.h"
#include "appsettings.h"
#include "appimagereader.h"
#include <QDir>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QFutureWatcher>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QStandardPaths>
#include <KNotification>
#include <KLocalizedString>
#include <QProcess>
#include <QtConcurrent/QtConcurrent>

static std::chrono::hours intervalForFrequency(int freq, int customDays)
{
    switch (freq) {
    case 1: return std::chrono::hours(24);
    case 2: return std::chrono::hours(24 * 7);
    case 3: return std::chrono::hours(24 * 30);
    case 4: return std::chrono::hours(24 * customDays);
    default: return std::chrono::hours(24);
    }
}

static bool isNewerVersion(const QString &remote, const QString &local)
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

UpdateDaemon::UpdateDaemon(QObject *parent)
    : QObject(parent)
    , m_timer(new QTimer(this))
    , m_networkManager(new QNetworkAccessManager(this))
{
    connect(m_timer, &QTimer::timeout, this, &UpdateDaemon::checkUpdates);
    connect(AppSettings::instance(), &AppSettings::updateFrequencyChanged, this, [this]() {
        const int freq = AppSettings::instance()->updateFrequency();
        m_timer->setInterval(intervalForFrequency(freq, AppSettings::instance()->customUpdateDays()));
        if (m_timer->isActive())
            m_timer->start();
    });
    watchDownloads();
}

void UpdateDaemon::start()
{
    const int freq = AppSettings::instance()->updateFrequency();
    m_timer->setInterval(intervalForFrequency(freq, AppSettings::instance()->customUpdateDays()));
    checkUpdates();
    m_timer->start();
}

void UpdateDaemon::watchDownloads()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (dir.isEmpty())
        return;

    const QStringList filters = { QStringLiteral("*.AppImage"), QStringLiteral("*.appimage") };
    for (const QFileInfo &fi : QDir(dir).entryInfoList(filters, QDir::Files))
        m_knownDownloads.insert(fi.absoluteFilePath());

    m_downloadWatcher = new QFileSystemWatcher(this);
    m_downloadWatcher->addPath(dir);
    connect(m_downloadWatcher, &QFileSystemWatcher::directoryChanged,
            this, &UpdateDaemon::onDownloadDirChanged);
}

void UpdateDaemon::onDownloadDirChanged()
{
    if (!AppSettings::instance()->watchDownloads())
        return;

    const QString dir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    const QStringList filters = { QStringLiteral("*.AppImage"), QStringLiteral("*.appimage") };
    const QFileInfoList entries = QDir(dir).entryInfoList(filters, QDir::Files);

    for (const QFileInfo &fi : entries) {
        const QString path = fi.absoluteFilePath();
        if (m_knownDownloads.contains(path))
            continue;
        m_knownDownloads.insert(path);

        QString displayName = AppImageReader::cleanName(fi.fileName());
        if (displayName.endsWith(QLatin1String(".AppImage"), Qt::CaseInsensitive))
            displayName.chop(9);
        // Strip trailing version number (e.g. "Krita 5.3.1" → "Krita")
        displayName.remove(QRegularExpression(QStringLiteral(R"(\s+\d[\d.]*$)")));
        displayName = displayName.trimmed();

        auto *n = new KNotification(QStringLiteral("downloaded"), KNotification::Persistent, this);
        n->setTitle(i18n("%1 downloaded", displayName));
        n->setIconName(QStringLiteral("application-x-executable"));
        auto *action = n->addAction(i18n("Manage"));
        connect(action, &KNotificationAction::activated, this, [path]() {
            QProcess::startDetached(QStringLiteral("appimagemanager"), { path });
        });
        n->sendEvent();
    }

    QSet<QString> current;
    for (const QFileInfo &fi : entries)
        current.insert(fi.absoluteFilePath());
    m_knownDownloads.intersect(current);
}

void UpdateDaemon::checkUpdates()
{
    if (AppSettings::instance()->updateFrequency() == 0)
        return;

    const QString dir = AppSettings::instance()->applicationsPath();
    const QStringList filters = { QStringLiteral("*.AppImage"), QStringLiteral("*.appimage") };
    const QFileInfoList files = QDir(dir).entryInfoList(filters, QDir::Files);
    if (files.isEmpty())
        return;

    auto *watcher = new QFutureWatcher<QList<AppImageInfo>>(this);
    connect(watcher, &QFutureWatcher<QList<AppImageInfo>>::finished, this, [this, watcher]() {
        watcher->deleteLater();
        for (const AppImageInfo &info : watcher->result()) {
            if (!info.updateInfo.startsWith(QStringLiteral("gh-releases-zsync|")))
                continue;
            const QStringList parts = info.updateInfo.split(QLatin1Char('|'));
            if (parts.size() < 3)
                continue;
            const QString url = QStringLiteral("https://api.github.com/repos/%1/%2/releases/latest")
                                    .arg(parts.at(1), parts.at(2));
            QNetworkRequest request((QUrl(url)));
            request.setRawHeader("User-Agent", "AppImageManager-UpdateCheck/1.0");

            QNetworkReply *reply = m_networkManager->get(request);
            connect(reply, &QNetworkReply::finished, this, [this, reply, info]() mutable {
                reply->deleteLater();
                if (reply->error() != QNetworkReply::NoError)
                    return;
                const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
                if (!doc.isObject())
                    return;
                QString tag = doc.object().value(QLatin1String("tag_name")).toString();
                if (tag.isEmpty())
                    return;
                if (tag.startsWith(QLatin1Char('v')) || tag.startsWith(QLatin1Char('V')))
                    tag.remove(0, 1);
                QString currentVer = info.version;
                if (currentVer.startsWith(QLatin1Char('v')) || currentVer.startsWith(QLatin1Char('V')))
                    currentVer.remove(0, 1);
                if (!isNewerVersion(tag, currentVer))
                    return;
                auto *notification = new KNotification(QStringLiteral("updateAvailable"),
                                                       KNotification::Persistent, this);
                notification->setTitle(i18n("Update Available"));
                notification->setText(i18n("An update is available for %1 (%2 → %3).",
                                           info.cleanName.isEmpty() ? info.originalName : info.cleanName,
                                           currentVer, tag));
                notification->setIconName(info.appId.isEmpty()
                                          ? QStringLiteral("application-x-executable") : info.appId);
                auto *action = notification->addDefaultAction(i18n("Open Manager"));
                connect(action, &KNotificationAction::activated, this, []() {
                    QProcess::startDetached(QStringLiteral("appimagemanager"), {});
                });
                notification->sendEvent();
            });
        }
    });

    watcher->setFuture(QtConcurrent::run([files]() {
        QList<AppImageInfo> results;
        results.reserve(files.size());
        for (const QFileInfo &fi : files)
            results.append(AppImageReader(fi.absoluteFilePath()).read());
        return results;
    }));
}
