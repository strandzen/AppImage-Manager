// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#include "launchenvironment.h"
#include "sandbox.h"

#include <KConfigGroup>
#include <KSharedConfig>
#include <KShell>

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>

static KConfigGroup environmentGroup()
{
    KSharedConfig::Ptr config = KSharedConfig::openConfig(QStringLiteral("appimagemanagerrc"));
    config->reparseConfiguration(); // the dashboard writes, the manage window may read
    return config->group(QStringLiteral("Environment"));
}

EnvironmentParse parseEnvironment(const QString &text)
{
    static const QRegularExpression validKey(QStringLiteral("^[A-Za-z_][A-Za-z0-9_]*$"));
    EnvironmentParse result;
    QStringList keys;
    const QStringList lines = text.split(QLatin1Char('\n'));
    for (int i = 0; i < lines.size(); ++i) {
        const QString line = lines.at(i).trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
            continue;
        const int eq = line.indexOf(QLatin1Char('='));
        const QString key = eq > 0 ? line.left(eq).trimmed() : QString();
        if (!validKey.match(key).hasMatch() || keys.contains(key) || line.contains(QChar(0))) {
            result.invalidLines.append(i + 1);
            continue;
        }
        keys.append(key);
        result.variables.append(key + QLatin1Char('=') + line.mid(eq + 1));
    }
    return result;
}

QStringList loadEnvironment(const QString &filePath)
{
    return environmentGroup().readEntry(QFileInfo(filePath).fileName(), QStringList());
}

void saveEnvironment(const QString &filePath, const QStringList &variables)
{
    KConfigGroup group = environmentGroup();
    const QString key = QFileInfo(filePath).fileName();
    if (variables.isEmpty())
        group.deleteEntry(key);
    else
        group.writeEntry(key, variables);
    group.sync();
}

static KConfigGroup configFolderGroup()
{
    KSharedConfig::Ptr config = KSharedConfig::openConfig(QStringLiteral("appimagemanagerrc"));
    config->reparseConfiguration();
    return config->group(QStringLiteral("ConfigFolder"));
}

QString configFolderName(const QString &filePath)
{
    return configFolderGroup().readEntry(QFileInfo(filePath).fileName(), QString());
}

void setConfigFolderName(const QString &filePath, const QString &name)
{
    KConfigGroup group = configFolderGroup();
    const QString key = QFileInfo(filePath).fileName();
    if (name.isEmpty())
        group.deleteEntry(key);
    else
        group.writeEntry(key, name);
    group.sync();
}

QString configFolderPath(const QString &filePath)
{
    const QString name = configFolderName(filePath);
    if (name.isEmpty())
        return {};
    return QFileInfo(filePath).absolutePath() + QStringLiteral("/.config/") + name;
}

QString defaultConfigFolderName(const QString &filePath)
{
    // Name only: drop OS and CPU words, then cut at the version ("-1.2", "_v3").
    static const QRegularExpression version(QStringLiteral("[-_. ]v?\\d.*$"));
    static const QRegularExpression platform(
        QStringLiteral("[-_. ](linux|x86[-_]64|amd64|x64|aarch64|arm64|armhf|i[36]86|anylinux)(?=$|[-_. ])"));
    static const QRegularExpression notSlug(QStringLiteral("[^a-z0-9._-]+"));
    QString name = QFileInfo(filePath).fileName().toLower();
    if (name.endsWith(QLatin1String(".appimage")))
        name.chop(9);
    for (int i = 0; i < 3; ++i)  // "-linux-x86_64": one word per pass (before the version: "_64")
        name.remove(platform);
    name.remove(version);
    name.replace(notSlug, QStringLiteral("-"));
    while (name.startsWith(QLatin1Char('.')) || name.startsWith(QLatin1Char('-')))
        name.remove(0, 1);
    return name.isEmpty() ? QStringLiteral("app") : name;
}

QStringList launchVariables(const QString &filePath)
{
    QStringList variables = loadEnvironment(filePath);
    const QString folder = configFolderPath(filePath);
    if (folder.isEmpty())
        return variables;
    QDir().mkpath(folder);
    const auto isSet = [&variables](const QString &key) {
        return std::any_of(variables.cbegin(), variables.cend(), [&key](const QString &v) {
            return v.startsWith(key + QLatin1Char('='));
        });
    };
    if (!isSet(QStringLiteral("XDG_CONFIG_HOME")))
        variables.append(QStringLiteral("XDG_CONFIG_HOME=") + folder);
    if (!isSet(QStringLiteral("XDG_CONFIG_DIRS")))
        variables.append(QStringLiteral("XDG_CONFIG_DIRS=")
                         + QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + QLatin1Char(':')
                         + qEnvironmentVariable("XDG_CONFIG_DIRS", QStringLiteral("/etc/xdg")));
    return variables;
}

bool launchWithEnvironment(const QString &filePath)
{
    // Sandboxed: `appimagemanager --run` mounts it and starts it in the sandbox.
    const QStringList command = launchCommand(filePath);
    if (command.size() > 1)
        return QProcess::startDetached(command.first(), command.mid(1));

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    for (const QString &variable : launchVariables(filePath))
        env.insert(variable.section(QLatin1Char('='), 0, 0), variable.section(QLatin1Char('='), 1));
    QProcess process;
    process.setProgram(filePath);
    process.setProcessEnvironment(env);
    return process.startDetached();
}

QString environmentExecPrefix(const QStringList &variables)
{
    if (variables.isEmpty())
        return {};
    QString prefix = QStringLiteral("env");
    for (const QString &variable : variables)
        prefix += QLatin1Char(' ') + KShell::quoteArg(variable);
    return prefix + QLatin1Char(' ');
}
