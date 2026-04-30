// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import appimagemanager

Kirigami.ApplicationWindow {
    id: root
    title: i18n("AppImage Manager")
    width: Kirigami.Units.gridUnit * 40
    height: Kirigami.Units.gridUnit * 30
    minimumWidth: Kirigami.Units.gridUnit * 30
    minimumHeight: Kirigami.Units.gridUnit * 20

    Kirigami.Theme.colorSet: Kirigami.Theme.Window
    Kirigami.Theme.inherit: false

    pageStack.initialPage: mainPage

    Connections {
        target: listModel
        function onOpenUninstallWindow(filePath) {
            uninstallDialog.backend = dashboardController.createUninstallBackend(filePath)
            uninstallDialog.open()
        }
    }

    // ── Dialogs ───────────────────────────────────────────────────────────────
    UninstallDialog {
        id: uninstallDialog
    }

    SettingsDialog {
        id: settingsDialog
    }

    StorageDialog {
        id: storageDialog
        onOpenInFileManager: (path) => dashboardController.openInFileManager(path)
    }

    // ── Main page ─────────────────────────────────────────────────────────────
    Component {
        id: mainPage

        Kirigami.ScrollablePage {
            title: i18n("Installed AppImages")

            header: Kirigami.SearchField {
                id: searchField
                Layout.fillWidth: true
                Layout.margins: Kirigami.Units.largeSpacing
                placeholderText: i18n("Search installed apps…")
                onTextChanged: proxyModel.filterText = text
            }

            actions: [
                Kirigami.Action {
                    icon.name: "view-sort-ascending"
                    text: i18n("Sort")
                    displayHint: Kirigami.DisplayHint.IconOnly

                    Kirigami.Action {
                        text: i18n("By Name")
                        checkable: true
                        checked: proxyModel.sortRole === 0
                        onTriggered: proxyModel.sortRole = 0
                    }
                    Kirigami.Action {
                        text: i18n("By Size")
                        checkable: true
                        checked: proxyModel.sortRole === 1
                        onTriggered: proxyModel.sortRole = 1
                    }
                    Kirigami.Action {
                        text: i18n("By Date Added")
                        checkable: true
                        checked: proxyModel.sortRole === 2
                        onTriggered: proxyModel.sortRole = 2
                    }
                },
                Kirigami.Action {
                    icon.name: "configure"
                    text: i18n("Settings")
                    displayHint: Kirigami.DisplayHint.IconOnly
                    onTriggered: settingsDialog.open()
                }
            ]

            Kirigami.PlaceholderMessage {
                anchors.centerIn: parent
                visible: proxyModel.count === 0 && !listModel.scanning
                icon.name: "application-x-executable"
                text: i18n("No AppImages installed")
                explanation: i18n("Right-click an AppImage file and choose \"Manage AppImage\" to install it.")
            }

            Controls.BusyIndicator {
                anchors.centerIn: parent
                running: listModel.scanning && proxyModel.count === 0
                visible: running
            }

            ListView {
                id: listView
                model: proxyModel
                spacing: 0
                clip: true

                add: Transition {
                    NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Kirigami.Units.longDuration }
                }
                remove: Transition {
                    NumberAnimation { property: "opacity"; to: 0; duration: Kirigami.Units.shortDuration }
                }
                displaced: Transition {
                    NumberAnimation { properties: "y"; duration: Kirigami.Units.longDuration; easing.type: Easing.OutCubic }
                }

                footer: Kirigami.InlineMessage {
                    width: listView.width
                    visible: AppSettings.showDisclaimer && proxyModel.count > 0
                    type: Kirigami.MessageType.Warning
                    text: i18n("AppImages are unverified executables. Only install from sources you trust.")
                    showCloseButton: false
                }

                delegate: Controls.ItemDelegate {
                    id: delegate
                    width: listView.width
                    highlighted: listView.currentIndex === index

                    background: Rectangle {
                        color: delegate.highlighted
                               ? Kirigami.Theme.highlightColor
                               : index % 2 === 0
                                 ? Kirigami.Theme.backgroundColor
                                 : Kirigami.Theme.alternateBackgroundColor
                    }

                    onDoubleClicked: {
                        if (model.metadataLoaded)
                            proxyModel.requestLaunch(index)
                    }

                    contentItem: RowLayout {
                        spacing: Kirigami.Units.largeSpacing

                        Kirigami.Icon {
                            source: model.iconSource
                            implicitWidth:  Kirigami.Units.iconSizes.medium
                            implicitHeight: Kirigami.Units.iconSizes.medium
                            Layout.alignment: Qt.AlignVCenter
                        }

                        // Name
                        Kirigami.Heading {
                            level: 4
                            text: model.displayName
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                            Layout.alignment: Qt.AlignVCenter
                        }

                        // Actions and Info
                        RowLayout {
                            spacing: Kirigami.Units.smallSpacing
                            visible: model.metadataLoaded

                            Kirigami.Chip {
                                text: model.version
                                visible: model.version !== ""
                                closable: false
                                checkable: false
                                onClicked: {}
                            }

                            Kirigami.Chip {
                                text: model.formattedSize
                                visible: model.appSize > 0
                                closable: false
                                checkable: false
                                onClicked: {
                                    storageDialog.backend =
                                        dashboardController.createStorageBackend(model.filePath)
                                    storageDialog.open()
                                }
                            }

                            Kirigami.Chip {
                                text: i18n("Shortcut")
                                checkable: true
                                checked: model.hasDesktopLink
                                closable: false
                                onToggled: proxyModel.toggleDesktopLink(index, checked)
                            }

                            Controls.Button {
                                icon.name: "edit-delete"
                                display: Controls.AbstractButton.IconOnly
                                onClicked: proxyModel.requestRemoveAt(index)
                                Controls.ToolTip.text: i18n("Remove")
                                Controls.ToolTip.visible: hovered
                                icon.color: hovered ? Kirigami.Theme.negativeTextColor
                                                    : Kirigami.Theme.textColor
                                Behavior on icon.color {
                                    ColorAnimation { duration: Kirigami.Units.shortDuration }
                                }
                            }
                        }

                        Controls.BusyIndicator {
                            implicitWidth:  Kirigami.Units.iconSizes.small
                            implicitHeight: Kirigami.Units.iconSizes.small
                            running: !model.metadataLoaded
                            visible: running
                        }
                    }
                }
            }
        }
    }
}
