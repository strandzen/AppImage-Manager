// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import org.kde.kirigami as Kirigami

ApplicationWindow {
    id: uninstallWindow
    width: 500
    height: 400
    title: i18n("Uninstall %1", backend.cleanName)
    flags: Qt.Window | Qt.WindowStaysOnTopHint
    modality: Qt.ApplicationModal

    Kirigami.Theme.colorSet: Kirigami.Theme.Window
    Kirigami.Theme.inherit: false

    // Track whether the AppImage itself is checked for removal
    property bool appImageChecked: true

    // ── Confirmation dialog ───────────────────────────────────────────────────
    Kirigami.PromptDialog {
        id: confirmDialog
        title: i18n("Move to Trash?")
        subtitle: i18n("The selected items will be moved to the Trash. You can restore them from there if needed.")
        standardButtons: Kirigami.Dialog.Ok | Kirigami.Dialog.Cancel
        onAccepted: {
            var pathsToDelete = backend.corpseModel.checkedPaths()
            if (uninstallWindow.appImageChecked)
                pathsToDelete.push("APPIMAGE_ITSELF")
            backend.removeAppImageAndCorpses(pathsToDelete)
            uninstallWindow.close()
        }
    }

    Component.onCompleted: updateTotalSize()

    Connections {
        target: backend
        function onBusyChanged() { updateTotalSize() }
    }

    Connections {
        target: backend.corpseModel
        function onModelReset() { updateTotalSize() }
        function onDataChanged() { updateTotalSize() }
    }

    function updateTotalSize() {
        var total = backend.corpseModel.checkedSize()
        if (appImageChecked)
            total += backend.appSize
        totalSizeLabel.text = i18n("Selected for removal: %1", backend.formatBytes(total))
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 15
        spacing: 10

        BusyIndicator {
            anchors.centerIn: parent
            running: backend.isFindingCorpses || backend.isRemovingItems
            visible: running
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: !backend.isFindingCorpses && !backend.isRemovingItems

            Label {
                text: i18n("Select items to remove:")
                font.bold: true
                color: Kirigami.Theme.textColor
            }

            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true

                ListView {
                    id: listView
                    spacing: 5

                    // The AppImage itself as the first (fixed) item
                    header: RowLayout {
                        width: listView.width

                        CheckBox {
                            checked: uninstallWindow.appImageChecked
                            onCheckedChanged: {
                                uninstallWindow.appImageChecked = checked
                                updateTotalSize()
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            Label {
                                text: backend.originalName + i18n(" (AppImage)")
                                Layout.fillWidth: true
                                elide: Label.ElideMiddle
                                font.pixelSize: 12
                                color: Kirigami.Theme.textColor
                            }
                            Label {
                                text: backend.formatBytes(backend.appSize)
                                font.pixelSize: 10
                                opacity: 0.7
                                color: Kirigami.Theme.textColor
                            }
                        }
                    }

                    // Corpse files from the native C++ model
                    model: backend.corpseModel

                    delegate: RowLayout {
                        required property string filePath
                        required property int fileSize
                        required property bool isChecked
                        required property int index

                        width: listView.width

                        CheckBox {
                            checked: isChecked
                            onCheckedChanged: {
                                backend.corpseModel.setData(
                                    backend.corpseModel.index(index, 0),
                                    checked,
                                    backend.corpseModel.checkedRole)
                                updateTotalSize()
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            Label {
                                text: filePath
                                Layout.fillWidth: true
                                elide: Label.ElideMiddle
                                font.pixelSize: 12
                                color: Kirigami.Theme.textColor
                            }
                            Label {
                                text: backend.formatBytes(fileSize)
                                font.pixelSize: 10
                                opacity: 0.7
                                color: Kirigami.Theme.textColor
                            }
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true

                Label {
                    id: totalSizeLabel
                    Layout.fillWidth: true
                    font.bold: true
                    color: Kirigami.Theme.textColor
                }

                Button {
                    text: i18n("Cancel")
                    onClicked: uninstallWindow.close()
                }

                Button {
                    text: i18n("Remove")
                    icon.name: "edit-delete"
                    palette.button: Kirigami.Theme.negativeTextColor
                    palette.buttonText: Kirigami.Theme.backgroundColor
                    onClicked: confirmDialog.open()
                }
            }
        }
    }
}
