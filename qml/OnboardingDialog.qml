// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import QtQuick.Dialogs
import appimagemanager

Kirigami.Dialog {
    id: onboardingDialog

    title: i18n("Welcome to AppImage Manager")
    padding: Kirigami.Units.largeSpacing
    standardButtons: Kirigami.Dialog.NoButton

    property real parentWindowWidth: 0
    property int currentStep: 0

    preferredWidth: parentWindowWidth > 0
        ? Math.min(Kirigami.Units.gridUnit * 28, parentWindowWidth - Kirigami.Units.gridUnit * 2)
        : Kirigami.Units.gridUnit * 28
    preferredHeight: Kirigami.Units.gridUnit * 22

    closePolicy: Controls.Popup.NoAutoClose

    ColumnLayout {
        width: parent.width
        spacing: Kirigami.Units.largeSpacing

        // Progress Dots
        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: Kirigami.Units.smallSpacing

            Repeater {
                model: 4
                Rectangle {
                    width: Kirigami.Units.gridUnit * 0.5
                    height: width
                    radius: width / 2
                    color: index === onboardingDialog.currentStep ? Kirigami.Theme.highlightColor : Kirigami.Theme.disabledTextColor
                    Behavior on color { ColorAnimation { duration: 150 } }
                }
            }
        }

        // Step content
        StackLayout {
            id: stepStack
            Layout.fillWidth: true
            implicitHeight: Kirigami.Units.gridUnit * 12
            clip: true
            currentIndex: onboardingDialog.currentStep

            // Slide 1: Welcome & Overview
            ColumnLayout {
                spacing: Kirigami.Units.largeSpacing

                Kirigami.Heading {
                    text: i18n("AppImage Manager")
                    level: 1
                    Layout.alignment: Qt.AlignHCenter
                }

                Kirigami.Icon {
                    source: "appimagemanager"
                    fallback: "application-x-executable"
                    implicitWidth: Kirigami.Units.gridUnit * 4
                    implicitHeight: Kirigami.Units.gridUnit * 4
                    Layout.alignment: Qt.AlignHCenter
                }

                Controls.Label {
                    text: i18n("A native KDE utility to seamlessly install, update, and manage your AppImage applications. Keep your software organized and integrated directly with your desktop environment.")
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                    Layout.fillWidth: true
                    Layout.preferredWidth: 0
                }

                Item { Layout.fillHeight: true }
            }

            // Slide 2: Applications Folder Configuration
            ColumnLayout {
                spacing: Kirigami.Units.largeSpacing

                Kirigami.Heading {
                    text: i18n("Applications Folder")
                    level: 2
                    Layout.alignment: Qt.AlignHCenter
                }

                Controls.Label {
                    text: i18n("AppImages are stored and monitored in a centralized directory. Applications placed in this folder will automatically be integrated into your system menu.")
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                    Layout.fillWidth: true
                    Layout.preferredWidth: 0
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: Kirigami.Units.gridUnit * 4
                    color: Kirigami.Theme.alternateBackgroundColor
                    radius: Kirigami.Units.smallSpacing
                    border.color: Theme.cardBorderColor
                    border.width: 1

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: Kirigami.Units.mediumSpacing
                        spacing: Kirigami.Units.mediumSpacing

                        Kirigami.Icon {
                            source: "folder-open"
                            implicitWidth: Kirigami.Units.iconSizes.medium
                            implicitHeight: Kirigami.Units.iconSizes.medium
                            Layout.alignment: Qt.AlignVCenter
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            Controls.Label {
                                text: i18n("Applications Folder:")
                                font.bold: true
                            }
                            Controls.Label {
                                text: AppSettings.applicationsPath
                                elide: Text.ElideMiddle
                                font.family: "monospace"
                                font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                                Layout.fillWidth: true
                            }
                        }

                        Controls.Button {
                            icon.name: "folder-open"
                            onClicked: onboardingFolderDialog.open()
                            Controls.ToolTip.text: i18n("Browse…")
                            Controls.ToolTip.visible: hovered
                        }
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

                Item { Layout.fillHeight: true }
            }

            // Slide 3: Desktop Integrations
            ColumnLayout {
                spacing: Kirigami.Units.largeSpacing

                Kirigami.Heading {
                    text: i18n("Desktop Integration")
                    level: 2
                    Layout.alignment: Qt.AlignHCenter
                }

                RowLayout {
                    spacing: Kirigami.Units.largeSpacing
                    Layout.fillWidth: true

                    Kirigami.Icon {
                        source: "system-file-manager"
                        implicitWidth: Kirigami.Units.iconSizes.medium
                        implicitHeight: Kirigami.Units.iconSizes.medium
                        Layout.alignment: Qt.AlignTop
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Kirigami.Units.smallSpacing

                        Controls.Label {
                            text: i18n("Dolphin Integration")
                            font.bold: true
                        }
                        Controls.Label {
                            text: i18n("Right-click any AppImage file in Dolphin and select 'Manage AppImage' to inspect, sign-check, install, or uninstall it.")
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                            Layout.preferredWidth: 0
                            opacity: 0.8
                        }
                    }
                }

                RowLayout {
                    spacing: Kirigami.Units.largeSpacing
                    Layout.fillWidth: true

                    Kirigami.Icon {
                        source: "download"
                        implicitWidth: Kirigami.Units.iconSizes.medium
                        implicitHeight: Kirigami.Units.iconSizes.medium
                        Layout.alignment: Qt.AlignTop
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Kirigami.Units.smallSpacing

                        Controls.Label {
                            text: i18n("Downloads Folder Watcher")
                            font.bold: true
                        }
                        Controls.Label {
                            text: i18n("AppImage Manager can watch your Downloads folder and trigger system notifications to instantly integrate new AppImages.")
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                            Layout.preferredWidth: 0
                            opacity: 0.8
                        }
                    }
                }

                Controls.CheckBox {
                    id: watchDownloadsCheck
                    text: i18n("Monitor Downloads folder")
                    checked: AppSettings.watchDownloads
                    onToggled: AppSettings.watchDownloads = checked
                    Layout.alignment: Qt.AlignLeft
                    Layout.leftMargin: Kirigami.Units.gridUnit * 2.5
                }

                Item { Layout.fillHeight: true }
            }

            // Slide 4: Ready to Start
            ColumnLayout {
                spacing: Kirigami.Units.largeSpacing

                Kirigami.Heading {
                    text: i18n("You're All Set!")
                    level: 2
                    Layout.alignment: Qt.AlignHCenter
                }

                Kirigami.Icon {
                    source: "dialog-ok-apply"
                    implicitWidth: Kirigami.Units.gridUnit * 4
                    implicitHeight: Kirigami.Units.gridUnit * 4
                    Layout.alignment: Qt.AlignHCenter
                }

                Controls.Label {
                    text: i18n("You are ready to manage your AppImages. You can check for updates, browse store applications in 'Discover', or configure advanced options like updates frequency in the sidebar.")
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                    Layout.fillWidth: true
                    Layout.preferredWidth: 0
                }

                Item { Layout.fillHeight: true }
            }
        }

        // Navigation controls
        RowLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing

            Controls.Button {
                text: i18n("Skip Onboarding")
                icon.name: "dialog-cancel"
                visible: onboardingDialog.currentStep < 3
                onClicked: {
                    AppSettings.firstRun = false
                    onboardingDialog.close()
                }
            }

            Item { Layout.fillWidth: true }

            Controls.Button {
                text: i18n("Back")
                icon.name: "go-previous"
                visible: onboardingDialog.currentStep > 0
                onClicked: onboardingDialog.currentStep--
            }

            Controls.Button {
                text: onboardingDialog.currentStep === 3 ? i18n("Get Started") : i18n("Next")
                icon.name: onboardingDialog.currentStep === 3 ? "dialog-ok" : "go-next"
                highlighted: true
                onClicked: {
                    if (onboardingDialog.currentStep === 3) {
                        AppSettings.firstRun = false
                        onboardingDialog.close()
                    } else {
                        onboardingDialog.currentStep++
                    }
                }
            }
        }
    }

    FolderDialog {
        id: onboardingFolderDialog
        title: i18n("Select Applications Folder")
        currentFolder: "file://" + AppSettings.applicationsPath
        onAccepted: AppSettings.setApplicationsPathFromUrl(onboardingFolderDialog.selectedFolder)
    }
}
