// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#include "sandbox.h"
#include "launchenvironment.h"
#include "logging.h"

#include <KConfigGroup>
#include <KSharedConfig>

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QStandardPaths>

#include <csignal>
#include <sys/prctl.h>
#include <unistd.h>

static KConfigGroup sandboxGroup(const QString &filePath)
{
    KSharedConfig::Ptr config = KSharedConfig::openConfig(QStringLiteral("appimagemanagerrc"));
    config->reparseConfiguration(); // written by the dashboard, read by --run
    return config->group(QStringLiteral("Sandbox")).group(QFileInfo(filePath).fileName());
}

// Settings → Security, read fresh: the dashboard writes it, --run reads it.
static KConfigGroup securityGroup()
{
    KSharedConfig::Ptr config = KSharedConfig::openConfig(QStringLiteral("appimagemanagerrc"));
    config->reparseConfiguration();
    return config->group(QStringLiteral("Security"));
}

bool sandboxAllEnabled()
{
    return securityGroup().readEntry("sandboxAll", false) && sandboxAvailable();
}

QStringList sharedSandboxFolders()
{
    return securityGroup().readEntry("sandboxFolders", QStringList());
}

SandboxSettings loadSandbox(const QString &filePath)
{
    const KConfigGroup group = sandboxGroup(filePath);
    const bool enabled = group.hasKey("Enabled") ? group.readEntry("Enabled", false) : sandboxAllEnabled();
    return { enabled, group.readEntry("Folders", QStringList()) };
}

void saveSandbox(const QString &filePath, const SandboxSettings &settings)
{
    KConfigGroup group = sandboxGroup(filePath);
    // The same as Settings → Security says: no own choice, so it keeps following it.
    if (settings.enabled == sandboxAllEnabled())
        group.deleteEntry("Enabled");
    else
        group.writeEntry("Enabled", settings.enabled);
    if (settings.folders.isEmpty())
        group.deleteEntry("Folders");
    else
        group.writeEntry("Folders", settings.folders);
    if (group.keyList().isEmpty())
        group.deleteGroup();
    group.sync();
}

bool sandboxAvailable()
{
    // Looked up once: it is asked for every Details refresh.
    static const bool available = !QStandardPaths::findExecutable(QStringLiteral("bwrap")).isEmpty();
    return available;
}

QStringList sandboxArguments(const QString &appImage, const QString &mountPoint, const QString &settingsFolder,
                             const QStringList &folders, const QString &home, const QString &runtimeDir,
                             const QStringList &args)
{
    QStringList a = {
        QStringLiteral("--die-with-parent"),
        QStringLiteral("--new-session"),
        QStringLiteral("--unshare-pid"),
        // The system, read-only.
        QStringLiteral("--ro-bind"), QStringLiteral("/usr"), QStringLiteral("/usr"),
        QStringLiteral("--ro-bind"), QStringLiteral("/etc"), QStringLiteral("/etc"),
        QStringLiteral("--ro-bind-try"), QStringLiteral("/opt"), QStringLiteral("/opt"),
        QStringLiteral("--ro-bind-try"), QStringLiteral("/var/cache/fontconfig"), QStringLiteral("/var/cache/fontconfig"),
        QStringLiteral("--ro-bind"), QStringLiteral("/sys"), QStringLiteral("/sys"),
        QStringLiteral("--dev-bind"), QStringLiteral("/dev"), QStringLiteral("/dev"),
        QStringLiteral("--proc"), QStringLiteral("/proc"),
        QStringLiteral("--tmpfs"), QStringLiteral("/tmp"),
        QStringLiteral("--bind-try"), QStringLiteral("/tmp/.X11-unix"), QStringLiteral("/tmp/.X11-unix"),
        // Display, sound and the session bus live in the runtime dir; DNS may be resolved's.
        QStringLiteral("--bind"), runtimeDir, runtimeDir,
        QStringLiteral("--ro-bind-try"), QStringLiteral("/run/dbus"), QStringLiteral("/run/dbus"),
        QStringLiteral("--ro-bind-try"), QStringLiteral("/run/systemd/resolve"), QStringLiteral("/run/systemd/resolve"),
    };
    // /bin, /lib, … are links into /usr on most systems, real folders on some.
    for (const QString &dir : { QStringLiteral("/bin"), QStringLiteral("/sbin"), QStringLiteral("/lib"),
                                QStringLiteral("/lib64"), QStringLiteral("/lib32") }) {
        const QFileInfo info(dir);
        if (info.isSymLink())
            a << QStringLiteral("--symlink") << info.symLinkTarget() << dir;
        else if (info.isDir())
            a << QStringLiteral("--ro-bind") << dir << dir;
    }
    // An empty home, with the desktop's look (theme, fonts, icons) readable.
    a << QStringLiteral("--tmpfs") << home;
    for (const QString &rel : { QStringLiteral(".config/kdeglobals"), QStringLiteral(".config/gtk-3.0"),
                                QStringLiteral(".config/gtk-4.0"), QStringLiteral(".config/fontconfig"),
                                QStringLiteral(".gtkrc-2.0"), QStringLiteral(".local/share/fonts"),
                                QStringLiteral(".fonts"), QStringLiteral(".local/share/icons"),
                                QStringLiteral(".icons") }) {
        a << QStringLiteral("--ro-bind-try") << home + QLatin1Char('/') + rel << home + QLatin1Char('/') + rel;
    }
    // What it may change: its settings folder and the folders it was given.
    a << QStringLiteral("--bind") << settingsFolder << settingsFolder;
    for (const QString &folder : folders)
        a << QStringLiteral("--bind-try") << folder << folder;
    // The AppImage (some apps look at $APPIMAGE) and its mounted contents.
    a << QStringLiteral("--ro-bind") << appImage << appImage;
    a << QStringLiteral("--ro-bind") << mountPoint << mountPoint;
    a << QStringLiteral("--chdir") << home;
    a << mountPoint + QStringLiteral("/AppRun");
    a << args;
    return a;
}

static QString ourBinary()
{
    const QString self = QCoreApplication::applicationFilePath();
    if (QFileInfo(self).fileName() == QLatin1String("appimagemanager"))
        return self;
    const QString found = QStandardPaths::findExecutable(QStringLiteral("appimagemanager"));
    return found.isEmpty() ? QStringLiteral("appimagemanager") : found;
}

QStringList launchCommand(const QString &filePath)
{
    if (loadSandbox(filePath).enabled)
        return { ourBinary(), QStringLiteral("--run"), filePath };
    return { filePath };
}

int runAppImage(const QString &filePath, const QStringList &args)
{
    const SandboxSettings sandbox = loadSandbox(filePath);
    // Sandboxed, its settings, data and cache all live in its settings folder.
    if (sandbox.enabled && configFolderName(filePath).isEmpty())
        setConfigFolderName(filePath, defaultConfigFolderName(filePath));

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    const QStringList variables = launchVariables(filePath);
    for (const QString &variable : variables)
        env.insert(variable.section(QLatin1Char('='), 0, 0), variable.section(QLatin1Char('='), 1));

    if (!sandbox.enabled) {
        QProcess app;
        app.setProgram(filePath);
        app.setArguments(args);
        app.setProcessEnvironment(env);
        app.setProcessChannelMode(QProcess::ForwardedChannels);
        app.start();
        if (!app.waitForStarted())
            return 127;
        app.waitForFinished(-1);
        return app.exitCode();
    }
    if (!sandboxAvailable()) {
        qCWarning(AIM_LOG) << "Sandbox: bwrap is not installed; not starting" << filePath;
        return 127;
    }

    const QString folder = configFolderPath(filePath);
    QDir().mkpath(folder + QStringLiteral("/.local/share"));
    QDir().mkpath(folder + QStringLiteral("/.cache"));
    const auto setDefault = [&env, &variables](const QString &key, const QString &value) {
        if (variables.filter(QRegularExpression(QLatin1Char('^') + key + QLatin1Char('='))).isEmpty())
            env.insert(key, value);
    };
    setDefault(QStringLiteral("XDG_DATA_HOME"), folder + QStringLiteral("/.local/share"));
    setDefault(QStringLiteral("XDG_CACHE_HOME"), folder + QStringLiteral("/.cache"));

    // Mounted outside: FUSE doesn't work inside the sandbox. The mount process prints the
    // mount point and stays until it is told to stop.
    QProcess mount;
    mount.setProgram(filePath);
    mount.setArguments({ QStringLiteral("--appimage-mount") });
    // If we are killed (logout), the mount goes too instead of staying behind.
    mount.setChildProcessModifier([]() { ::prctl(PR_SET_PDEATHSIG, SIGTERM); });
    mount.start();
    QString mountPoint;
    while (mountPoint.isEmpty() && mount.waitForReadyRead(30000))
        mountPoint = QString::fromLocal8Bit(mount.readLine()).trimmed();
    if (mountPoint.isEmpty() || !QFileInfo::exists(mountPoint + QStringLiteral("/AppRun"))) {
        qCWarning(AIM_LOG) << "Sandbox: could not mount" << filePath;
        mount.kill();
        mount.waitForFinished(3000);
        return 127;
    }

    env.insert(QStringLiteral("APPIMAGE"), filePath);
    env.insert(QStringLiteral("APPDIR"), mountPoint);
    env.insert(QStringLiteral("ARGV0"), filePath);
    env.insert(QStringLiteral("OWD"), QDir::currentPath());
    const QString runtimeDir = qEnvironmentVariable("XDG_RUNTIME_DIR",
                                                    QStringLiteral("/run/user/") + QString::number(getuid()));
    QProcess bwrap;
    bwrap.setProgram(QStandardPaths::findExecutable(QStringLiteral("bwrap")));
    QStringList folders = sharedSandboxFolders() + sandbox.folders;
    folders.removeDuplicates();
    bwrap.setArguments(sandboxArguments(filePath, mountPoint, folder, folders, QDir::homePath(), runtimeDir, args));
    bwrap.setProcessEnvironment(env);
    bwrap.setProcessChannelMode(QProcess::ForwardedChannels);
    bwrap.start();
    int exitCode = 127;
    if (bwrap.waitForStarted()) {
        bwrap.waitForFinished(-1);
        exitCode = bwrap.exitCode();
    } else {
        qCWarning(AIM_LOG) << "Sandbox: bwrap did not start:" << bwrap.errorString();
    }
    mount.terminate();  // unmounts
    if (!mount.waitForFinished(5000))
        mount.kill();
    return exitCode;
}
