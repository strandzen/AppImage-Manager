// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#include "appimageactionplugin.h"
#include "../gui/appimagewindow.h"

#include <KFileItem>
#include <KFileItemListProperties>
#include <KLocalizedString>
#include <KPluginFactory>

#include <QAction>
#include <QIcon>
#include <QString>

K_PLUGIN_CLASS_WITH_JSON(AppImageActionPlugin, "appimagemanager.json")

AppImageActionPlugin::AppImageActionPlugin(QObject *parent,
                                           const KPluginMetaData &metaData,
                                           const QVariantList &args)
    : KAbstractFileItemActionPlugin(parent)
{
    Q_UNUSED(metaData)
    Q_UNUSED(args)
}

QList<QAction *> AppImageActionPlugin::actions(const KFileItemListProperties &props,
                                                QWidget * /*parentWidget*/)
{
    // Only act on a single local AppImage file
    if (props.items().count() != 1)
        return {};

    const KFileItemList items = props.items();
    const KFileItem &item = items.constFirst();
    const QString mime = item.mimetype();
    if (mime != QLatin1String("application/vnd.appimage")
        && mime != QLatin1String("application/x-iso9660-appimage")) {
        return {};
    }

    const QString path = item.localPath();
    if (path.isEmpty())
        return {};

    auto *action = new QAction(QIcon::fromTheme(QStringLiteral("application-x-executable")),
                               i18n("Manage AppImage"),
                               this);

    connect(action, &QAction::triggered, this, [path]() {
        AppImageWindow::open(path);
    });

    return {action};
}

#include "appimageactionplugin.moc"
