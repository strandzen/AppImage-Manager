// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import org.kde.kirigami as Kirigami

ApplicationWindow {
    id: uninstallWindow
    width: Kirigami.Units.gridUnit * 28
    height: Kirigami.Units.gridUnit * 24
    minimumWidth: Kirigami.Units.gridUnit * 24
    minimumHeight: Kirigami.Units.gridUnit * 20
    title: i18n("Uninstall %1", backend.displayName)
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
        anchors.margins: Kirigami.Units.largeSpacing
        spacing: Kirigami.Units.largeSpacing

        BusyIndicator {
            Layout.alignment: Qt.AlignHCenter
            running: !backend.metadataLoaded || backend.isFindingCorpses || backend.isRemovingItems
            visible: running
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: backend.metadataLoaded && !backend.isFindingCorpses && !backend.isRemovingItems
            spacing: Kirigami.Units.mediumSpacing

            Kirigami.Heading {
                text: i18n("Select items to remove")
                level: 3
            }

            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true

                ListView {
                    id: listView
                    spacing: 0

                    // The AppImage itself as the first (fixed) item
                    header: ItemDelegate {
                        width: listView.width
                        onClicked: {
                            uninstallWindow.appImageChecked = !uninstallWindow.appImageChecked
                            updateTotalSize()
                        }

                        contentItem: RowLayout {
                            spacing: Kirigami.Units.largeSpacing
                            CheckBox {
                                checked: uninstallWindow.appImageChecked
                                onClicked: { /* Handled by delegate onClicked */ }
                            }

                            ColumnLayout {
                                spacing: 0
                                Layout.fillWidth: true
                                Label {
                                    text: backend.originalName
                                    font.bold: true
                                    elide: Label.ElideMiddle
                                    Layout.fillWidth: true
                                }
                                Label {
                                    text: i18n("Main AppImage File • %1", backend.formattedSize)
                                    font.pixelSize: Kirigami.Units.fonts.small.pixelSize
                                    opacity: 0.6
                                }
                            }
                        }
                    }

                    model: backend.corpseModel

                    delegate: ItemDelegate {
                        required property string filePath
                        required property int fileSize
                        required property bool isChecked
                        required property int index

                        width: listView.width
                        onClicked: {
                            backend.corpseModel.setData(
                                backend.corpseModel.index(index, 0),
                                !isChecked,
                                backend.corpseModel.checkedRole)
                            updateTotalSize()
                        }

                        contentItem: RowLayout {
                            spacing: Kirigami.Units.largeSpacing
                            CheckBox {
                                checked: isChecked
                                onClicked: { /* Handled by delegate onClicked */ }
                            }

                            ColumnLayout {
                                spacing: 0
                                Layout.fillWidth: true
                                Label {
                                    text: filePath
                                    elide: Label.ElideMiddle
                                    Layout.fillWidth: true
                                }
                                Label {
                                    text: backend.formatBytes(fileSize)
                                    font.pixelSize: Kirigami.Units.fonts.small.pixelSize
                                    opacity: 0.6
                                }
                            }
                        }
                    }
                }
            }

            Kirigami.Separator { Layout.fillWidth: true }

            RowLayout {
                Layout.fillWidth: true
                spacing: Kirigami.Units.largeSpacing

                Label {
                    id: totalSizeLabel
                    Layout.fillWidth: true
                    font.bold: true
                }

                Button {
                    text: i18n("Cancel")
                    onClicked: uninstallWindow.close()
                }

                Button {
                    text: i18n("Remove")
                    icon.name: "edit-delete"
                    highlighted: true
                    onClicked: confirmDialog.open()
                }
            }
        }
    }
}
