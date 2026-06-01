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

    title: backend ? i18n("Uninstall %1", backend.cleanName) : ""
    padding: Kirigami.Units.largeSpacing
    standardButtons: Kirigami.Dialog.NoButton
    preferredWidth: Kirigami.Units.gridUnit * 28

    // Internal state
    property bool _appChecked: true
    property int  _corpseRevision: 0   // bumped by Connections to invalidate _totalText binding

    readonly property string _totalText: {
        _corpseRevision   // dependency: re-evaluate when corpse data changes
        if (!backend || !backend.metadataLoaded) return ""
        const corpseBytes = corpseModel ? corpseModel.checkedSize() : 0
        const total = corpseBytes + (_appChecked ? backend.appSize : 0)
        return i18n("Selected for removal: %1", backend.formatBytes(total))
    }

    onBackendChanged: {
        _appChecked = true
        _corpseRevision = 0
        if (backend && backend.metadataLoaded)
            backend.findCorpses()
    }

    onClosed: {
        backend = null
        _appChecked = true
    }

    Connections {
        target: dialog.backend
        function onBusyChanged() { dialog._corpseRevision++ }
    }

    Connections {
        target: dialog.corpseModel
        function onModelReset() { dialog._corpseRevision++ }
        function onDataChanged() { dialog._corpseRevision++ }
    }

    Kirigami.PromptDialog {
        id: confirmRemoveDialog
        title: i18n("Move to Trash?")
        subtitle: {
            const corpseCount = dialog.corpseModel ? dialog.corpseModel.checkedCount() : 0
            const total = (dialog._appChecked ? 1 : 0) + corpseCount
            return i18ncp("@info",
                "1 item will be moved to Trash. You can restore it if needed.",
                "%1 items will be moved to Trash. You can restore them if needed.", total)
        }
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

        ColumnLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: Kirigami.Units.smallSpacing
            visible: !dialog.backend
                     || !dialog.backend.metadataLoaded
                     || dialog.backend.isFindingCorpses
                     || dialog.backend.isRemovingItems

            Controls.BusyIndicator { running: parent.visible; Layout.alignment: Qt.AlignHCenter }

            Controls.Label {
                text: dialog.backend && dialog.backend.isRemovingItems
                      ? i18n("Moving to Trash…")
                      : i18n("Scanning for leftover data…")
                opacity: 0.7
                font: Kirigami.Theme.smallFont
                Layout.alignment: Qt.AlignHCenter
            }
        }

        ColumnLayout {
            spacing: Kirigami.Units.smallSpacing
            visible: dialog.backend
                     && dialog.backend.metadataLoaded
                     && !dialog.backend.isFindingCorpses
                     && !dialog.backend.isRemovingItems

            RowLayout {
                spacing: Kirigami.Units.smallSpacing

                Controls.Label {
                    text: i18n("Select items to remove:")
                    font.bold: true
                    Layout.fillWidth: true
                }

                Controls.ToolButton {
                    icon.name: "dialog-information"
                    flat: true
                    visible: dialog.corpseModel && dialog.corpseModel.rowCount() > 0
                    Controls.ToolTip.text: i18n("Leftover directories are sorted by match confidence — high-confidence entries are pre-selected. Only remove directories you are sure belong to this app.")
                    Controls.ToolTip.visible: hovered
                    Controls.ToolTip.delay: Kirigami.Units.toolTipDelay
                }

                Controls.Button {
                    text: i18n("All")
                    visible: dialog.corpseModel && dialog.corpseModel.rowCount() > 0
                    onClicked: dialog.corpseModel.setAllChecked(true)
                }
                Controls.Button {
                    text: i18n("None")
                    visible: dialog.corpseModel && dialog.corpseModel.rowCount() > 0
                    onClicked: dialog.corpseModel.setAllChecked(false)
                }
            }

            // AppImage itself
            Rectangle {
                Layout.fillWidth: true
                color: Kirigami.Theme.alternateBackgroundColor
                radius: Kirigami.Units.smallSpacing
                implicitHeight: appItemRow.implicitHeight + Kirigami.Units.smallSpacing * 2

                RowLayout {
                    id: appItemRow
                    anchors { fill: parent; margins: Kirigami.Units.smallSpacing }
                    spacing: Kirigami.Units.smallSpacing

                    Controls.CheckBox {
                        checked: dialog._appChecked
                        onCheckedChanged: {
                            dialog._appChecked = checked
                        }
                    }
                    ColumnLayout {
                        spacing: 2
                        Controls.Label {
                            text: dialog.backend ? dialog.backend.originalName : ""
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
            }

            // Corpse files — split into High-confidence (pre-selected) and
            // Low-confidence (unchecked, shown under a warning disclosure).
            // The model guarantees High items are sorted before Low items.
            ListView {
                id: corpseList
                Layout.fillWidth: true
                implicitHeight: Math.min(contentHeight, Kirigami.Units.gridUnit * 12)
                clip: true
                model: dialog.corpseModel

                // Section header: blank for "high", warning banner for "low".
                section.property: "confidence"
                section.criteria: ViewSection.FullString
                section.delegate: Item {
                    required property string section
                    width: corpseList.width
                    height: section === "low" ? lowConfidenceHeader.implicitHeight + Kirigami.Units.smallSpacing * 2 : 0
                    visible: section === "low"

                    Kirigami.InlineMessage {
                        id: lowConfidenceHeader
                        anchors {
                            left: parent.left; right: parent.right
                            verticalCenter: parent.verticalCenter
                        }
                        type: Kirigami.MessageType.Warning
                        text: i18n("Possibly related — these directories may not belong to this app. Verify before removing.")
                        showCloseButton: false
                    }
                }

                delegate: RowLayout {
                    required property string filePath
                    required property int fileSize
                    required property bool isChecked
                    required property string confidence
                    required property int index
                    width: ListView.view.width
                    spacing: Kirigami.Units.smallSpacing
                    opacity: confidence === "low" ? 0.75 : 1.0

                    Controls.CheckBox {
                        checked: isChecked
                        onCheckedChanged: {
                            dialog.backend.corpseModel.setData(
                                dialog.backend.corpseModel.index(index, 0),
                                checked,
                                dialog.backend.corpseModel.checkedRole)
                        }
                    }
                    ColumnLayout {
                        spacing: 2
                        Layout.fillWidth: true
                        Controls.Label {
                            readonly property string home: Platform.StandardPaths.writableLocation(Platform.StandardPaths.HomeLocation)
                            text: filePath.startsWith(home) ? "~" + filePath.slice(home.length) : filePath
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                            HoverHandler { id: tooltipHover }
                            Controls.ToolTip.text: filePath
                            Controls.ToolTip.visible: tooltipHover.hovered
                            Controls.ToolTip.delay: Kirigami.Units.toolTipDelay
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
                    text: i18n("Move to Trash")
                    icon.name: "trash-empty"
                    palette.button: Kirigami.Theme.negativeTextColor
                    palette.buttonText: Kirigami.Theme.backgroundColor
                    onClicked: confirmRemoveDialog.open()
                }
            }
        }
    }
}
