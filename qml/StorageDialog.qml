// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import Qt.labs.platform as Platform
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

        ColumnLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: Kirigami.Units.smallSpacing
            visible: !dialog.backend
                     || !dialog.backend.metadataLoaded
                     || dialog.backend.isFindingCorpses

            Controls.BusyIndicator { running: parent.visible; Layout.alignment: Qt.AlignHCenter }

            Controls.Label {
                text: i18n("Loading file information…")
                opacity: 0.7
                font: Kirigami.Theme.smallFont
                Layout.alignment: Qt.AlignHCenter
            }
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
                visible: dialog.corpseModel !== null
            }

            Controls.Label {
                text: i18n("Related Files")
                font.bold: true
                Layout.topMargin: Kirigami.Units.smallSpacing
                visible: dialog.corpseModel !== null
            }

            Controls.Label {
                text: i18n("No related files found.")
                opacity: 0.6
                visible: dialog.corpseModel !== null && dialog.corpseModel.count === 0
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
                                readonly property string home: Platform.StandardPaths.writableLocation(Platform.StandardPaths.HomeLocation)
                                text: filePath.startsWith(home) ? "~" + filePath.slice(home.length) : filePath
                                elide: Text.ElideMiddle
                                Layout.fillWidth: true
                                Controls.ToolTip.text: filePath
                                Controls.ToolTip.visible: hovered
                                Controls.ToolTip.delay: Kirigami.Units.toolTipDelay
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

                Controls.Label {
                    text: dialog.backend ? i18n("Total: %1", dialog.backend.formattedSize) : ""
                    font.bold: true
                    Layout.fillWidth: true
                }
                Controls.Button {
                    text: i18n("Close")
                    onClicked: dialog.close()
                }
            }
        }
    }
}
