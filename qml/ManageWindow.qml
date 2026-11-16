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
    // Its own title: window managers (and scripts that restore window sizes) must not take
    // it for the dashboard, which is the same application.
    title: i18nc("window title: app name", "%1 – AppImage Manager",
                 backend.metadataLoaded ? backend.displayName : backend.originalName)

    // A fixed size. The limits are constants, not bound to width/height: bound, they followed
    // any outside resize (e.g. a KWin script restoring the dashboard's size) and allowed it.
    readonly property int fixedWidth: Kirigami.Units.gridUnit * 26
    readonly property int fixedHeight: Kirigami.Units.gridUnit * 22
    width: fixedWidth
    height: fixedHeight
    minimumWidth: fixedWidth
    maximumWidth: fixedWidth
    minimumHeight: fixedHeight
    maximumHeight: fixedHeight

    Kirigami.Theme.colorSet: Kirigami.Theme.Window
    Kirigami.Theme.inherit: false

    readonly property int managedIconSize: Kirigami.Units.iconSizes.huge
    readonly property bool dialogOpen: uninstallDialog.visible || replaceDialog.visible
                                       || signatureWarnDialog.visible || aboutDialog.visible

    // Dropped on the folder, by drag or by the keyboard.
    function requestInstall() {
        if (backend.existingState !== AppImageBackend.NotInstalled)
            replaceDialog.open()
        else
            backend.beginInstall(false)
    }

    // Ctrl+Shift+→ installs: the icon slides into the folder like a drag.
    Shortcut {
        sequence: "Ctrl+Shift+Right"
        enabled: backend.metadataLoaded && !backend.isInstalled && !root.dialogOpen
                 && !dragArea.drag.active && !keyboardMove.running
        onActivated: {
            const target = folderContainer.mapToItem(iconContainer, 0, 0)
            keyboardMove.toX = target.x
            keyboardMove.toY = target.y
            keyboardMove.start()
        }
    }
    // Installed: Enter starts the app.
    Shortcut {
        sequences: ["Return", "Enter"]
        enabled: backend.isInstalled && !root.dialogOpen
        onActivated: backend.launchAppImage()
    }

    SequentialAnimation {
        id: keyboardMove
        property real toX: 0
        property real toY: 0
        ScriptAction { script: { dragIcon.x = 0; dragIcon.y = 0 } }
        ParallelAnimation {
            NumberAnimation { target: dragIcon; property: "x"; to: keyboardMove.toX
                              duration: Kirigami.Units.longDuration; easing.type: Easing.InOutQuad }
            NumberAnimation { target: dragIcon; property: "y"; to: keyboardMove.toY
                              duration: Kirigami.Units.longDuration; easing.type: Easing.InOutQuad }
        }
        PauseAnimation { duration: Kirigami.Units.shortDuration }
        ScriptAction {
            script: {
                dragIcon.x = 0
                dragIcon.y = 0
                root.requestInstall()
            }
        }
    }

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

                Controls.Label {
                    Layout.alignment: Qt.AlignHCenter
                    text: [backend.appVersion, backend.appSize > 0 ? backend.formattedSize : ""]
                          .filter(s => s !== "").join(" · ")
                    visible: text !== ""
                    color: Kirigami.Theme.disabledTextColor
                }

                // Another copy of this app is already installed.
                Controls.Label {
                    Layout.alignment: Qt.AlignHCenter
                    visible: !backend.isInstalled && backend.existingState !== AppImageBackend.NotInstalled
                    font: Kirigami.Theme.smallFont
                    text: {
                        switch (backend.existingState) {
                        case AppImageBackend.ThisNewer:
                            return i18n("Version %1 is installed · this one is newer", backend.existingVersion)
                        case AppImageBackend.ThisOlder:
                            return i18n("Version %1 is installed · this one is older", backend.existingVersion)
                        case AppImageBackend.SameVersion:
                            return i18n("This version is already installed")
                        default:
                            return backend.existingVersion !== ""
                                ? i18n("Version %1 is already installed", backend.existingVersion)
                                : i18n("Already installed")
                        }
                    }
                    color: backend.existingState === AppImageBackend.ThisNewer ? Kirigami.Theme.positiveTextColor
                         : backend.existingState === AppImageBackend.ThisOlder ? Kirigami.Theme.neutralTextColor
                         : Kirigami.Theme.disabledTextColor
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

        // ── Icon drag-and-drop area ───────────────────────────────────────────
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            RowLayout {
                id: innerRow
                anchors.centerIn: parent
                spacing: Kirigami.Units.gridUnit * 4

                ColumnLayout {
                    Layout.alignment: Qt.AlignVCenter
                    spacing: Kirigami.Units.smallSpacing

                    Item {
                        id: iconContainer
                        implicitWidth: root.managedIconSize; implicitHeight: root.managedIconSize
                        Layout.alignment: Qt.AlignHCenter

                        // The stationary symbolic icon
                        Kirigami.Icon {
                            id: appIcon
                            anchors.fill: parent
                            source: backend.appIconSource !== "" ? backend.appIconSource : "application-x-executable"
                            opacity: dragArea.drag.active || keyboardMove.running ? 0.3 : 1.0
                            Behavior on opacity { NumberAnimation { duration: Kirigami.Units.shortDuration } }
                        }

                        // The floating drag surrogate
                        Kirigami.Icon {
                            id: dragIcon
                            width: parent.width; height: parent.height
                            source: appIcon.source
                            opacity: 0.8
                            visible: dragArea.drag.active || keyboardMove.running

                            // On during the keyboard move too, so the folder highlights.
                            Drag.active: dragArea.drag.active || keyboardMove.running
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
                                if (dragIcon.Drag.target !== null)
                                    root.requestInstall()
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
                        text: i18n("Drag icon to install (%1)", "Ctrl+Shift+→")
                        visible: !backend.isInstalled
                        color: Kirigami.Theme.disabledTextColor
                        font: Kirigami.Theme.smallFont
                        Layout.alignment: Qt.AlignHCenter
                    }
                }

                ColumnLayout {
                    Layout.alignment: Qt.AlignVCenter
                    spacing: Kirigami.Units.smallSpacing

                    Item {
                        id: folderContainer
                        implicitWidth: root.managedIconSize; implicitHeight: root.managedIconSize
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
                        font.bold: (dragArea.drag.active || keyboardMove.running) && !backend.isInstalled
                    }
                }
            }
        }

        // ── Disclaimer (not installed) ────────────────────────────────────────
        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: !backend.isInstalled && AppSettings.showDisclaimer
            type: Kirigami.MessageType.Warning
            text: i18n("AppImages are unverified executables. Only install from sources you trust.")
            showCloseButton: false
        }

        // ── Remove (installed) ────────────────────────────────────────────────
        Controls.Button {
            Layout.alignment: Qt.AlignHCenter
            visible: backend.isInstalled
            text: i18n("Remove…"); icon.name: "edit-delete"
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

    // ── Already installed: which copy to keep ─────────────────────────────────
    Kirigami.Dialog {
        id: replaceDialog
        readonly property int existing: backend.existingState
        readonly property string installed: backend.existingVersion !== ""
            ? i18nc("app name and version", "%1 %2", backend.displayName, backend.existingVersion)
            : backend.displayName

        title: existing === AppImageBackend.ThisNewer ? i18n("Replace Older Version?")
             : existing === AppImageBackend.ThisOlder ? i18n("Install Older Version?")
             : i18n("Already Installed")
        preferredWidth: Kirigami.Units.gridUnit * 22
        padding: Kirigami.Units.largeSpacing
        standardButtons: Kirigami.Dialog.Cancel

        customFooterActions: [
            Kirigami.Action {
                text: i18n("Keep Both")
                icon.name: "edit-copy"
                // A copy with this file name would be overwritten.
                visible: !backend.existingSameFile
                onTriggered: {
                    replaceDialog.close()
                    backend.beginInstall(false)
                }
            },
            Kirigami.Action {
                text: i18n("Replace")
                icon.name: "document-replace"
                onTriggered: {
                    replaceDialog.close()
                    backend.beginInstall(true)
                }
            }
        ]

        ColumnLayout {
            spacing: Kirigami.Units.largeSpacing
            width: replaceDialog.availableWidth

            Controls.Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: {
                    switch (replaceDialog.existing) {
                    case AppImageBackend.ThisNewer:
                        return i18n("%1 is already installed, and this version (%2) is newer. Install it and move the older version to the Trash?",
                                    replaceDialog.installed, backend.appVersion)
                    case AppImageBackend.ThisOlder:
                        return i18n("%1 is already installed, which is newer than this version (%2). Install this one and move the newer version to the Trash?",
                                    replaceDialog.installed, backend.appVersion)
                    case AppImageBackend.SameVersion:
                        return i18n("%1 is already installed. Replace it with this file?", replaceDialog.installed)
                    default:
                        return i18n("%1 is already installed. Replace it with this one?", replaceDialog.installed)
                    }
                }
            }
            Controls.Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                font: Kirigami.Theme.smallFont
                color: Kirigami.Theme.disabledTextColor
                text: i18n("Its update source, environment variables and app menu entry are kept.")
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
