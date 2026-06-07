// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard
import QtQuick.Dialogs
import appimagemanager

Kirigami.Dialog {
    id: dialog
    title: i18n("Settings")
    padding: 0
    
    preferredWidth: Kirigami.Units.gridUnit * 28
    preferredHeight: Kirigami.Units.gridUnit * 34

    Connections {
        target: AppSettings
        function onApplicationsPathError(badPath) {
            errorMsg.text = i18n("Cannot create folder: %1", badPath)
            errorMsg.visible = true
            pathField.text = AppSettings.applicationsPath
        }
        function onApplicationsPathChanged() {
            errorMsg.visible = false
            pathField.text = AppSettings.applicationsPath
        }
    }

    Controls.ScrollView {
        id: scrollView
        implicitHeight: Kirigami.Units.gridUnit * 34
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        Controls.ScrollBar.horizontal.policy: Controls.ScrollBar.AlwaysOff
        Controls.ScrollBar.vertical.policy: Controls.ScrollBar.AsNeeded

        ColumnLayout {
            width: scrollView.availableWidth - Kirigami.Units.largeSpacing * 2
            x: Kirigami.Units.largeSpacing
            spacing: 0

            Kirigami.InlineMessage {
                id: errorMsg
                Layout.fillWidth: true
                Layout.margins: Kirigami.Units.largeSpacing
                type: Kirigami.MessageType.Error
                visible: false
                showCloseButton: true
            }

            // ── Card 1: Storage & Location ─────────────────────────────────────
            FormCard.FormHeader {
                title: i18n("Storage & Location")
            }

            FormCard.FormCard {
                FormCard.AbstractFormDelegate {
                    background: null
                    contentItem: RowLayout {
                        spacing: Kirigami.Units.smallSpacing
                        Controls.Label {
                            text: i18n("Applications folder:")
                        }
                        Controls.TextField {
                            id: pathField
                            Layout.fillWidth: true
                            text: AppSettings.applicationsPath
                            onEditingFinished: AppSettings.applicationsPath = text
                        }
                        Controls.Button {
                            icon.name: "folder-open"
                            onClicked: folderDialog.open()
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
            FormCard.FormHeader {
                title: i18n("Appearance")
            }

            FormCard.FormCard {
                FormCard.FormComboBoxDelegate {
                    text: i18n("Manage window icon size")
                    model: [i18n("Small"), i18n("Medium"), i18n("Large")]
                    currentIndex: AppSettings.manageIconSize
                    onActivated: AppSettings.manageIconSize = currentIndex
                }

                FormCard.FormDelegateSeparator {}

                FormCard.FormSwitchDelegate {
                    id: showInstallBoxCheck
                    text: i18n("Show drag-and-drop install zone at bottom of list")
                    description: i18n("Displays a persistent installation target at the footer of the dashboard application list.")
                    checked: AppSettings.showInstallBox
                    onToggled: AppSettings.showInstallBox = checked
                }

                FormCard.FormDelegateSeparator {}

                FormCard.FormSwitchDelegate {
                    id: accentBordersCheck
                    text: i18n("Show accent borders on cards and drop zones")
                    description: i18n("Outlines visual frames and drop targets with active highlight colors instead of quiet gray outlines.")
                    checked: AppSettings.accentBorders
                    onToggled: AppSettings.accentBorders = checked
                }
            }

            // ── Card 3: Behavior & Security ────────────────────────────────────
            FormCard.FormHeader {
                title: i18n("Behavior & Security")
            }

            FormCard.FormCard {
                FormCard.FormSwitchDelegate {
                    id: verifySignaturesCheck
                    text: i18n("Verify GPG signatures before installing")
                    description: i18n("Checks each AppImage for an embedded GPG signature. Warns on unsigned files; blocks installation of files with invalid signatures unless overridden.")
                    checked: AppSettings.verifySignatures
                    onToggled: AppSettings.verifySignatures = checked
                }

                FormCard.FormDelegateSeparator {}

                FormCard.FormSwitchDelegate {
                    id: showDisclaimerCheck
                    text: i18n("Show security disclaimer")
                    description: i18n("Displays an alert warning about unverified executable files when installing new AppImages.")
                    checked: AppSettings.showDisclaimer
                    onToggled: AppSettings.showDisclaimer = checked
                }

                FormCard.FormDelegateSeparator {}

                FormCard.FormSwitchDelegate {
                    id: showNotificationsCheck
                    text: i18n("Show install/uninstall notifications")
                    description: i18n("Fires native system popups when installation or removal actions complete successfully.")
                    checked: AppSettings.showNotifications
                    onToggled: AppSettings.showNotifications = checked
                }

                FormCard.FormDelegateSeparator {}

                FormCard.FormSwitchDelegate {
                    id: watchDownloadsCheck
                    text: i18n("Notify when an AppImage is downloaded")
                    description: i18n("Monitors your Downloads folder and triggers a native KDE notification with quick installation actions.")
                    checked: AppSettings.watchDownloads
                    onToggled: AppSettings.watchDownloads = checked
                }
                
                FormCard.AbstractFormDelegate {
                    background: null
                    visible: listModel.downloadWatcherSandboxed
                    contentItem: Kirigami.InlineMessage {
                        Layout.fillWidth: true
                        type: Kirigami.MessageType.Information
                        text: i18n("Download monitoring is unavailable because AppImage Manager is running in a sandbox.")
                        showCloseButton: false
                    }
                }
            }

            // ── Card 4: Background Updates ─────────────────────────────────────
            FormCard.FormHeader {
                title: i18n("Background Updates")
            }

            FormCard.FormCard {
                FormCard.FormComboBoxDelegate {
                    text: i18n("Check for updates")
                    description: i18n("Configures the frequency of background update scans for all registered AppImages on this machine.")
                    model: [i18n("Never"), i18n("Daily"), i18n("Weekly"), i18n("Monthly"), i18n("Custom")]
                    currentIndex: AppSettings.updateFrequency
                    onActivated: AppSettings.updateFrequency = currentIndex
                }

                FormCard.FormDelegateSeparator {
                    visible: AppSettings.updateFrequency === 4
                }

                FormCard.FormSpinBoxDelegate {
                    label: i18n("Custom interval (days)")
                    visible: AppSettings.updateFrequency === 4
                    from: 1
                    to: 365
                    value: AppSettings.customUpdateDays
                    onValueChanged: AppSettings.customUpdateDays = value
                }
            }
            
            Item {
                // Bottom padding
                height: Kirigami.Units.largeSpacing
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
                onClicked: restoreDefaultsDialog.open()
            }

            Kirigami.PromptDialog {
                id: restoreDefaultsDialog
                title: i18n("Restore Defaults?")
                subtitle: i18n("All settings will be reset to their default values. This cannot be undone.")
                standardButtons: Kirigami.Dialog.Ok | Kirigami.Dialog.Cancel
                Kirigami.Theme.colorSet: Kirigami.Theme.Window
                Kirigami.Theme.inherit: false
                onAccepted: {
                    AppSettings.resetToDefaults()
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

    FolderDialog {
        id: folderDialog
        title: i18n("Select Applications Folder")
        currentFolder: "file://" + AppSettings.applicationsPath
        onAccepted: AppSettings.setApplicationsPathFromUrl(folderDialog.selectedFolder)
    }
}
