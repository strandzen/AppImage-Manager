// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#include "appimagewindow.h"
#include "appimagebackend.h"
#include "appimageiconprovider.h"
#include "logging.h"

#include <KLocalizedQmlContext>
#include <KQuickIconProvider>

#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QUrl>

QHash<QString, AppImageWindow *> AppImageWindow::s_instances;

// static
void AppImageWindow::open(const QString &appImagePath)
{
    auto it = s_instances.find(appImagePath);
    if (it != s_instances.end()) {
        if (it.value()->m_window) {
            it.value()->m_window->raise();
            it.value()->m_window->requestActivate();
        }
        return;
    }

    auto *instance = new AppImageWindow(appImagePath);
    s_instances.insert(appImagePath, instance);
    instance->setupAndShow();
}

AppImageWindow::AppImageWindow(const QString &path, QObject *parent)
    : QObject(parent)
    , m_path(path)
{
}

void AppImageWindow::setupAndShow()
{
    // Provider has no QObject parent — engine takes ownership via addImageProvider.
    // Backend is created first so it is a QObject child before the engine, meaning
    // Qt destroys it first on teardown. The engine (and thus the provider) outlives
    // the backend, so m_backend's pointer to the provider is never dangling.
    m_iconProvider = new AppImageIconProvider;
    m_backend = new AppImageBackend(m_path, m_iconProvider, this);

    m_engine = new QQmlApplicationEngine(this);
    KLocalization::setupLocalizedContext(m_engine);
    m_engine->addImageProvider(QStringLiteral("appimage"), m_iconProvider);
    m_engine->addImageProvider(QStringLiteral("icon"), new KQuickIconProvider);
    m_engine->rootContext()->setContextProperty(QStringLiteral("backend"), m_backend);

    m_engine->load(QUrl(QStringLiteral("qrc:/org/kde/appimagemanager/ManageWindow.qml")));

    const QList<QObject *> rootObjects = m_engine->rootObjects();
    if (rootObjects.isEmpty()) {
        qCWarning(AIM_LOG) << "Failed to load ManageWindow.qml";
        s_instances.remove(m_path);
        deleteLater();
        return;
    }

    m_window = qobject_cast<QQuickWindow *>(rootObjects.first());
    if (!m_window) {
        qCWarning(AIM_LOG) << "Root object is not a QQuickWindow";
        s_instances.remove(m_path);
        deleteLater();
        return;
    }

    connect(m_window, &QQuickWindow::closing, this, [this]() {
        s_instances.remove(m_path);
        deleteLater();
    });

    m_window->show();
    m_window->raise();
    m_window->requestActivate();
}
