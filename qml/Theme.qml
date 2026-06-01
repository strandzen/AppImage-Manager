// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
pragma Singleton
import QtQuick
import org.kde.kirigami as Kirigami
import appimagemanager

QtObject {
    readonly property color cardBorderColor: AppSettings.accentBorders
        ? Kirigami.Theme.focusColor
        : (Kirigami.Theme.textColor && Kirigami.Theme.textColor.r !== undefined
            ? Qt.rgba(Kirigami.Theme.textColor.r,
                      Kirigami.Theme.textColor.g,
                      Kirigami.Theme.textColor.b, 0.15)
            : Qt.rgba(0.5, 0.5, 0.5, 0.15))
}
