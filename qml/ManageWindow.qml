// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import org.kde.kirigami as Kirigami
import appimagemanager

ApplicationWindow {
    id: root
    visible: true
    title: i18n("AppImage Manager")

    width:  Kirigami.Units.gridUnit * 26
    height: Kirigami.Units.gridUnit * 22
    minimumWidth:  width;  maximumWidth:  width
    minimumHeight: height; maximumHeight: height

    flags: Qt.Window | Qt.WindowMinimizeButtonHint | Qt.WindowCloseButtonHint

    Kirigami.Theme.colorSet: Kirigami.Theme.Window
    Kirigami.Theme.inherit: false

    readonly property int managedIconSize: [
        Kirigami.Units.iconSizes.huge,
        Math.round(Kirigami.Units.iconSizes.huge * 1.5),
        Kirigami.Units.iconSizes.enormous
    ][AppSettings.manageIconSize]

    readonly property color cardBorderColor: AppSettings.accentBorders 
        ? root.Kirigami.Theme.focusColor 
        : (root.Kirigami.Theme.textColor && root.Kirigami.Theme.textColor.r !== undefined
            ? Qt.rgba(root.Kirigami.Theme.textColor.r, root.Kirigami.Theme.textColor.g, root.Kirigami.Theme.textColor.b, 0.15)
            : Qt.rgba(0.5, 0.5, 0.5, 0.15))

    UninstallDialog { id: uninstallDialog }

    // ── About dialog ─────────────────────────────────────────────────────────
    AboutDialog { id: aboutDialog }

    BusyIndicator {
        anchors.centerIn: parent
        running: !backend.metadataLoaded
        visible: !backend.metadataLoaded
    }

    ColumnLayout {
        id: mainLayout
        anchors.fill: parent
        anchors.margins: Kirigami.Units.largeSpacing * 2
        spacing: Kirigami.Units.largeSpacing
        visible: backend.metadataLoaded

        // ── Heading row ───────────────────────────────────────────────────────
        RowLayout {
            Layout.fillWidth: true
            spacing: 0

            Item { Layout.fillWidth: true }

            ColumnLayout {
                spacing: Kirigami.Units.smallSpacing
                Layout.alignment: Qt.AlignHCenter

                Kirigami.Heading {
                    text: backend.displayName
                    level: 2; horizontalAlignment: Text.AlignHCenter; Layout.fillWidth: true; elide: Text.ElideRight
                    ToolTip.visible: infoMouseArea.containsMouse; ToolTip.text: backend.originalName
                    MouseArea { id: infoMouseArea; anchors.fill: parent; hoverEnabled: true }
                }

                Flow {
                    Layout.alignment: Qt.AlignHCenter
                    spacing: Kirigami.Units.smallSpacing
                    Kirigami.Chip {
                        text: i18n("Version: %1", backend.appVersion || i18n("Unknown"))
                        closable: false; checkable: false
                    }
                    Kirigami.Chip {
                        text: i18n("Size: %1", backend.formattedSize)
                        closable: false; checkable: false
                    }
                }
            }

            Item { Layout.fillWidth: true }

            ToolButton {
                icon.name: "help-about"
                onClicked: aboutDialog.open()
                ToolTip.text: i18n("About"); ToolTip.visible: hovered
                Layout.alignment: Qt.AlignTop
            }
        }

        // ── Icon drag-and-drop box ────────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: Kirigami.Theme.alternateBackgroundColor
            radius: Kirigami.Units.smallSpacing * 2
            border.color: root.cardBorderColor
            border.width: folderDropArea.containsDrag ? 2 : 1
            opacity: folderDropArea.containsDrag ? 1.0 : 0.8

            Behavior on border.width { NumberAnimation { duration: Kirigami.Units.shortDuration } }
            Behavior on opacity { NumberAnimation { duration: Kirigami.Units.shortDuration } }

            RowLayout {
                id: innerRow
                anchors.centerIn: parent
                spacing: Kirigami.Units.gridUnit * 4

                ColumnLayout {
                    Layout.alignment: Qt.AlignVCenter
                    spacing: Kirigami.Units.smallSpacing

                    Item {
                        id: iconContainer
                        width: root.managedIconSize; height: root.managedIconSize
                        Layout.alignment: Qt.AlignHCenter
                        Kirigami.Icon {
                            id: appIcon
                            width: iconContainer.width; height: iconContainer.height
                            source: backend.appIconSource !== "" ? backend.appIconSource : "application-x-executable"
                            Drag.active: dragArea.drag.active
                            Drag.hotSpot.x: width / 2; Drag.hotSpot.y: height / 2; Drag.keys: ["appimage"]
                        }
                        MouseArea {
                            id: dragArea; anchors.fill: parent
                            hoverEnabled: backend.isInstalled
                            drag.target: backend.isInstalled ? null : appIcon
                            cursorShape: backend.isInstalled ? Qt.PointingHandCursor : Qt.OpenHandCursor
                            onClicked: if (backend.isInstalled) backend.launchAppImage()
                            onPressed: {
                                if (backend.isInstalled) return
                                snapBackX.stop(); snapBackY.stop()
                                var pos = appIcon.mapToItem(root.contentItem, 0, 0)
                                appIcon.parent = root.contentItem
                                appIcon.x = pos.x; appIcon.y = pos.y
                            }
                            onReleased: {
                                if (backend.isInstalled) return
                                const dropPos = appIcon.mapToItem(folderContainer, appIcon.width / 2, appIcon.height / 2)
                                const hit = dropPos.x >= 0 && dropPos.x <= folderContainer.width
                                         && dropPos.y >= 0 && dropPos.y <= folderContainer.height
                                if (hit) {
                                    backend.installAppImage()
                                    appIcon.parent = iconContainer; appIcon.x = 0; appIcon.y = 0
                                } else {
                                    var pos = appIcon.mapToItem(iconContainer, 0, 0)
                                    appIcon.parent = iconContainer
                                    appIcon.x = pos.x; appIcon.y = pos.y
                                    snapBackX.to = 0; snapBackY.to = 0
                                    snapBackX.restart(); snapBackY.restart()
                                }
                            }

                            NumberAnimation { id: snapBackX; target: appIcon; property: "x"; duration: Kirigami.Units.longDuration; easing.type: Easing.OutCubic }
                            NumberAnimation { id: snapBackY; target: appIcon; property: "y"; duration: Kirigami.Units.longDuration; easing.type: Easing.OutCubic }
                            ToolTip.visible: backend.isInstalled && containsMouse
                            ToolTip.text: i18n("Launch %1", backend.displayName)
                        }
                    }
                    Label {
                        text: backend.displayName; horizontalAlignment: Text.AlignHCenter
                        Layout.alignment: Qt.AlignHCenter; elide: Label.ElideRight
                        Layout.maximumWidth: Kirigami.Units.gridUnit * 10; font.bold: backend.isInstalled
                    }
                    Label {
                        text: i18n("Drag icon to install")
                        visible: !backend.isInstalled
                        opacity: 0.5
                        font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                        Layout.alignment: Qt.AlignHCenter
                    }
                }

                ColumnLayout {
                    Layout.alignment: Qt.AlignVCenter
                    spacing: Kirigami.Units.smallSpacing

                    Item {
                        id: folderContainer
                        width: root.managedIconSize; height: root.managedIconSize
                        Layout.alignment: Qt.AlignHCenter

                        DropArea {
                            id: folderDropArea
                            anchors.fill: parent
                            keys: ["appimage"]
                            enabled: !backend.isInstalled
                        }

                        Kirigami.Icon {
                            source: "system-file-manager"; anchors.fill: parent
                            scale: folderDropArea.containsDrag ? 1.2 : 1.0
                            Behavior on scale { NumberAnimation { duration: Kirigami.Units.shortDuration; easing.type: Easing.OutBack } }
                        }

                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: backend.openDashboard()
                            ToolTip.text: i18n("Open Dashboard")
                            ToolTip.visible: containsMouse && !folderDropArea.containsDrag
                        }
                    }

                    Label {
                        text: i18n("Applications")
                        horizontalAlignment: Text.AlignHCenter
                        Layout.alignment: Qt.AlignHCenter
                        font.bold: dragArea.drag.active && !backend.isInstalled
                    }
                }
            }
        }

        // ── Disclaimer (not installed) ────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            radius: Kirigami.Units.smallSpacing * 2
            clip: true
            color: "transparent"
            visible: !backend.isInstalled && AppSettings.showDisclaimer
            implicitHeight: disclaimerMsg.implicitHeight

            Kirigami.InlineMessage {
                id: disclaimerMsg
                width: parent.width
                visible: true
                type: Kirigami.MessageType.Warning
                text: i18n("AppImages are unverified executables. Only install from sources you trust.")
                showCloseButton: false
            }
        }

        // ── Remove (installed) ────────────────────────────────────────────────
        Button {
            Layout.alignment: Qt.AlignHCenter
            visible: backend.isInstalled
            text: i18n("Remove"); icon.name: "edit-delete"
            onClicked: {
                uninstallDialog.backend = backend
                uninstallDialog.open()
            }
        }
    }

    Connections {
        target: backend
        function onUninstallFinished() { root.close() }
        function onErrorOccurred(message) {
            errorMessage.text = message
            errorMessage.visible = true
        }
    }

    Kirigami.InlineMessage {
        id: errorMessage
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom; margins: Kirigami.Units.largeSpacing }
        type: Kirigami.MessageType.Error
        visible: false
        showCloseButton: true
        onVisibleChanged: if (!visible) text = ""
    }
}
