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

    // Provided by parent to open paths in the file manager
    signal openInFileManager(string path)

    title: backend ? i18n("Files — %1", backend.displayName) : ""
    padding: Kirigami.Units.largeSpacing
    standardButtons: Kirigami.Dialog.NoButton
    preferredWidth: Kirigami.Units.gridUnit * 30

    onClosed: { backend = null }

    ColumnLayout {
        spacing: Kirigami.Units.largeSpacing

        Controls.BusyIndicator {
            Layout.alignment: Qt.AlignHCenter
            running: !dialog.backend
                     || !dialog.backend.metadataLoaded
                     || dialog.backend.isFindingCorpses
            visible: running
        }

        ColumnLayout {
            spacing: 0
            visible: dialog.backend
                     && dialog.backend.metadataLoaded
                     && !dialog.backend.isFindingCorpses

            // AppImage file row
            Controls.ItemDelegate {
                Layout.fillWidth: true
                onClicked: dialog.openInFileManager(dialog.backend.filePath)

                contentItem: RowLayout {
                    spacing: Kirigami.Units.largeSpacing

                    Kirigami.Icon {
                        source: dialog.backend ? dialog.backend.appIconSource : ""
                        implicitWidth:  Kirigami.Units.iconSizes.medium
                        implicitHeight: Kirigami.Units.iconSizes.medium
                        Layout.alignment: Qt.AlignVCenter
                    }
                    ColumnLayout {
                        spacing: 2
                        Layout.fillWidth: true
                        Controls.Label {
                            text: dialog.backend ? dialog.backend.originalName : ""
                            font.bold: true
                            elide: Text.ElideMiddle
                            Layout.fillWidth: true
                        }
                        Controls.Label {
                            text: dialog.backend ? dialog.backend.formattedSize : ""
                            opacity: 0.7
                            font: Kirigami.Theme.smallFont
                        }
                    }
                    Kirigami.Icon {
                        source: "go-next"
                        implicitWidth:  Kirigami.Units.iconSizes.small
                        implicitHeight: Kirigami.Units.iconSizes.small
                        opacity: 0.5
                        Layout.alignment: Qt.AlignVCenter
                    }
                }
            }

            Kirigami.Separator {
                Layout.fillWidth: true
                visible: dialog.corpseModel && dialog.corpseModel.count > 0
            }

            // Related files / folders
            ListView {
                Layout.fillWidth: true
                implicitHeight: Math.min(contentHeight, Kirigami.Units.gridUnit * 12)
                clip: true
                model: dialog.corpseModel

                delegate: Controls.ItemDelegate {
                    required property string filePath
                    required property int fileSize
                    width: ListView.view.width

                    onClicked: dialog.openInFileManager(filePath)

                    contentItem: RowLayout {
                        spacing: Kirigami.Units.largeSpacing
                        ColumnLayout {
                            spacing: 2
                            Layout.fillWidth: true
                            Controls.Label {
                                text: filePath
                                elide: Text.ElideMiddle
                                Layout.fillWidth: true
                            }
                            Controls.Label {
                                text: dialog.backend ? dialog.backend.formatBytes(fileSize) : ""
                                opacity: 0.7
                                font: Kirigami.Theme.smallFont
                            }
                        }
                        Kirigami.Icon {
                            source: "go-next"
                            implicitWidth:  Kirigami.Units.iconSizes.small
                            implicitHeight: Kirigami.Units.iconSizes.small
                            opacity: 0.5
                            Layout.alignment: Qt.AlignVCenter
                        }
                    }
                }
            }

            // Footer
            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: Kirigami.Units.largeSpacing
                Item { Layout.fillWidth: true }
                Controls.Button {
                    text: i18n("Close")
                    onClicked: dialog.close()
                }
            }
        }
    }
}
