// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import org.kde.kirigami as Kirigami
import appimagemanager

Kirigami.Dialog {
    id: dialog
    title: i18n("Settings")
    padding: Kirigami.Units.largeSpacing
    standardButtons: Kirigami.Dialog.NoButton

    Connections {
        target: AppSettings
        function onApplicationsPathError(badPath) {
            errorMsg.text = i18n("Cannot create folder: %1", badPath)
            errorMsg.visible = true
            pathField.text = AppSettings.applicationsPath
        }
        function onApplicationsPathChanged() {
            errorMsg.visible = false
        }
    }

    FolderDialog {
        id: folderDialog
        onAccepted: {
            var path = selectedFolder.toString()
            if (path.startsWith("file://"))
                path = path.substring(7)
            AppSettings.applicationsPath = path
        }
    }

    ColumnLayout {
        spacing: Kirigami.Units.largeSpacing

        Kirigami.InlineMessage {
            id: errorMsg
            Layout.fillWidth: true
            type: Kirigami.MessageType.Error
            visible: false
            showCloseButton: true
        }

        Kirigami.FormLayout {
            RowLayout {
                Kirigami.FormData.label: i18n("Applications folder:")
                spacing: Kirigami.Units.smallSpacing
                Controls.TextField {
                    id: pathField
                    implicitWidth: Kirigami.Units.gridUnit * 18
                    text: AppSettings.applicationsPath
                    onEditingFinished: AppSettings.applicationsPath = text
                }
                Controls.Button {
                    icon.name: "folder-open"
                    onClicked: folderDialog.open()
                    Controls.ToolTip.text: i18n("Browse…")
                    Controls.ToolTip.visible: hovered
                }
            }
            Controls.CheckBox {
                text: i18n("Show security disclaimer")
                checked: AppSettings.showDisclaimer
                onToggled: AppSettings.showDisclaimer = checked
            }
            Controls.CheckBox {
                text: i18n("Show install/uninstall notifications")
                checked: AppSettings.showNotifications
                onToggled: AppSettings.showNotifications = checked
            }
        }
    }
}
