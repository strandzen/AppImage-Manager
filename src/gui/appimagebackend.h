// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#pragma once

#include "../core/appimageinfo.h"
#include "corpsemodel.h"

#include <KJob>
#include <QObject>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

class AppImageIconProvider;

class AppImageBackend : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Use the 'backend' context property")

    Q_PROPERTY(bool   metadataLoaded  READ isMetadataLoaded  NOTIFY metadataLoadedChanged)
    Q_PROPERTY(QString filePath       READ filePath          CONSTANT)
    Q_PROPERTY(QString appName        READ appName           NOTIFY infoChanged)
    Q_PROPERTY(QString cleanName      READ cleanName         NOTIFY infoChanged)
    Q_PROPERTY(QString displayName    READ displayName       NOTIFY infoChanged)
    Q_PROPERTY(QString originalName   READ originalName      NOTIFY infoChanged)
    Q_PROPERTY(QString appVersion     READ appVersion        NOTIFY infoChanged)
    Q_PROPERTY(qint64  appSize        READ appSize           NOTIFY infoChanged)
    Q_PROPERTY(QString formattedSize  READ formattedSize     NOTIFY infoChanged)
    Q_PROPERTY(QString appIconSource  READ appIconSource     NOTIFY infoChanged)
    Q_PROPERTY(bool    isInstalled    READ isInstalled       NOTIFY installedChanged)
    Q_PROPERTY(bool    hasDesktopLink READ hasDesktopLink    NOTIFY desktopLinkChanged)
    Q_PROPERTY(CorpseModel* corpseModel READ corpseModel     CONSTANT)
    Q_PROPERTY(bool isFindingCorpses  READ isFindingCorpses  NOTIFY busyChanged)
    Q_PROPERTY(bool isRemovingItems   READ isRemovingItems   NOTIFY busyChanged)

public:
    explicit AppImageBackend(const QString &appImagePath,
                             AppImageIconProvider *iconProvider,
                             QObject *parent = nullptr);

    bool     isMetadataLoaded() const { return m_metadataLoaded; }
    QString  filePath()         const { return m_appImagePath; }
    QString  appName()          const { return m_info.appName; }
    QString  cleanName()        const { return m_info.cleanName; }
    QString  displayName()      const;
    QString  originalName()     const { return m_info.originalName; }
    QString  appVersion()       const { return m_info.version; }
    qint64   appSize()          const { return m_info.fileSize; }
    QString  formattedSize()    const;
    QString  appIconSource()    const { return m_cachedIconSource; }
    bool     isInstalled()      const { return m_isInstalled; }
    bool     hasDesktopLink()   const { return m_hasDesktopLink; }
    CorpseModel *corpseModel()  const { return m_corpseModel; }
    bool     isFindingCorpses() const { return m_isFindingCorpses; }
    bool     isRemovingItems()  const { return m_isRemovingItems; }

Q_SIGNALS:
    void metadataLoadedChanged();
    void infoChanged();
    void installedChanged();
    void desktopLinkChanged();
    void busyChanged();
    void uninstallFinished();
    void errorOccurred(const QString &message);

public Q_SLOTS:
    void installAppImage();
    void launchAppImage();
    void toggleDesktopLink(bool enable);
    void findCorpses();
    void removeAppImageAndCorpses(const QStringList &corpsePaths, bool includeAppImage);

    Q_INVOKABLE QString formatBytes(qint64 bytes) const;

private Q_SLOTS:
    void onMetadataReady(const AppImageInfo &info);
    void onInstallJobFinished(KJob *job);
    void onRemoveJobFinished(KJob *job);

private:
    void startMetadataLoad();

    QString m_appImagePath;
    AppImageInfo m_info;
    AppImageIconProvider *m_iconProvider;
    CorpseModel *m_corpseModel;

    QString m_cachedIconSource = QStringLiteral("image://icon/application-x-executable");

    bool m_metadataLoaded   = false;
    bool m_isInstalled      = false;
    bool m_hasDesktopLink   = false;
    bool m_isFindingCorpses = false;
    bool m_isRemovingItems  = false;

    QList<QUrl> m_lastTrashedUrls; // trash:/ destinations from last remove job
};
