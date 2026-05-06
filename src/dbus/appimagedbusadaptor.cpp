// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#include "appimagedbusadaptor.h"
#include "../gui/appimagelistmodel.h"
#include "version.h"

#include <QVariantMap>

AppImageDBusAdaptor::AppImageDBusAdaptor(AppImageListModel *model)
    : QDBusAbstractAdaptor(model)
    , m_model(model)
{
    setAutoRelaySignals(false);
}

QVariantList AppImageDBusAdaptor::InstalledAppImages() const
{
    QVariantList result;
    const int count = m_model->rowCount();
    for (int i = 0; i < count; ++i) {
        const QModelIndex idx = m_model->index(i);
        if (!m_model->data(idx, AppImageListModel::MetadataLoadedRole).toBool())
            continue;
        QVariantMap entry;
        entry[QStringLiteral("path")]          = m_model->data(idx, AppImageListModel::FilePathRole);
        entry[QStringLiteral("name")]          = m_model->data(idx, AppImageListModel::DisplayNameRole);
        entry[QStringLiteral("version")]       = m_model->data(idx, AppImageListModel::VersionRole);
        entry[QStringLiteral("hasDesktopLink")]= m_model->data(idx, AppImageListModel::HasDesktopLinkRole);
        result << entry;
    }
    return result;
}

QString AppImageDBusAdaptor::Version() const
{
    return QStringLiteral(APPIMAGEMANAGER_VERSION_STRING);
}
