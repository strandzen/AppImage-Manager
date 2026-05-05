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
    width: Kirigami.Units.gridUnit * 50
    height: Kirigami.Units.gridUnit * 30
    minimumWidth: Kirigami.Units.gridUnit * 40
    minimumHeight: Kirigami.Units.gridUnit * 20

    Kirigami.Theme.colorSet: Kirigami.Theme.Window
    Kirigami.Theme.inherit: false

    // ── Column width constants ────────────────────────────────────────────────
    readonly property int colVersionWidth:  Kirigami.Units.gridUnit * 6
    readonly property int colSizeWidth:     Kirigami.Units.gridUnit * 6
    readonly property int colShortcutWidth: Kirigami.Units.gridUnit * 6
    readonly property int colRemoveWidth:   Kirigami.Units.iconSizes.medium + Kirigami.Units.smallSpacing * 2
    readonly property int delegateHPad:     Kirigami.Units.largeSpacing
    readonly property int nameColOffset:    delegateHPad
                                          + Kirigami.Units.iconSizes.medium
                                          + Kirigami.Units.largeSpacing

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

    UpdateDialog {
        id: updateDialog
        onDownloadRequested: proxyModel.downloadUpdate(updateDialog.proxyRow)
    }

    // ── Main page ─────────────────────────────────────────────────────────────
    Component {
        id: mainPage

        Kirigami.ScrollablePage {
            title: i18n("Installed AppImages")

            header: ColumnLayout {
                spacing: 0

                Kirigami.SearchField {
                    id: searchField
                    Layout.fillWidth: true
                    Layout.margins: Kirigami.Units.largeSpacing
                    placeholderText: i18n("Search installed apps…")
                    onTextChanged: proxyModel.filterText = text
                }

                Kirigami.Separator { Layout.fillWidth: true }

                // ── Column headers ────────────────────────────────────────────
                RowLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin:  root.nameColOffset
                    Layout.rightMargin: root.delegateHPad + root.colRemoveWidth + Kirigami.Units.largeSpacing
                    Layout.topMargin:    Kirigami.Units.smallSpacing
                    Layout.bottomMargin: Kirigami.Units.smallSpacing
                    spacing: Kirigami.Units.largeSpacing

                    Controls.ToolButton {
                        text: i18n("Name") + (proxyModel.sortRole === 0
                              ? (proxyModel.sortOrder === Qt.AscendingOrder ? " ▲" : " ▼") : "")
                        Layout.fillWidth: true
                        display: Controls.AbstractButton.TextOnly
                        font.pointSize: Kirigami.Theme.smallFont.pointSize
                        contentItem: Text {
                            text: parent.text
                            font.pointSize: parent.font.pointSize
                            font.bold: proxyModel.sortRole === 0
                            color: Kirigami.Theme.textColor
                            horizontalAlignment: Text.AlignLeft
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                        }
                        onClicked: {
                            if (proxyModel.sortRole === 0)
                                proxyModel.sortOrder = proxyModel.sortOrder === Qt.AscendingOrder
                                                       ? Qt.DescendingOrder : Qt.AscendingOrder
                            else
                                proxyModel.sortRole = 0
                        }
                    }

                    Controls.ToolButton {
                        text: i18n("Version") + (proxyModel.sortRole === 3
                              ? (proxyModel.sortOrder === Qt.AscendingOrder ? " ▲" : " ▼") : "")
                        Layout.preferredWidth: root.colVersionWidth
                        display: Controls.AbstractButton.TextOnly
                        font.pointSize: Kirigami.Theme.smallFont.pointSize
                        contentItem: Text {
                            text: parent.text
                            font.pointSize: parent.font.pointSize
                            font.bold: proxyModel.sortRole === 3
                            color: Kirigami.Theme.textColor
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                        }
                        onClicked: {
                            if (proxyModel.sortRole === 3)
                                proxyModel.sortOrder = proxyModel.sortOrder === Qt.AscendingOrder
                                                       ? Qt.DescendingOrder : Qt.AscendingOrder
                            else
                                proxyModel.sortRole = 3
                        }
                    }

                    Controls.ToolButton {
                        text: i18n("Size") + (proxyModel.sortRole === 1
                              ? (proxyModel.sortOrder === Qt.AscendingOrder ? " ▲" : " ▼") : "")
                        Layout.preferredWidth: root.colSizeWidth
                        display: Controls.AbstractButton.TextOnly
                        font.pointSize: Kirigami.Theme.smallFont.pointSize
                        contentItem: Text {
                            text: parent.text
                            font.pointSize: parent.font.pointSize
                            font.bold: proxyModel.sortRole === 1
                            color: Kirigami.Theme.textColor
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                        }
                        onClicked: {
                            if (proxyModel.sortRole === 1)
                                proxyModel.sortOrder = proxyModel.sortOrder === Qt.AscendingOrder
                                                       ? Qt.DescendingOrder : Qt.AscendingOrder
                            else
                                proxyModel.sortRole = 1
                        }
                    }

                    Controls.ToolButton {
                        text: i18n("Shortcut") + (proxyModel.sortRole === 4
                              ? (proxyModel.sortOrder === Qt.AscendingOrder ? " ▲" : " ▼") : "")
                        Layout.preferredWidth: root.colShortcutWidth
                        display: Controls.AbstractButton.TextOnly
                        font.pointSize: Kirigami.Theme.smallFont.pointSize
                        contentItem: Text {
                            text: parent.text
                            font.pointSize: parent.font.pointSize
                            font.bold: proxyModel.sortRole === 4
                            color: Kirigami.Theme.textColor
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                        }
                        onClicked: {
                            if (proxyModel.sortRole === 4)
                                proxyModel.sortOrder = proxyModel.sortOrder === Qt.AscendingOrder
                                                       ? Qt.DescendingOrder : Qt.AscendingOrder
                            else
                                proxyModel.sortRole = 4
                        }
                    }
                }

                // Scanning indicator — thin bar replaces separator while loading
                Kirigami.Separator {
                    Layout.fillWidth: true
                    visible: !listModel.scanning
                }
                Controls.ProgressBar {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 2
                    indeterminate: true
                    visible: listModel.scanning
                    padding: 0
                }
            }

            actions: [
                Kirigami.Action {
                    icon.name: "view-refresh"
                    text: i18n("Check for Updates")
                    displayHint: Kirigami.DisplayHint.IconOnly
                    onTriggered: listModel.checkForUpdates()
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
                icon.name: proxyModel.filterText !== "" ? "edit-find" : "application-x-executable"
                text: proxyModel.filterText !== ""
                      ? i18n("No results for \"%1\"", proxyModel.filterText)
                      : i18n("No AppImages installed")
                explanation: proxyModel.filterText !== ""
                             ? i18n("Try a different search term.")
                             : i18n("Right-click an AppImage file and choose \"Manage AppImage\" to install it.")
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
                    leftPadding: root.delegateHPad
                    rightPadding: root.delegateHPad

                    background: Rectangle {
                        color: removeButton.hovered
                               ? Kirigami.Theme.negativeBackgroundColor
                               : delegate.highlighted
                                 ? Kirigami.Theme.highlightColor
                                 : delegate.hovered
                                   ? Kirigami.Theme.hoverColor
                                   : index % 2 === 0
                                     ? Kirigami.Theme.backgroundColor
                                     : Kirigami.Theme.alternateBackgroundColor
                        Behavior on color { ColorAnimation { duration: Kirigami.Units.shortDuration } }
                    }

                    onDoubleClicked: {
                        if (model.metadataLoaded)
                            proxyModel.requestLaunch(index)
                    }

                    contentItem: RowLayout {
                        spacing: Kirigami.Units.largeSpacing

                        // Icon
                        Kirigami.Icon {
                            source: model.iconSource
                            implicitWidth:  Kirigami.Units.iconSizes.medium
                            implicitHeight: Kirigami.Units.iconSizes.medium
                            Layout.alignment: Qt.AlignVCenter
                        }

                        // Name + loading indicator
                        RowLayout {
                            Layout.fillWidth: true
                            Layout.alignment: Qt.AlignVCenter
                            spacing: Kirigami.Units.smallSpacing

                            Kirigami.Heading {
                                level: 4
                                text: model.displayName
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                                Layout.alignment: Qt.AlignVCenter
                            }

                            Controls.BusyIndicator {
                                implicitWidth:  Kirigami.Units.iconSizes.small
                                implicitHeight: Kirigami.Units.iconSizes.small
                                running: !model.metadataLoaded
                                visible: running
                            }
                        }

                        // Version column (chip xor progress bar)
                        Item {
                            Layout.preferredWidth: root.colVersionWidth
                            Layout.fillHeight: true
                            opacity: model.metadataLoaded ? 1.0 : 0.0
                            Behavior on opacity {
                                NumberAnimation { duration: Kirigami.Units.shortDuration }
                            }

                            Kirigami.Chip {
                                width: parent.width
                                anchors.verticalCenter: parent.verticalCenter
                                text: model.updateAvailable ? i18n("Available") : model.version
                                visible: (model.version !== "" || model.updateAvailable) && !model.isUpdating
                                closable: false
                                checkable: false
                                palette.button: model.updateAvailable ? Kirigami.Theme.positiveBackgroundColor : undefined
                                palette.buttonText: model.updateAvailable ? Kirigami.Theme.positiveTextColor : undefined
                                onClicked: {
                                    if (model.updateAvailable) {
                                        updateDialog.proxyRow      = index
                                        updateDialog.appName       = model.displayName
                                        updateDialog.currentVersion = model.version
                                        updateDialog.newVersion    = model.updateVersion
                                        updateDialog.open()
                                    }
                                }
                            }

                            Text {
                                anchors.centerIn: parent
                                text: "—"
                                color: Kirigami.Theme.textColor
                                opacity: 0.3
                                font.pointSize: Kirigami.Theme.smallFont.pointSize
                                visible: model.version === "" && !model.updateAvailable && !model.isUpdating
                            }

                            Controls.ProgressBar {
                                anchors.centerIn: parent
                                visible: model.isUpdating
                                value: model.updateProgress
                                from: 0
                                to: 100
                                width: parent.width
                                Controls.ToolTip.text: i18n("Downloading update... %1%", model.updateProgress)
                                Controls.ToolTip.visible: hovered
                            }
                        }

                        // Size column
                        Item {
                            Layout.preferredWidth: root.colSizeWidth
                            Layout.fillHeight: true
                            opacity: model.metadataLoaded ? 1.0 : 0.0
                            Behavior on opacity {
                                NumberAnimation { duration: Kirigami.Units.shortDuration }
                            }

                            Kirigami.Chip {
                                width: parent.width
                                anchors.verticalCenter: parent.verticalCenter
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
                        }

                        // Visible-in-Search column
                        Item {
                            Layout.preferredWidth: root.colShortcutWidth
                            Layout.fillHeight: true
                            opacity: model.metadataLoaded ? 1.0 : 0.0
                            Behavior on opacity {
                                NumberAnimation { duration: Kirigami.Units.shortDuration }
                            }

                            Kirigami.Chip {
                                width: parent.width
                                anchors.verticalCenter: parent.verticalCenter
                                text: model.hasDesktopLink ? i18n("Visible") : i18n("Hidden")
                                checkable: true
                                checked: model.hasDesktopLink
                                closable: false
                                onToggled: proxyModel.toggleDesktopLink(index, checked)
                            }
                        }

                        // Remove column
                        Item {
                            Layout.preferredWidth: root.colRemoveWidth
                            Layout.fillHeight: true
                            opacity: model.metadataLoaded ? 1.0 : 0.0
                            Behavior on opacity {
                                NumberAnimation { duration: Kirigami.Units.shortDuration }
                            }

                            Controls.Button {
                                id: removeButton
                                anchors.left: parent.left
                                anchors.verticalCenter: parent.verticalCenter
                                icon.name: "edit-delete"
                                display: Controls.AbstractButton.IconOnly
                                palette.highlight: Kirigami.Theme.negativeBackgroundColor
                                onClicked: proxyModel.requestRemoveAt(index)
                                Kirigami.Theme.inherit: false
                                Kirigami.Theme.colorSet: hovered ? Kirigami.Theme.Negative : Kirigami.Theme.Button
                                icon.color: hovered ? Kirigami.Theme.negativeTextColor
                                                    : Kirigami.Theme.textColor
                                Behavior on icon.color {
                                    ColorAnimation { duration: Kirigami.Units.shortDuration }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
