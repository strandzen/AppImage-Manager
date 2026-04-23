// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#include "appimagebackend.h"
#include "appimageiconprovider.h"
#include "corpsemodel.h"
#include "logging.h"
#include "../core/appimagemanager.h"
#include "../core/appimagereader.h"

#include <KFormat>
#include <KJob>
#include <KIO/CopyJob>
#include <KLocalizedString>
#include <KNotification>

#include <QDir>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QIcon>
#include <QtConcurrent/QtConcurrent>
#include <QUrl>

AppImageBackend::AppImageBackend(const QString &appImagePath,
                                 AppImageIconProvider *iconProvider,
                                 QObject *parent)
    : QObject(parent)
    , m_appImagePath(appImagePath)
    , m_manager(new AppImageManager(this))
    , m_iconProvider(iconProvider)
    , m_corpseModel(new CorpseModel(this))
{
    qRegisterMetaType<AppImageInfo>();

    m_info.originalName = QFileInfo(appImagePath).fileName();

    const QString applicationsDir = QDir::homePath() + QLatin1String("/Applications");
    m_isInstalled = (QFileInfo(appImagePath).absolutePath() == applicationsDir);

    startMetadataLoad();
}

void AppImageBackend::startMetadataLoad()
{
    auto *watcher = new QFutureWatcher<AppImageInfo>(this);
    connect(watcher, &QFutureWatcher<AppImageInfo>::finished, this, [this, watcher]() {
        onMetadataReady(watcher->result());
        watcher->deleteLater();
    });
    watcher->setFuture(QtConcurrent::run([path = m_appImagePath]() {
        return AppImageReader(path).read();
    }));
}

void AppImageBackend::onMetadataReady(const AppImageInfo &info)
{
    m_info = info;
    m_info.originalName = QFileInfo(m_appImagePath).fileName();

    if (!info.iconData.isEmpty())
        m_iconProvider->setIconData(info.iconData, info.iconExt);

    m_hasDesktopLink = m_manager->isDesktopLinkEnabled(m_appImagePath, m_info);
    m_metadataLoaded = true;

    Q_EMIT infoChanged();
    Q_EMIT desktopLinkChanged();
    Q_EMIT metadataLoadedChanged();
}

// ──────────────────────────────────────────────────────────────────────────────
// Q_INVOKABLE
// ──────────────────────────────────────────────────────────────────────────────

QString AppImageBackend::formatBytes(qint64 bytes) const
{
    static const KFormat fmt;
    return fmt.formatByteSize(static_cast<double>(bytes));
}

// ──────────────────────────────────────────────────────────────────────────────
// Slots called from QML
// ──────────────────────────────────────────────────────────────────────────────

void AppImageBackend::installAppImage()
{
    auto *job = m_manager->installAppImage(QUrl::fromLocalFile(m_appImagePath));
    connect(job, &KJob::result, this, &AppImageBackend::onInstallJobFinished);
    job->start();
}

void AppImageBackend::toggleDesktopLink(bool enable)
{
    if (enable)
        m_manager->createDesktopLink(m_appImagePath, m_info);
    else
        m_manager->removeDesktopLink(m_appImagePath, m_info);

    m_hasDesktopLink = enable;
    Q_EMIT desktopLinkChanged();
}

void AppImageBackend::findCorpses()
{
    m_isFindingCorpses = true;
    m_corpseModel->clear();
    Q_EMIT busyChanged();

    auto *watcher = new QFutureWatcher<QList<QPair<QString, qint64>>>(this);
    connect(watcher, &QFutureWatcher<QList<QPair<QString, qint64>>>::finished,
            this, [this, watcher]() {
        m_corpseModel->setCorpses(watcher->result());
        m_isFindingCorpses = false;
        watcher->deleteLater();
        Q_EMIT busyChanged();
    });
    watcher->setFuture(QtConcurrent::run([mgr = m_manager, info = m_info]() {
        return mgr->findCorpses(info);
    }));
}

void AppImageBackend::removeAppImageAndCorpses(const QStringList &paths)
{
    m_isRemovingItems = true;
    Q_EMIT busyChanged();

    const bool removeAppImage = paths.contains(QLatin1String("APPIMAGE_ITSELF"));

    QList<QUrl> corpseUrls;
    for (const QString &p : paths) {
        if (p != QLatin1String("APPIMAGE_ITSELF"))
            corpseUrls << QUrl::fromLocalFile(p);
    }

    KJob *job = nullptr;
    if (removeAppImage) {
        job = m_manager->removeItems(QUrl::fromLocalFile(m_appImagePath), m_info, corpseUrls);
    } else if (!corpseUrls.isEmpty()) {
        job = KIO::trash(corpseUrls, KIO::HideProgressInfo);
    } else {
        m_isRemovingItems = false;
        Q_EMIT busyChanged();
        Q_EMIT uninstallFinished();
        return;
    }

    connect(job, &KJob::result, this, &AppImageBackend::onRemoveJobFinished);
    job->start();
}

// ──────────────────────────────────────────────────────────────────────────────
// KJob result handlers
// ──────────────────────────────────────────────────────────────────────────────

void AppImageBackend::onInstallJobFinished(KJob *job)
{
    if (job->error()) {
        qCWarning(AIM_LOG) << "Install failed:" << job->errorString();
        Q_EMIT errorOccurred(job->errorString());
        return;
    }

    m_appImagePath = QDir::homePath()
                   + QLatin1String("/Applications/")
                   + QFileInfo(m_appImagePath).fileName();
    m_isInstalled = true;
    Q_EMIT installedChanged();

    KNotification::event(QStringLiteral("installed"),
                         i18n("AppImage installed"),
                         i18n("%1 was moved to Applications.", m_info.appName),
                         QStringLiteral("application-x-executable"),
                         KNotification::CloseOnTimeout);
}

void AppImageBackend::onRemoveJobFinished(KJob *job)
{
    m_isRemovingItems = false;
    Q_EMIT busyChanged();

    if (job->error()) {
        qCWarning(AIM_LOG) << "Remove failed:" << job->errorString();
        Q_EMIT errorOccurred(job->errorString());
        return;
    }

    KNotification::event(QStringLiteral("removed"),
                         i18n("AppImage removed"),
                         i18n("%1 was moved to Trash.", m_info.appName),
                         QStringLiteral("edit-delete"),
                         KNotification::CloseOnTimeout);

    Q_EMIT uninstallFinished();
}

// ──────────────────────────────────────────────────────────────────────────────
// Property helpers
// ──────────────────────────────────────────────────────────────────────────────

QString AppImageBackend::appIconSource() const
{
    if (!m_info.appId.isEmpty() && QIcon::hasThemeIcon(m_info.appId))
        return QLatin1String("image://icon/") + m_info.appId;
    if (!m_info.iconData.isEmpty())
        return QStringLiteral("image://appimage/icon");
    return QStringLiteral("image://icon/application-x-executable");
}
