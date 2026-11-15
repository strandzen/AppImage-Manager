// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import appimagemanager

Kirigami.Dialog {
    id: aboutDialog
    title: i18n("About AppImage Manager")
    padding: Kirigami.Units.largeSpacing
    standardButtons: Kirigami.Dialog.Close

    preferredWidth: Kirigami.Units.gridUnit * 24
    preferredHeight: Kirigami.Units.gridUnit * 28

    onOpened: aboutContent.fetchContributors()

    Controls.ScrollView {
        id: scrollView
        implicitHeight: Kirigami.Units.gridUnit * 28
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        Controls.ScrollBar.horizontal.policy: Controls.ScrollBar.AlwaysOff
        Controls.ScrollBar.vertical.policy: Controls.ScrollBar.AsNeeded

        AboutContent {
            id: aboutContent
            width: scrollView.availableWidth
        }
    }
}
