// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import appimagemanager

Kirigami.Dialog {
    id: dialog
    title: i18n("Settings")
    padding: Kirigami.Units.largeSpacing
    
    preferredWidth: Kirigami.Units.gridUnit * 24
    preferredHeight: Kirigami.Units.gridUnit * 28

    readonly property color cardBorderColor: AppSettings.accentBorders 
        ? dialog.Kirigami.Theme.focusColor 
        : (dialog.Kirigami.Theme.textColor && dialog.Kirigami.Theme.textColor.r !== undefined
            ? Qt.rgba(dialog.Kirigami.Theme.textColor.r, dialog.Kirigami.Theme.textColor.g, dialog.Kirigami.Theme.textColor.b, 0.15)
            : Qt.rgba(0.5, 0.5, 0.5, 0.15))

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

    Controls.ScrollView {
        id: scrollView
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        Controls.ScrollBar.horizontal.policy: Controls.ScrollBar.AlwaysOff
        Controls.ScrollBar.vertical.policy: Controls.ScrollBar.AsNeeded

        // Expose scrollbar width so the ColumnLayout can account for overlay scrollbars.
        // For non-overlay styles, availableWidth already excludes the scrollbar;
        // Math.min ensures we never allocate more than either measure allows.
        readonly property real vBarWidth: Controls.ScrollBar.vertical.visible
            ? Controls.ScrollBar.vertical.width : 0

        ColumnLayout {
            spacing: Kirigami.Units.largeSpacing
            width: scrollView.width - scrollView.vBarWidth - Kirigami.Units.largeSpacing
            x: Kirigami.Units.largeSpacing / 2

            Kirigami.InlineMessage {
                id: errorMsg
                Layout.fillWidth: true
                type: Kirigami.MessageType.Error
                visible: false
                showCloseButton: true
            }

            // ── Card 1: Storage & Location ─────────────────────────────────────
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: storageLayout.implicitHeight + Kirigami.Units.largeSpacing * 2
                color: Kirigami.Theme.alternateBackgroundColor
                border.color: dialog.cardBorderColor
                border.width: 1
                radius: Kirigami.Units.smallSpacing * 2

                ColumnLayout {
                    id: storageLayout
                    x: Kirigami.Units.largeSpacing
                    y: Kirigami.Units.largeSpacing
                    width: parent.width - Kirigami.Units.largeSpacing * 2
                    spacing: Kirigami.Units.mediumSpacing

                    Kirigami.Heading {
                        text: i18n("Storage & Location")
                        level: 3
                        Layout.fillWidth: true
                    }

                    Controls.Label {
                        text: i18n("Applications folder:")
                        font.bold: true
                        Layout.fillWidth: true
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Kirigami.Units.smallSpacing

                        Controls.TextField {
                            id: pathField
                            Layout.fillWidth: true
                            text: AppSettings.applicationsPath
                            onEditingFinished: AppSettings.applicationsPath = text
                        }

                        Controls.Button {
                            icon.name: "folder-open"
                            onClicked: AppSettings.openFolderPicker(dialog.Window.window)
                            Controls.ToolTip.text: i18n("Browse…")
                            Controls.ToolTip.visible: hovered
                        }

                        Controls.Button {
                            icon.name: "system-file-manager"
                            onClicked: Qt.openUrlExternally("file://" + AppSettings.applicationsPath)
                            Controls.ToolTip.text: i18n("Open Folder in Dolphin")
                            Controls.ToolTip.visible: hovered
                        }
                    }
                }
            }

            // ── Card 2: Appearance ─────────────────────────────────────────────
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: appearanceLayout.implicitHeight + Kirigami.Units.largeSpacing * 2
                color: Kirigami.Theme.alternateBackgroundColor
                border.color: dialog.cardBorderColor
                border.width: 1
                radius: Kirigami.Units.smallSpacing * 2

                ColumnLayout {
                    id: appearanceLayout
                    x: Kirigami.Units.largeSpacing
                    y: Kirigami.Units.largeSpacing
                    width: parent.width - Kirigami.Units.largeSpacing * 2
                    spacing: Kirigami.Units.mediumSpacing

                    Kirigami.Heading {
                        text: i18n("Appearance")
                        level: 3
                        Layout.fillWidth: true
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Kirigami.Units.largeSpacing

                        Controls.Label {
                            text: i18n("Manage window icon size:")
                            Layout.alignment: Qt.AlignVCenter
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        Controls.ComboBox {
                            Layout.preferredWidth: Kirigami.Units.gridUnit * 6
                            Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
                            model: [i18n("Small"), i18n("Medium"), i18n("Large")]
                            currentIndex: AppSettings.manageIconSize
                            onActivated: AppSettings.manageIconSize = currentIndex
                        }
                    }

                    Kirigami.Separator { Layout.fillWidth: true }

                    ColumnLayout {
                        spacing: 0
                        Layout.fillWidth: true

                        Controls.CheckBox {
                            id: showInstallBoxCheck
                            Layout.fillWidth: true
                            text: i18n("Show drag-and-drop install zone at bottom of list")
                            checked: AppSettings.showInstallBox
                            onToggled: AppSettings.showInstallBox = showInstallBoxCheck.checked
                        }

                        Controls.Label {
                            text: i18n("Displays a persistent installation target at the footer of the dashboard application list.")
                            opacity: 0.6
                            font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                            Layout.leftMargin: Kirigami.Units.gridUnit * 1.5
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }
                    }

                    Kirigami.Separator { Layout.fillWidth: true }

                    ColumnLayout {
                        spacing: 0
                        Layout.fillWidth: true

                        Controls.CheckBox {
                            id: accentBordersCheck
                            Layout.fillWidth: true
                            text: i18n("Show accent borders on cards and drop zones")
                            checked: AppSettings.accentBorders
                            onToggled: AppSettings.accentBorders = accentBordersCheck.checked
                        }

                        Controls.Label {
                            text: i18n("Outlines visual frames and drop targets with active highlight colors instead of quiet gray outlines.")
                            opacity: 0.6
                            font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                            Layout.leftMargin: Kirigami.Units.gridUnit * 1.5
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }
                    }
                }
            }

            // ── Card 3: Behavior & Security ────────────────────────────────────
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: behaviorLayout.implicitHeight + Kirigami.Units.largeSpacing * 2
                color: Kirigami.Theme.alternateBackgroundColor
                border.color: dialog.cardBorderColor
                border.width: 1
                radius: Kirigami.Units.smallSpacing * 2

                ColumnLayout {
                    id: behaviorLayout
                    x: Kirigami.Units.largeSpacing
                    y: Kirigami.Units.largeSpacing
                    width: parent.width - Kirigami.Units.largeSpacing * 2
                    spacing: Kirigami.Units.mediumSpacing

                    Kirigami.Heading {
                        text: i18n("Behavior & Security")
                        level: 3
                        Layout.fillWidth: true
                    }

                    ColumnLayout {
                        spacing: 0
                        Layout.fillWidth: true

                        Controls.CheckBox {
                            id: showDisclaimerCheck
                            Layout.fillWidth: true
                            text: i18n("Show security disclaimer")
                            checked: AppSettings.showDisclaimer
                            onToggled: AppSettings.showDisclaimer = showDisclaimerCheck.checked
                        }

                        Controls.Label {
                            text: i18n("Displays an alert warning about unverified executable files when installing new AppImages.")
                            opacity: 0.6
                            font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                            Layout.leftMargin: Kirigami.Units.gridUnit * 1.5
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }
                    }

                    Kirigami.Separator { Layout.fillWidth: true }

                    ColumnLayout {
                        spacing: 0
                        Layout.fillWidth: true

                        Controls.CheckBox {
                            id: showNotificationsCheck
                            Layout.fillWidth: true
                            text: i18n("Show install/uninstall notifications")
                            checked: AppSettings.showNotifications
                            onToggled: AppSettings.showNotifications = showNotificationsCheck.checked
                        }

                        Controls.Label {
                            text: i18n("Fires native system popups when installation or removal actions complete successfully.")
                            opacity: 0.6
                            font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                            Layout.leftMargin: Kirigami.Units.gridUnit * 1.5
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }
                    }

                    Kirigami.Separator { Layout.fillWidth: true }

                    ColumnLayout {
                        spacing: 0
                        Layout.fillWidth: true

                        Controls.CheckBox {
                            id: watchDownloadsCheck
                            Layout.fillWidth: true
                            text: i18n("Notify when an AppImage is downloaded")
                            checked: AppSettings.watchDownloads
                            onToggled: AppSettings.watchDownloads = watchDownloadsCheck.checked
                        }

                        Controls.Label {
                            text: i18n("Monitors your Downloads folder and triggers a native KDE notification with quick installation actions.")
                            opacity: 0.6
                            font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                            Layout.leftMargin: Kirigami.Units.gridUnit * 1.5
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }
                    }
                }
            }

            // ── Card 4: Background Updates ─────────────────────────────────────
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: updatesLayout.implicitHeight + Kirigami.Units.largeSpacing * 2
                color: Kirigami.Theme.alternateBackgroundColor
                border.color: dialog.cardBorderColor
                border.width: 1
                radius: Kirigami.Units.smallSpacing * 2

                ColumnLayout {
                    id: updatesLayout
                    x: Kirigami.Units.largeSpacing
                    y: Kirigami.Units.largeSpacing
                    width: parent.width - Kirigami.Units.largeSpacing * 2
                    spacing: Kirigami.Units.mediumSpacing

                    Kirigami.Heading {
                        text: i18n("Background Updates")
                        level: 3
                        Layout.fillWidth: true
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Kirigami.Units.largeSpacing

                        Controls.Label {
                            text: i18n("Check for updates:")
                            Layout.alignment: Qt.AlignVCenter
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        Controls.ComboBox {
                            Layout.preferredWidth: Kirigami.Units.gridUnit * 6
                            Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
                            model: [i18n("Never"), i18n("Daily"), i18n("Weekly"), i18n("Monthly"), i18n("Custom")]
                            currentIndex: AppSettings.updateFrequency
                            onActivated: AppSettings.updateFrequency = currentIndex
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Kirigami.Units.largeSpacing
                        visible: AppSettings.updateFrequency === 4

                        Controls.Label {
                            text: i18n("Custom interval (days):")
                            Layout.alignment: Qt.AlignVCenter
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        Controls.SpinBox {
                            Layout.preferredWidth: Kirigami.Units.gridUnit * 6
                            Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
                            from: 1
                            to: 365
                            value: AppSettings.customUpdateDays
                            onValueModified: AppSettings.customUpdateDays = value
                        }
                    }

                    Controls.Label {
                        text: i18n("Configures the frequency of background update scans for all registered AppImages on this machine.")
                        opacity: 0.6
                        font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                }
            }
        }
    }

    footer: Item {
        implicitHeight: footerLayout.implicitHeight + Kirigami.Units.largeSpacing
        width: parent.width

        RowLayout {
            id: footerLayout
            anchors {
                left: parent.left
                right: parent.right
                bottom: parent.bottom
                leftMargin: Kirigami.Units.largeSpacing
                rightMargin: Kirigami.Units.largeSpacing
                bottomMargin: Kirigami.Units.largeSpacing
            }

            Controls.Button {
                text: i18n("Restore Defaults")
                icon.name: "document-revert"
                onClicked: {
                    AppSettings.resetToDefaults();
                    pathField.text = AppSettings.applicationsPath
                }
            }

            Item {
                Layout.fillWidth: true
            }

            Controls.Button {
                text: i18n("Close")
                icon.name: "window-close"
                onClicked: dialog.close()
            }
        }
    }
}
