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

// Owns all state and async operations for a single open AppImage.
// One instance per AppImageWindow; also used by DashboardWindow for uninstall/storage operations.
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
    // Another copy of this app already in Applications (see ExistingState).
    Q_PROPERTY(int     existingState    READ existingState    NOTIFY existingChanged)
    Q_PROPERTY(QString existingVersion  READ existingVersion  NOTIFY existingChanged)
    // The installed copy has this file's name: installing next to it is not possible.
    Q_PROPERTY(bool    existingSameFile READ existingSameFile NOTIFY existingChanged)

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
    int      existingState()    const { return m_existingState; }
    QString  existingVersion()  const { return m_existingInfo.version; }
    bool     existingSameFile() const { return m_existingSameFile; }

    // This file compared with the copy already installed.
    enum ExistingState {
        NotInstalled   = 0,
        ThisNewer      = 1,
        SameVersion    = 2,
        ThisOlder      = 3,
        VersionUnknown = 4,  // a version is missing or the two can't be compared
    };
    Q_ENUM(ExistingState)

    // Mirror of SignatureState enum for QML access.
    // Values must stay in sync with appimageinfo.h::SignatureState.
    enum SigState {
        SigUnchecked      = 0,
        SigValid          = 1,
        SigInvalid        = 2,
        SigUnsigned       = 3,
        SigGpgUnavailable = 4,
    };
    Q_ENUM(SigState)

Q_SIGNALS:
    void metadataLoadedChanged();
    void infoChanged();
    void installedChanged();
    void desktopLinkChanged();
    void busyChanged();
    void existingChanged();
    void uninstallFinished();
    void errorOccurred(const QString &message);
    // Emitted after beginInstall() finishes the async signature check.
    // QML should inspect state: if SigValid/SigGpgUnavailable/SigUnchecked → call doInstall();
    // if SigUnsigned or SigInvalid → show warning first, then optionally call doInstall().
    void signatureCheckReady(int state);

public Q_SLOTS:
    // Starts the async sig-check, then emits signatureCheckReady. replace: the installed
    // copy (existingState) goes to the Trash and its settings move to this file.
    void beginInstall(bool replace = false);
    void doInstall();     // performs the actual KIO install job (call after signatureCheckReady)
    void launchAppImage();
    void toggleDesktopLink(bool enable);
    void findCorpses();
    void removeAppImageAndCorpses(const QStringList &corpsePaths, bool includeAppImage);

    Q_INVOKABLE QString formatBytes(qint64 bytes) const;
    Q_INVOKABLE void    openDashboard();

private Q_SLOTS:
    void onMetadataReady(const AppImageInfo &info);
    void onInstallJobFinished(KJob *job);

private:
    void startMetadataLoad();
    void findInstalledCopy();
    void startInstallJob();
    bool finishReplace();
    void onRemoveJobFinished(KJob *job, const QList<QUrl> &trashedUrls);

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

    QString      m_existingPath;
    AppImageInfo m_existingInfo;
    int          m_existingState    = NotInstalled;
    bool         m_existingSameFile = false;
    bool         m_replace          = false;
    bool         m_oldHadLink       = false;
};
