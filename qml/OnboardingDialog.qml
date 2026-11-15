// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import QtQuick.Dialogs
import appimagemanager

// First-run setup on a single page: apps folder, Downloads watching, Discover.
Kirigami.Dialog {
    id: onboardingDialog

    title: i18n("Welcome to AppImage Manager")
    padding: Kirigami.Units.largeSpacing
    standardButtons: Kirigami.Dialog.NoButton
    showCloseButton: false
    closePolicy: Controls.Popup.NoAutoClose

    // QML file scope does not see the window's id, so the width is passed in.
    property real parentWindowWidth: 0

    preferredWidth: parentWindowWidth > 0
        ? Math.min(Kirigami.Units.gridUnit * 28, parentWindowWidth - Kirigami.Units.gridUnit * 2)
        : Kirigami.Units.gridUnit * 28
    // Fixed height: binding it to the window height causes a Kirigami Dialog binding loop.
    preferredHeight: Kirigami.Units.gridUnit * 22

    // Discover is opt-in for new installs: off until the user turns it on below.
    onOpened: if (AppSettings.firstRun) AppSettings.discoverEnabled = false
    onClosed: AppSettings.firstRun = false

    customFooterActions: [
        Kirigami.Action {
            icon.name: "dialog-ok"
            text: i18n("Get Started")
            onTriggered: onboardingDialog.close()
        }
    ]

    ColumnLayout {
        width: parent.width
        spacing: Kirigami.Units.largeSpacing

        Controls.Label {
            text: i18n("Install, update and remove AppImages. You can change these choices later in Settings.")
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
            // Wrapped labels report their unwrapped width; without this the dialog grows past preferredWidth.
            Layout.preferredWidth: 0
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing

            Controls.Label {
                text: i18n("Applications folder")
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing

                Controls.TextField {
                    Layout.fillWidth: true
                    text: AppSettings.applicationsPath
                    readOnly: true
                }

                Controls.Button {
                    icon.name: "folder-open"
                    text: i18n("Browse…")
                    display: Controls.AbstractButton.IconOnly
                    onClicked: onboardingFolderDialog.open()
                    Controls.ToolTip.text: text
                    Controls.ToolTip.visible: hovered
                    Controls.ToolTip.delay: Kirigami.Units.toolTipDelay
                }
            }

            Kirigami.InlineMessage {
                id: onboardingErrorMsg
                Layout.fillWidth: true
                type: Kirigami.MessageType.Error
                visible: false
                showCloseButton: true
            }

            Connections {
                target: AppSettings
                function onApplicationsPathError(badPath) {
                    onboardingErrorMsg.text = i18n("Cannot create folder: %1", badPath)
                    onboardingErrorMsg.visible = true
                }
                function onApplicationsPathChanged() {
                    onboardingErrorMsg.visible = false
                }
            }
        }

        Controls.CheckBox {
            Layout.fillWidth: true
            text: i18n("Notify when an AppImage is downloaded")
            checked: AppSettings.watchDownloads
            enabled: !listModel.downloadWatcherSandboxed
            onToggled: AppSettings.watchDownloads = checked
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing

            Controls.CheckBox {
                Layout.fillWidth: true
                text: i18n("Enable Discover to search online app catalogs")
                checked: AppSettings.discoverEnabled
                onToggled: AppSettings.discoverEnabled = checked
            }

            Repeater {
                model: amStoreModel.sources
                Controls.CheckBox {
                    required property var modelData
                    text: modelData.name
                    checked: modelData.enabled
                    enabled: AppSettings.discoverEnabled
                    Layout.leftMargin: Kirigami.Units.gridUnit * 1.5
                    onToggled: amStoreModel.updateSource(modelData.id, modelData.name,
                                                         modelData.url, modelData.type, checked)
                }
            }
        }

        Item { Layout.fillHeight: true }

        Controls.Label {
            text: i18n("Tip: right-click an AppImage in Dolphin and choose Manage AppImage.")
            wrapMode: Text.WordWrap
            color: Kirigami.Theme.disabledTextColor
            Layout.fillWidth: true
            Layout.preferredWidth: 0
        }
    }

    FolderDialog {
        id: onboardingFolderDialog
        title: i18n("Select Applications Folder")
        currentFolder: "file://" + AppSettings.applicationsPath
        onAccepted: AppSettings.setApplicationsPathFromUrl(onboardingFolderDialog.selectedFolder)
    }
}
