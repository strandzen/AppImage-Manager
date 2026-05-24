// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#include "gui/appimagewindow.h"
#include "gui/dashboardwindow.h"
#include "core/updatedaemon.h"
#include "version.h"

#include <KAboutData>
#include <KCrash>
#include <KDBusService>
#include <KLocalizedString>

#include <QApplication>
#include <QCommandLineParser>
#include <QIcon>
#include <QUrl>

int main(int argc, char *argv[])
{
    KLocalizedString::setApplicationDomain("appimagemanager");

    QApplication app(argc, argv);

    KAboutData about(
        QStringLiteral("appimagemanager"),
        i18n("AppImage Manager"),
        QStringLiteral(APPIMAGEMANAGER_VERSION_STRING),
        i18n("Manage AppImage files"),
        KAboutLicense::GPL_V2,
        i18n("© 2024 AppImage Manager Contributors")
    );
    about.setOrganizationDomain("appimagemanager.org");
    about.setDesktopFileName(QStringLiteral("io.github.strandzen.AppImageManager"));
    KAboutData::setApplicationData(about);

    QApplication::setWindowIcon(QIcon::fromTheme(QStringLiteral("appimagemanager"),
                                                     QIcon::fromTheme(QStringLiteral("application-x-executable"))));

    KCrash::initialize();

    QCommandLineParser parser;
    parser.setApplicationDescription(about.shortDescription());
    about.setupCommandLine(&parser);
    parser.addPositionalArgument(
        QStringLiteral("file"),
        i18n("AppImage file to manage. If omitted, opens the dashboard."),
        QStringLiteral("[file]"));
    parser.addOption(QCommandLineOption(QStringLiteral("daemon"),    i18n("Run in background as an update daemon")));
    parser.addOption(QCommandLineOption(QStringLiteral("dashboard"), i18n("Open the dashboard window")));
    parser.process(app);
    about.processCommandLine(&parser);

    const bool isDaemon = parser.isSet(QStringLiteral("daemon"));
    const QStringList args = parser.positionalArguments();
    const bool isFileMode = !args.isEmpty();
    const bool isDashboard = !isDaemon && !isFileMode;

    // Per-mode KDBusService policy:
    //   file mode  → Multiple so Dolphin can open several manage windows in
    //                parallel without single-instance lockout.
    //   daemon     → Multiple; the daemon registers its own well-known
    //                service name (io.github.appimagemanager.Daemon).
    //   dashboard  → Unique so a second `appimagemanager --dashboard`
    //                invocation raises the existing window instead of
    //                opening a duplicate.
    KDBusService service(isDashboard ? KDBusService::Unique : KDBusService::Multiple);

    if (isDashboard) {
        QObject::connect(&service, &KDBusService::activateRequested,
                         &app, [](const QStringList &, const QString &) {
            DashboardWindow::open();  // raises the existing instance
        });
    }

    if (isDaemon) {
        UpdateDaemon *daemon = new UpdateDaemon(&app);
        daemon->start();
        return app.exec();
    }

    if (isFileMode)
        AppImageWindow::open(args.first());
    else
        DashboardWindow::open();

    return app.exec();
}
