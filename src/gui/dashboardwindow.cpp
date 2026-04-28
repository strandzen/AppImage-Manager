// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#include "dashboardwindow.h"
#include "appimagebackend.h"
#include "appimageiconprovider.h"
#include "appimagelistmodel.h"
#include "appimagesortfiltermodel.h"
#include "logging.h"

#include <KLocalizedQmlContext>
#include <KQuickIconProvider>

#include <QDesktopServices>
#include <QFileInfo>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QUrl>

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
    m_iconProvider = new AppImageIconProvider;
    m_listModel    = new AppImageListModel(m_iconProvider, this);
    m_proxyModel   = new AppImageSortFilterModel(this);
    m_proxyModel->setSourceModel(m_listModel);

    m_engine = new QQmlApplicationEngine(this);
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
        deleteLater();
    });

    m_listModel->scan();

    m_window->show();
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

QObject *DashboardWindow::createStorageBackend(const QString &filePath)
{
    if (m_storageBackend) m_storageBackend->deleteLater();
    m_storageBackend = createBackend(filePath, false);
    return m_storageBackend;
}

void DashboardWindow::openInFileManager(const QString &path)
{
    QFileInfo fi(path);
    QDesktopServices::openUrl(QUrl::fromLocalFile(fi.isDir() ? path : fi.absolutePath()));
}
