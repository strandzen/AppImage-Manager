// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#include "dashboardwindow.h"
#include "appimagebackend.h"
#include "appimageiconprovider.h"
#include "appimagelistmodel.h"
#include "appimagesortfiltermodel.h"
#include "appimagemanager.h"
#include "appsettings.h"
#include "logging.h"

#include <KIO/CopyJob>
#include <KLocalizedQmlContext>
#include <KLocalizedString>
#include <KNotification>
#include <KSharedConfig>
#include <KConfigGroup>
#include <KQuickIconProvider>
#include "../dbus/appimagedbusadaptor.h"

#include <QDBusConnection>

#include <algorithm>

#include <QCoreApplication>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QUrl>
#include <QWindow>

DashboardWindow *DashboardWindow::s_instance = nullptr;

// static
void DashboardWindow::open()
{
    if (s_instance) {
        if (s_instance->m_window) {
            s_instance->m_window->raise();
            s_instance->m_window->requestActivate();
        }
        return;
    }
    s_instance = new DashboardWindow;
    s_instance->setupAndShow();
}

DashboardWindow::DashboardWindow(QObject *parent)
    : QObject(parent)
{
}

void DashboardWindow::setupAndShow()
{
    // Engine must be created FIRST and own the data models.
    // Qt deletes children FIFO, so if models were children of DashboardWindow
    // they'd be freed before the engine — causing a crash when QML teardown
    // tries to evaluate bindings that reference them.
    m_engine = new QQmlApplicationEngine(this);

    m_iconProvider = new AppImageIconProvider;  // ownership transferred to engine via addImageProvider below
    m_listModel    = new AppImageListModel(m_iconProvider, m_engine);
    m_proxyModel   = new AppImageSortFilterModel(m_engine);
    m_proxyModel->setSourceModel(m_listModel);

    // Disconnect the engine's auto-quit so the dashboard closing does not kill
    // the whole process when a manage window is also open. We handle quit
    // ourselves in the closing handler below.
    // Note: QCoreApplication::quit/exit are static slots — must use SIGNAL/SLOT strings.
    QObject::disconnect(m_engine, SIGNAL(quit()), qApp, SLOT(quit()));
    QObject::disconnect(m_engine, SIGNAL(exit(int)), qApp, SLOT(exit(int)));

    KLocalization::setupLocalizedContext(m_engine);
    m_engine->addImageProvider(QStringLiteral("appimage"), m_iconProvider);
    m_engine->addImageProvider(QStringLiteral("icon"), new KQuickIconProvider);
    m_engine->rootContext()->setContextProperty(QStringLiteral("listModel"),           m_listModel);
    m_engine->rootContext()->setContextProperty(QStringLiteral("proxyModel"),           m_proxyModel);
    m_engine->rootContext()->setContextProperty(QStringLiteral("dashboardController"), this);

    m_engine->load(QUrl(QStringLiteral("qrc:/appimagemanager/DashboardWindow.qml")));

    const QList<QObject *> rootObjects = m_engine->rootObjects();
    if (rootObjects.isEmpty()) {
        qCWarning(AIM_LOG) << "Failed to load DashboardWindow.qml";
        s_instance = nullptr;
        deleteLater();
        return;
    }

    m_window = qobject_cast<QQuickWindow *>(rootObjects.first());
    if (!m_window) {
        qCWarning(AIM_LOG) << "Root object of DashboardWindow.qml is not a QQuickWindow";
        s_instance = nullptr;
        deleteLater();
        return;
    }

    connect(m_window, &QQuickWindow::closing, this, [this]() {
        s_instance = nullptr;

        KConfigGroup grp = KSharedConfig::openConfig()->group(QStringLiteral("DashboardWindow"));
        grp.writeEntry("size", m_window->size());
        grp.writeEntry("position", m_window->position());
        grp.sync();

        // Quit only if the dashboard was the sole visible window.
        const auto windows = qApp->topLevelWindows();
        const bool otherVisible = std::any_of(windows.begin(), windows.end(),
            [this](QWindow *w) { return w != m_window && w->isVisible(); });
        if (!otherVisible)
            QCoreApplication::quit();

        deleteLater();
    });

    // Register D-Bus adaptor for querying installed AppImages
    new AppImageDBusAdaptor(m_listModel);
    if (!QDBusConnection::sessionBus().registerObject(
            QStringLiteral("/io/github/appimagemanager/Manager"), m_listModel))
        qCWarning(AIM_LOG) << "Failed to register D-Bus object /io/github/appimagemanager/Manager";
    if (!QDBusConnection::sessionBus().registerService(
            QStringLiteral("io.github.appimagemanager.Manager1")))
        qCWarning(AIM_LOG) << "Failed to register D-Bus service io.github.appimagemanager.Manager1";

    m_listModel->scan();

    // Restore saved window geometry before showing.
    {
        KConfigGroup grp = KSharedConfig::openConfig()->group(QStringLiteral("DashboardWindow"));
        const QSize sz = grp.readEntry("size", QSize());
        if (sz.isValid())
            m_window->resize(sz);
        const QPoint pos = grp.readEntry("position", QPoint(-1, -1));
        if (pos.x() >= 0 && pos.y() >= 0)
            m_window->setPosition(pos);
    }

    m_window->show();
    m_window->setTitle(QStringLiteral("AppImage Dashboard"));
    m_window->raise();
    m_window->requestActivate();
}

AppImageBackend *DashboardWindow::createBackend(const QString &filePath, bool withCorpses)
{
    auto *iconProvider = new AppImageIconProvider;
    auto *backend      = new AppImageBackend(filePath, iconProvider, this);
    iconProvider->setParent(backend);
    if (withCorpses)
        connect(backend, &AppImageBackend::metadataLoadedChanged, backend,
                [backend]() { backend->findCorpses(); }, Qt::SingleShotConnection);
    return backend;
}

QObject *DashboardWindow::createUninstallBackend(const QString &filePath)
{
    if (m_uninstallBackend) m_uninstallBackend->deleteLater();
    m_uninstallBackend = createBackend(filePath, true);
    connect(m_uninstallBackend, &AppImageBackend::uninstallFinished,
            m_listModel, &AppImageListModel::refresh);
    return m_uninstallBackend;
}

void DashboardWindow::installFromPath(const QUrl &url)
{
    const QString dest = AppSettings::instance()->applicationsPath();
    auto *job = AppImageManager::installAppImage(url, dest);
    connect(job, &KJob::result, this, [url](KJob *j) {
        if (j->error()) {
            qCWarning(AIM_LOG) << "Install failed:" << j->errorString();
            auto *n = KNotification::event(QStringLiteral("installFailed"),
                                           i18n("Install failed"),
                                           j->errorString(),
                                           QStringLiteral("dialog-error"));
            n->sendEvent();
        }
        // On success the QFileSystemWatcher in AppImageListModel picks up the new file automatically.
    });
    job->start();
}
