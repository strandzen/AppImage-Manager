// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#include "gui/appimagewindow.h"
#include "gui/dashboardwindow.h"
#include "core/updatedaemon.h"
#include "core/sandbox.h"
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

    // appimagemanager --run <file> [args…]: start an AppImage the way its settings say
    // (sandboxed, with its environment) and wait for it. Used by app menu entries; no GUI.
    if (argc >= 3 && qstrcmp(argv[1], "--run") == 0) {
        QCoreApplication app(argc, argv);
        QStringList appArgs;
        for (int i = 3; i < argc; ++i)
            appArgs.append(QString::fromLocal8Bit(argv[i]));
        return runAppImage(QString::fromLocal8Bit(argv[2]), appArgs);
    }

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

    // The page a dashboard invocation asks for (also for arguments forwarded to a running one).
    const auto requestedPage = [](const QCommandLineParser &p) {
        if (p.isSet(QStringLiteral("welcome")))
            return QStringLiteral("welcome");
        return p.isSet(QStringLiteral("settings")) ? QStringLiteral("settings") : QString();
    };

    // Also used to read the arguments a second dashboard invocation forwards (activateRequested).
    const auto setupParser = [](QCommandLineParser &p) {
        p.addPositionalArgument(
            QStringLiteral("file"),
            i18n("AppImage file to manage. If omitted, opens the dashboard."),
            QStringLiteral("[file]"));
        p.addOption(QCommandLineOption(QStringLiteral("daemon"),    i18n("Run in background as an update daemon")));
        p.addOption(QCommandLineOption(QStringLiteral("dashboard"), i18n("Open the dashboard window")));
        p.addOption(QCommandLineOption(QStringLiteral("settings"),  i18n("Open the dashboard on its Settings page")));
        p.addOption(QCommandLineOption(QStringLiteral("welcome"),   i18n("Show the welcome page again")));
        p.addOption(QCommandLineOption(QStringLiteral("select"),
                                       i18n("Open the dashboard with this installed AppImage selected"),
                                       QStringLiteral("path")));
    };

    QCommandLineParser parser;
    parser.setApplicationDescription(about.shortDescription());
    about.setupCommandLine(&parser);
    setupParser(parser);
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
                         &app, [setupParser, requestedPage](const QStringList &arguments, const QString &) {
            QCommandLineParser forwarded;
            setupParser(forwarded);
            forwarded.parse(arguments);
            // Raises the existing instance, on the requested page.
            DashboardWindow::open(requestedPage(forwarded), forwarded.value(QStringLiteral("select")));
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
        DashboardWindow::open(requestedPage(parser), parser.value(QStringLiteral("select")));

    return app.exec();
}
