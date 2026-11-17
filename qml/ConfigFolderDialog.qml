// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import Qt.labs.platform as Platform

// Turns an app's settings folder in Applications (<Applications>/.config/<app>/) on or off,
// and offers to move its existing settings along.
Kirigami.Dialog {
    id: dialog

    property int    proxyRow: -1
    property string appName: ""
    property var    info: ({})   // listModel.configFolder()
    readonly property bool enabling: !(info.enabled ?? false)
    // Folders offered for moving: ~/.config folders when turning on, the folder's contents when off.
    readonly property var candidates: enabling
        ? (info.configDirs ?? []).map(d => ({ path: d.path, checked: d.likely }))
        : (info.contents ?? []).map(p => ({ path: p, checked: true }))

    signal saved(bool enabled)

    title: enabling ? i18n("Keep Settings in “Applications”?") : i18n("Keep Settings in ~/.config?")
    padding: Kirigami.Units.largeSpacing
    standardButtons: Kirigami.Dialog.Cancel
    preferredWidth: Kirigami.Units.gridUnit * 26

    Kirigami.Theme.colorSet: Kirigami.Theme.Window
    Kirigami.Theme.inherit: false

    customFooterActions: [
        Kirigami.Action {
            text: dialog.enabling ? i18n("Turn On") : i18n("Turn Off")
            icon.name: dialog.enabling ? "folder-move" : "edit-undo"
            enabled: dialog.proxyRow >= 0
            onTriggered: dialog.apply()
        }
    ]

    function openFor(row, name) {
        proxyRow = row
        appName = name
        info = proxyModel.configFolder(row)
        open()
    }

    function apply() {
        const moveDirs = []
        for (let i = 0; i < repeater.count; ++i) {
            const box = repeater.itemAt(i)
            if (box && box.checked)
                moveDirs.push(candidates[i].path)
        }
        const enabled = enabling
        proxyModel.setConfigFolder(proxyRow, enabled, moveDirs)
        saved(enabled)
        close()
    }

    function tilde(path) {
        const home = Platform.StandardPaths.writableLocation(Platform.StandardPaths.HomeLocation).toString().replace("file://", "")
        return path.startsWith(home) ? "~" + path.slice(home.length) : path
    }

    onClosed: {
        proxyRow = -1
        appName = ""
        info = ({})
    }

    ColumnLayout {
        spacing: Kirigami.Units.largeSpacing

        Controls.Label {
            Layout.fillWidth: true
            Layout.preferredWidth: 0
            wrapMode: Text.WordWrap
            text: dialog.enabling
                ? i18n("%1 will keep its settings in %2 instead of ~/.config.", dialog.appName, dialog.tilde(dialog.info.path ?? ""))
                : i18n("%1 will keep its settings in ~/.config again.", dialog.appName)
        }

        Controls.Label {
            Layout.fillWidth: true
            Layout.preferredWidth: 0
            visible: dialog.candidates.length > 0
            wrapMode: Text.WordWrap
            text: dialog.enabling ? i18n("Move its current settings there:") : i18n("Move its settings back:")
        }

        Repeater {
            id: repeater
            model: dialog.candidates
            delegate: Controls.CheckBox {
                required property var modelData
                Layout.fillWidth: true
                text: dialog.tilde(modelData.path)
                checked: modelData.checked
            }
        }

        Controls.Label {
            Layout.fillWidth: true
            Layout.preferredWidth: 0
            wrapMode: Text.WordWrap
            font: Kirigami.Theme.smallFont
            color: Kirigami.Theme.disabledTextColor
            text: dialog.candidates.length > 0
                ? i18n("Close %1 before moving its settings.", dialog.appName)
                : dialog.enabling ? i18n("No settings were found in ~/.config; it starts with its defaults.") : ""
            visible: text !== ""
        }
    }
}
