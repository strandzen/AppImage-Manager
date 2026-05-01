// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#include "gui/appimagewindow.h"
#include "gui/dashboardwindow.h"
#include "core/updatedaemon.h"

#include <KAboutData>
#include <KCrash>
#include <KDBusService>
#include <KLocalizedString>

#include <QGuiApplication>
#include <QCommandLineParser>
#include <QIcon>
#include <QUrl>

int main(int argc, char *argv[])
{
    KLocalizedString::setApplicationDomain("appimagemanager");

    QGuiApplication app(argc, argv);

    KAboutData about(
        QStringLiteral("appimagemanager"),
        i18n("AppImage Manager"),
        QStringLiteral("0.1.0"),
        i18n("Manage AppImage files"),
        KAboutLicense::GPL_V2,
        i18n("© 2024 AppImage Manager Contributors")
    );
    KAboutData::setApplicationData(about);

    QGuiApplication::setWindowIcon(QIcon::fromTheme(QStringLiteral("appimagemanager"),
                                                     QIcon::fromTheme(QStringLiteral("application-x-executable"))));

    KCrash::initialize();

    // Each invocation manages one file — use Multiple mode so Dolphin can open
    // several AppImages simultaneously without being blocked by single-instance logic.
    KDBusService service(KDBusService::Multiple);

    QCommandLineParser parser;
    parser.setApplicationDescription(about.shortDescription());
    about.setupCommandLine(&parser);
    parser.addPositionalArgument(
        QStringLiteral("file"),
        i18n("AppImage file to manage. If omitted, opens the dashboard."),
        QStringLiteral("[file]"));
    parser.addOption(QCommandLineOption(QStringLiteral("daemon"), i18n("Run in background as an update daemon")));
    parser.process(app);
    about.processCommandLine(&parser);

    if (parser.isSet(QStringLiteral("daemon"))) {
        UpdateDaemon *daemon = new UpdateDaemon(&app);
        daemon->start();
        return app.exec();
    }

    const QStringList args = parser.positionalArguments();
    if (args.isEmpty()) {
        DashboardWindow::open();
    } else {
        AppImageWindow::open(args.first());
    }

    return app.exec();
}
