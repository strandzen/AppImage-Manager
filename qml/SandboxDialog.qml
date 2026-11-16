// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import Qt.labs.platform as Platform
import appimagemanager

// Per-AppImage sandbox (bubblewrap): on/off and the folders the app may use besides its
// settings folder.
Kirigami.Dialog {
    id: dialog

    property int    proxyRow: -1
    property string appName: ""
    property bool   available: true
    property string settingsFolder: ""
    property var    folders: []

    signal saved(bool enabled)

    title: dialog.appName ? i18n("Sandbox — %1", dialog.appName) : i18n("Sandbox")
    padding: Kirigami.Units.largeSpacing
    standardButtons: Kirigami.Dialog.Cancel
    preferredWidth: Kirigami.Units.gridUnit * 26

    Kirigami.Theme.colorSet: Kirigami.Theme.Window
    Kirigami.Theme.inherit: false

    customFooterActions: [
        Kirigami.Action {
            text: i18n("Save")
            icon.name: "document-save"
            enabled: dialog.proxyRow >= 0 && (dialog.available || !enabledSwitch.checked)
            onTriggered: {
                proxyModel.setSandbox(dialog.proxyRow, enabledSwitch.checked, dialog.folders)
                dialog.saved(enabledSwitch.checked)
                dialog.close()
            }
        }
    ]

    function openFor(row, name) {
        proxyRow = row
        appName = name
        const info = proxyModel.sandbox(row)
        available = info.available
        settingsFolder = info.settingsFolder
        folders = info.folders
        enabledSwitch.checked = info.enabled
        open()
    }

    function tilde(path) {
        const home = Platform.StandardPaths.writableLocation(Platform.StandardPaths.HomeLocation).toString().replace("file://", "")
        return path.startsWith(home) ? "~" + path.slice(home.length) : path
    }

    onClosed: {
        proxyRow = -1
        appName = ""
        folders = []
    }

    ColumnLayout {
        spacing: Kirigami.Units.largeSpacing

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: !dialog.available
            type: Kirigami.MessageType.Warning
            text: i18n("The sandbox needs bubblewrap. Install the “bubblewrap” package to use it.")
        }

        Controls.Switch {
            id: enabledSwitch
            text: i18n("Run in a sandbox")
            enabled: dialog.available || checked
        }

        Controls.Label {
            Layout.fillWidth: true
            Layout.preferredWidth: 0
            visible: AppSettings.sandboxAll && dialog.available
            wrapMode: Text.WordWrap
            font: Kirigami.Theme.smallFont
            color: Kirigami.Theme.disabledTextColor
            text: i18n("All AppImages run in a sandbox (Settings → Security). Turn this off to leave %1 out.", dialog.appName)
        }

        Controls.Label {
            Layout.fillWidth: true
            Layout.preferredWidth: 0
            wrapMode: Text.WordWrap
            color: Kirigami.Theme.disabledTextColor
            text: i18n("%1 sees only its own folder (%2) and the folders below; the rest of your home folder and other drives are hidden. Display, sound and network keep working.",
                       dialog.appName, dialog.tilde(dialog.settingsFolder))
        }

        Kirigami.ListSectionHeader {
            Layout.fillWidth: true
            text: i18n("Folders it may use")
            leftPadding: 0
        }

        Repeater {
            model: dialog.folders
            delegate: RowLayout {
                required property string modelData
                required property int index
                Layout.fillWidth: true
                Kirigami.Icon {
                    source: "folder"
                    Layout.preferredWidth: Kirigami.Units.iconSizes.small
                    Layout.preferredHeight: Kirigami.Units.iconSizes.small
                }
                Controls.Label {
                    Layout.fillWidth: true
                    text: dialog.tilde(modelData)
                    elide: Text.ElideMiddle
                }
                Controls.ToolButton {
                    icon.name: "list-remove"
                    text: i18n("Remove")
                    display: Controls.AbstractButton.IconOnly
                    Controls.ToolTip.text: text
                    Controls.ToolTip.visible: hovered
                    Controls.ToolTip.delay: Kirigami.Units.toolTipDelay
                    onClicked: {
                        const copy = dialog.folders.slice()
                        copy.splice(index, 1)
                        dialog.folders = copy
                    }
                }
            }
        }

        Controls.Label {
            visible: dialog.folders.length === 0
            text: i18n("None")
            color: Kirigami.Theme.disabledTextColor
        }

        Controls.Label {
            Layout.fillWidth: true
            Layout.preferredWidth: 0
            visible: AppSettings.sandboxFolders.length > 0
            wrapMode: Text.WordWrap
            font: Kirigami.Theme.smallFont
            color: Kirigami.Theme.disabledTextColor
            text: i18np("Plus 1 folder every sandboxed app may use (Settings → Security).",
                        "Plus %1 folders every sandboxed app may use (Settings → Security).",
                        AppSettings.sandboxFolders.length)
        }

        Controls.Button {
            text: i18n("Add Folder…")
            icon.name: "folder-new"
            onClicked: folderPicker.open()
        }
    }

    FolderDialog {
        id: folderPicker
        title: i18n("Folder %1 May Use", dialog.appName)
        onAccepted: {
            const path = decodeURIComponent(selectedFolder.toString().replace("file://", ""))
            if (!dialog.folders.includes(path))
                dialog.folders = dialog.folders.concat([path])
        }
    }
}
