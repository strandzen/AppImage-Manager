// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#include "launchenvironment.h"

#include <KConfigGroup>
#include <KSharedConfig>
#include <KShell>

#include <QFileInfo>
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

bool launchWithEnvironment(const QString &filePath)
{
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    for (const QString &variable : loadEnvironment(filePath))
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
