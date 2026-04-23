// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#pragma once

#include <KAbstractFileItemActionPlugin>
#include <KPluginMetaData>

#include <QList>
#include <QVariantList>

class KFileItemListProperties;
class QAction;
class QWidget;

class AppImageActionPlugin : public KAbstractFileItemActionPlugin
{
    Q_OBJECT
public:
    AppImageActionPlugin(QObject *parent,
                         const KPluginMetaData &metaData,
                         const QVariantList &args);

    QList<QAction *> actions(const KFileItemListProperties &fileItemInfos,
                              QWidget *parentWidget) override;
};
