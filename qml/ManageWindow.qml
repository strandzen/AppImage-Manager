// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import org.kde.kirigami as Kirigami

ApplicationWindow {
    id: root
    width: 500
    height: 420
    minimumWidth: 500
    maximumWidth: 500
    minimumHeight: 420
    maximumHeight: 420
    visible: true
    title: i18n("Manage AppImage")

    Kirigami.Theme.colorSet: Kirigami.Theme.Window
    Kirigami.Theme.inherit: false

    // ── About sheet ──────────────────────────────────────────────────────────
    Kirigami.OverlaySheet {
        id: aboutSheet

        header: Kirigami.Heading { text: i18n("About AppImage Manager") }

        ColumnLayout {
            spacing: Kirigami.Units.largeSpacing
            width: 360

            Image {
                source: "image://icon/application-x-executable"
                width: 64; height: 64
                Layout.alignment: Qt.AlignHCenter
            }

            Kirigami.Heading {
                text: i18n("AppImage Manager")
                level: 2
                Layout.alignment: Qt.AlignHCenter
            }

            Label {
                text: i18n("Version 0.1.0")
                opacity: 0.7
                Layout.alignment: Qt.AlignHCenter
            }

            Label {
                text: i18n("A lightweight AppImage manager for KDE Plasma 6.")
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
            }

            Label {
                text: i18n("© 2024 AppImage Manager Contributors")
                opacity: 0.7
                Layout.alignment: Qt.AlignHCenter
            }

            Label {
                text: "<a href=\"https://github.com/strandzen/AppImage-Manager\">github.com/strandzen/AppImage-Manager</a>"
                onLinkActivated: Qt.openUrlExternally(link)
                Layout.alignment: Qt.AlignHCenter
            }

            Label {
                text: i18n("License: GNU General Public License v2 or later")
                opacity: 0.6
                font.pixelSize: 11
                Layout.alignment: Qt.AlignHCenter
            }
        }
    }

    // ── About button (top-right corner) ──────────────────────────────────────
    ToolButton {
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: 4
        icon.name: "help-about"
        flat: true
        onClicked: aboutSheet.open()
        ToolTip.text: i18n("About")
        ToolTip.visible: hovered
    }

    BusyIndicator {
        anchors.centerIn: parent
        running: !backend.metadataLoaded
        visible: !backend.metadataLoaded
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 10
        visible: backend.metadataLoaded

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            Label {
                text: backend.cleanName
                font.pixelSize: 22
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                Layout.fillWidth: true
                elide: Label.ElideRight

                ToolTip.visible: infoMouseArea.containsMouse
                ToolTip.text: backend.originalName

                MouseArea {
                    id: infoMouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                }
            }

            Label {
                text: i18n("Version: %1  •  Size: %2",
                            backend.appVersion,
                            backend.formatBytes(backend.appSize))
                color: Kirigami.Theme.textColor
                opacity: 0.7
                horizontalAlignment: Text.AlignHCenter
                Layout.fillWidth: true
            }
        }

        Item { Layout.fillHeight: true }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: innerRow.implicitHeight + 40
            implicitHeight: innerRow.implicitHeight + 60
            color: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.04)
            radius: Kirigami.Units.smallSpacing * 2
            border.color: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.1)
            border.width: 1

            RowLayout {
                id: innerRow
                anchors.centerIn: parent
                spacing: backend.isInstalled ? 0 : 80

                ColumnLayout {
                    spacing: 8
                    Item {
                        id: iconContainer
                        width: 96
                        height: 96
                        Layout.alignment: Qt.AlignHCenter

                        Image {
                            id: appIcon
                            width: 96
                            height: 96
                            source: backend.appIconSource !== "" ? backend.appIconSource : "image://icon/application-x-executable"
                            fillMode: Image.PreserveAspectFit

                            Drag.active: dragArea.drag.active
                            Drag.hotSpot.x: width / 2
                            Drag.hotSpot.y: height / 2
                            Drag.keys: ["appimage"]

                            states: [
                                State {
                                    when: appIcon.Drag.active
                                    ParentChange {
                                        target: appIcon
                                        parent: root.contentItem
                                    }
                                }
                            ]
                        }

                        MouseArea {
                            id: dragArea
                            anchors.fill: parent
                            hoverEnabled: backend.isInstalled
                            drag.target: backend.isInstalled ? null : appIcon
                            cursorShape: backend.isInstalled ? Qt.PointingHandCursor : Qt.OpenHandCursor

                            onClicked: {
                                if (backend.isInstalled)
                                    backend.launchAppImage()
                            }

                            onReleased: {
                                if (backend.isInstalled) return
                                var dropPos = appIcon.mapToItem(folderContainer, appIcon.width / 2, appIcon.height / 2)
                                if (dropPos.x >= 0 && dropPos.x <= folderContainer.width
                                        && dropPos.y >= 0 && dropPos.y <= folderContainer.height) {
                                    backend.installAppImage()
                                }
                                appIcon.parent = iconContainer
                                appIcon.x = 0
                                appIcon.y = 0
                            }

                            ToolTip.visible: backend.isInstalled && containsMouse
                            ToolTip.text: i18n("Launch %1", backend.cleanName)
                        }
                    }

                    Label {
                        text: backend.cleanName.replace('.AppImage', '')
                        horizontalAlignment: Text.AlignHCenter
                        Layout.alignment: Qt.AlignHCenter
                        elide: Label.ElideRight
                        Layout.maximumWidth: 140
                    }
                }

                ColumnLayout {
                    spacing: 8
                    visible: !backend.isInstalled

                    Item {
                        id: folderContainer
                        width: 96
                        height: 96
                        Layout.alignment: Qt.AlignHCenter

                        DropArea {
                            id: folderDropArea
                            anchors.fill: parent
                            keys: ["appimage"]
                        }

                        Image {
                            source: "image://icon/system-file-manager"
                            anchors.fill: parent
                            opacity: folderDropArea.containsDrag ? 0.8 : 1.0
                            scale: folderDropArea.containsDrag ? 1.10 : 1.0

                            Behavior on scale   { NumberAnimation { duration: 150 } }
                            Behavior on opacity { NumberAnimation { duration: 150 } }
                        }
                    }

                    Label {
                        text: i18n("Applications")
                        horizontalAlignment: Text.AlignHCenter
                        Layout.alignment: Qt.AlignHCenter
                        font.bold: dragArea.drag.active
                    }
                }
            }
        }

        Item { Layout.fillHeight: true }

        // Security disclaimer — visible only before the AppImage is installed.
        // Disappears once the user drags it to ~/Applications.
        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: !backend.isInstalled
            type: Kirigami.MessageType.Warning
            text: i18n("AppImages are unverified executables. Only install from sources you trust.")
            showCloseButton: false
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignHCenter
            spacing: Kirigami.Units.largeSpacing
            visible: backend.isInstalled

            CheckBox {
                text: i18n("Create Shortcut")
                Layout.alignment: Qt.AlignHCenter
                checked: backend.hasDesktopLink
                onCheckedChanged: {
                    if (checked !== backend.hasDesktopLink)
                        backend.toggleDesktopLink(checked)
                }
            }

            Button {
                text: i18n("Remove")
                icon.name: "edit-delete"
                Layout.alignment: Qt.AlignHCenter
                onClicked: {
                    backend.findCorpses()
                    var component = Qt.createComponent("qrc:/org/kde/appimagemanager/UninstallWindow.qml")
                    if (component.status === Component.Ready) {
                        var win = component.createObject(root)
                        win.show()
                    } else {
                        console.error(component.errorString())
                    }
                }
            }
        }
    }

    Connections {
        target: backend
        function onUninstallFinished() { root.close() }
    }
}
