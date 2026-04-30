// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Kirigami.Dialog {
    id: dialog

    property var backend: null
    property var corpseModel: backend ? backend.corpseModel : null

    title: backend ? i18n("Uninstall %1", backend.cleanName) : ""
    padding: Kirigami.Units.largeSpacing
    standardButtons: Kirigami.Dialog.NoButton
    preferredWidth: Kirigami.Units.gridUnit * 28

    // Internal state
    property bool _appChecked: true
    property string _totalText: ""

    function _updateTotal() {
        if (!backend) { _totalText = ""; return }
        var total = backend.corpseModel.checkedSize()
        if (_appChecked) total += backend.appSize
        _totalText = i18n("Selected for removal: %1", backend.formatBytes(total))
    }

    onBackendChanged: {
        _appChecked = true
        _totalText = ""
        if (backend && backend.metadataLoaded)
            backend.findCorpses()
    }

    onClosed: {
        backend = null
        _appChecked = true
        _totalText = ""
    }

    Connections {
        target: dialog.backend
        function onBusyChanged() { dialog._updateTotal() }
    }

    Connections {
        target: dialog.corpseModel
        function onModelReset() { dialog._updateTotal() }
        function onDataChanged() { dialog._updateTotal() }
    }

    Kirigami.PromptDialog {
        id: confirmRemoveDialog
        title: i18n("Move to Trash?")
        subtitle: i18n("The selected items will be moved to the Trash. You can restore them from there if needed.")
        standardButtons: Kirigami.Dialog.Ok | Kirigami.Dialog.Cancel
        onAccepted: {
            if (!dialog.backend) return
            dialog.backend.removeAppImageAndCorpses(
                dialog.backend.corpseModel.checkedPaths(),
                dialog._appChecked)
            dialog.close()
        }
    }

    ColumnLayout {
        spacing: Kirigami.Units.largeSpacing

        Controls.BusyIndicator {
            Layout.alignment: Qt.AlignHCenter
            running: !dialog.backend
                     || !dialog.backend.metadataLoaded
                     || dialog.backend.isFindingCorpses
                     || dialog.backend.isRemovingItems
            visible: running
        }

        ColumnLayout {
            spacing: Kirigami.Units.smallSpacing
            visible: dialog.backend
                     && dialog.backend.metadataLoaded
                     && !dialog.backend.isFindingCorpses
                     && !dialog.backend.isRemovingItems

            Controls.Label {
                text: i18n("Select items to remove:")
                font.bold: true
            }

            // AppImage itself
            RowLayout {
                spacing: Kirigami.Units.smallSpacing
                Controls.CheckBox {
                    checked: dialog._appChecked
                    onCheckedChanged: {
                        dialog._appChecked = checked
                        dialog._updateTotal()
                    }
                }
                ColumnLayout {
                    spacing: 2
                    Controls.Label {
                        text: dialog.backend
                              ? dialog.backend.originalName + i18n(" (AppImage)") : ""
                        elide: Text.ElideMiddle
                        Layout.fillWidth: true
                    }
                    Controls.Label {
                        text: dialog.backend
                              ? dialog.backend.formatBytes(dialog.backend.appSize) : ""
                        opacity: 0.7
                        font: Kirigami.Theme.smallFont
                    }
                }
            }

            // Corpse files
            ListView {
                Layout.fillWidth: true
                implicitHeight: Math.min(contentHeight, Kirigami.Units.gridUnit * 10)
                clip: true
                model: dialog.corpseModel

                delegate: RowLayout {
                    required property string filePath
                    required property int fileSize
                    required property bool isChecked
                    required property int index
                    width: ListView.view.width
                    spacing: Kirigami.Units.smallSpacing

                    Controls.CheckBox {
                        checked: isChecked
                        onCheckedChanged: {
                            dialog.backend.corpseModel.setData(
                                dialog.backend.corpseModel.index(index, 0),
                                checked,
                                dialog.backend.corpseModel.checkedRole)
                            dialog._updateTotal()
                        }
                    }
                    ColumnLayout {
                        spacing: 2
                        Layout.fillWidth: true
                        Controls.Label {
                            text: filePath
                            elide: Text.ElideMiddle
                            Layout.fillWidth: true
                        }
                        Controls.Label {
                            text: dialog.backend
                                  ? dialog.backend.formatBytes(fileSize) : ""
                            opacity: 0.7
                            font: Kirigami.Theme.smallFont
                        }
                    }
                }
            }

            // Footer: total + buttons
            RowLayout {
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing

                Controls.Label {
                    text: dialog._totalText
                    font.bold: true
                    Layout.fillWidth: true
                }
                Controls.Button {
                    text: i18n("Cancel")
                    onClicked: dialog.close()
                }
                Controls.Button {
                    text: i18n("Remove")
                    icon.name: "edit-delete"
                    palette.button: Kirigami.Theme.negativeTextColor
                    palette.buttonText: Kirigami.Theme.backgroundColor
                    onClicked: confirmRemoveDialog.open()
                }
            }
        }
    }
}
