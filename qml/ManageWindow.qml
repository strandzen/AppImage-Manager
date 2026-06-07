// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import appimagemanager

Kirigami.ApplicationWindow {
    id: root
    visible: true
    title: i18n("AppImage Manager")

    width:  Kirigami.Units.gridUnit * 26
    height: Kirigami.Units.gridUnit * 22
    minimumWidth:  width;  maximumWidth:  width
    minimumHeight: height; maximumHeight: height

    Kirigami.Theme.colorSet: Kirigami.Theme.Window
    Kirigami.Theme.inherit: false

    readonly property int managedIconSize: [
        Kirigami.Units.iconSizes.huge,
        Math.round(Kirigami.Units.iconSizes.huge * 1.5),
        Kirigami.Units.iconSizes.enormous
    ][AppSettings.manageIconSize]

    readonly property color cardBorderColor: Theme.cardBorderColor

    UninstallDialog { id: uninstallDialog }

    // ── About dialog ─────────────────────────────────────────────────────────
    AboutDialog { id: aboutDialog }

    Controls.BusyIndicator {
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
                    Controls.ToolTip.visible: infoMouseArea.containsMouse; Controls.ToolTip.text: backend.originalName
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
                        text: i18n("Size: %1", backend.appSize > 0 ? backend.formattedSize : i18n("Unknown"))
                        closable: false; checkable: false
                    }
                }
            }

            Item { Layout.fillWidth: true }

            Controls.ToolButton {
                icon.name: "help-about"
                onClicked: aboutDialog.open()
                Controls.ToolTip.text: i18n("About"); Controls.ToolTip.visible: hovered
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

                        // The stationary symbolic icon
                        Kirigami.Icon {
                            id: appIcon
                            anchors.fill: parent
                            source: backend.appIconSource !== "" ? backend.appIconSource : "application-x-executable"
                            opacity: dragArea.drag.active ? 0.3 : 1.0
                            Behavior on opacity { NumberAnimation { duration: Kirigami.Units.shortDuration } }
                        }

                        // The floating drag surrogate
                        Kirigami.Icon {
                            id: dragIcon
                            width: parent.width; height: parent.height
                            source: appIcon.source
                            opacity: 0.8
                            visible: dragArea.drag.active

                            Drag.active: dragArea.drag.active
                            Drag.keys: ["appimage"]
                            Drag.hotSpot.x: width / 2
                            Drag.hotSpot.y: height / 2
                        }

                        MouseArea {
                            id: dragArea; anchors.fill: parent
                            hoverEnabled: backend.isInstalled
                            drag.target: backend.isInstalled ? null : dragIcon
                            cursorShape: backend.isInstalled ? Qt.PointingHandCursor : Qt.OpenHandCursor
                            onClicked: if (backend.isInstalled) backend.launchAppImage()
                            onPressed: {
                                if (backend.isInstalled) return
                                dragIcon.x = 0
                                dragIcon.y = 0
                            }
                            onReleased: {
                                if (backend.isInstalled) return
                                if (dragIcon.Drag.target !== null) {
                                    backend.beginInstall()
                                }
                                // Reset position instantly to return to source
                                dragIcon.x = 0
                                dragIcon.y = 0
                            }
                            Controls.ToolTip.visible: backend.isInstalled && containsMouse
                            Controls.ToolTip.text: i18n("Launch %1", backend.displayName)
                        }
                    }
                    Controls.Label {
                        text: backend.displayName; horizontalAlignment: Text.AlignHCenter
                        Layout.alignment: Qt.AlignHCenter; elide: Text.ElideRight
                        Layout.maximumWidth: Kirigami.Units.gridUnit * 10; font.bold: backend.isInstalled
                    }
                    Controls.Label {
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
                            Controls.ToolTip.text: i18n("Open Dashboard")
                            Controls.ToolTip.visible: containsMouse && !folderDropArea.containsDrag
                        }
                    }

                    Controls.Label {
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
        Controls.Button {
            Layout.alignment: Qt.AlignHCenter
            visible: backend.isInstalled
            text: i18n("Remove"); icon.name: "trash-empty"
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
        function onSignatureCheckReady(state) {
            // SigValid=1, SigUnchecked=0, SigGpgUnavailable=4 → safe to proceed
            if (state === 1 || state === 0 || state === 4) {
                backend.doInstall()
            } else {
                // SigUnsigned=3 or SigInvalid=2 → warn the user first
                signatureWarnDialog.sigState = state
                signatureWarnDialog.open()
            }
        }
    }

    // ── Signature warning dialog ──────────────────────────────────────────────
    Kirigami.Dialog {
        id: signatureWarnDialog
        property int sigState: 3  // default: Unsigned

        title: sigState === 2 ? i18n("Invalid Signature") : i18n("No Signature Found")
        preferredWidth: Kirigami.Units.gridUnit * 22

        standardButtons: Kirigami.Dialog.Ok | Kirigami.Dialog.Cancel
        onAccepted: backend.doInstall()
        onClosed: sigState = 3

        ColumnLayout {
            spacing: Kirigami.Units.largeSpacing
            width: signatureWarnDialog.availableWidth

            Kirigami.Icon {
                Layout.alignment: Qt.AlignHCenter
                source: signatureWarnDialog.sigState === 2 ? "security-low" : "security-medium"
                implicitWidth:  Kirigami.Units.iconSizes.large
                implicitHeight: Kirigami.Units.iconSizes.large
            }

            Controls.Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
                text: signatureWarnDialog.sigState === 2
                    ? i18n("The GPG signature on this AppImage is <b>invalid</b>. The file may have been tampered with. Installing is strongly discouraged.")
                    : i18n("This AppImage has no GPG signature and cannot be verified. Only install from sources you trust.")
            }
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
