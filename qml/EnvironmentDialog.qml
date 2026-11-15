// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// Per-AppImage launch environment: KEY=value per line, applied to launches from
// AppImage Manager and from the app menu entry.
Kirigami.Dialog {
    id: dialog

    property int    proxyRow: -1
    property string appName: ""

    signal saved()

    title: dialog.appName ? i18n("Environment Variables — %1", dialog.appName) : i18n("Environment Variables")
    padding: Kirigami.Units.largeSpacing
    standardButtons: Kirigami.Dialog.Cancel
    preferredWidth: Kirigami.Units.gridUnit * 26

    Kirigami.Theme.colorSet: Kirigami.Theme.Window
    Kirigami.Theme.inherit: false

    customFooterActions: [
        Kirigami.Action {
            text: i18n("Save")
            icon.name: "document-save"
            enabled: dialog.proxyRow >= 0
            onTriggered: dialog.save()
        }
    ]

    function openFor(row, name) {
        proxyRow = row
        appName = name
        textArea.text = proxyModel.environment(row)
        errorMessage.visible = false
        open()
        textArea.forceActiveFocus()
    }

    function save() {
        const invalid = proxyModel.setEnvironment(proxyRow, textArea.text)
        if (invalid.length > 0) {
            errorMessage.text = i18np("Line %2 is not KEY=value or repeats a name.",
                                      "Lines %2 are not KEY=value or repeat a name.",
                                      invalid.length, invalid.join(", "))
            errorMessage.visible = true
            return
        }
        saved()
        close()
    }

    onClosed: {
        proxyRow = -1
        appName = ""
    }

    ColumnLayout {
        spacing: Kirigami.Units.largeSpacing

        Controls.Label {
            Layout.fillWidth: true
            Layout.preferredWidth: 0
            wrapMode: Text.WordWrap
            color: Kirigami.Theme.disabledTextColor
            text: i18n("One KEY=value per line. Used when the app is launched from here and from the app menu.")
        }

        Controls.ScrollView {
            Layout.fillWidth: true
            Layout.preferredHeight: Kirigami.Units.gridUnit * 8

            Controls.TextArea {
                id: textArea
                font.family: "monospace"
                wrapMode: TextEdit.NoWrap
                placeholderText: "WEBKIT_DISABLE_DMABUF_RENDERER=1"
                onTextChanged: errorMessage.visible = false
            }
        }

        Kirigami.InlineMessage {
            id: errorMessage
            Layout.fillWidth: true
            type: Kirigami.MessageType.Error
            visible: false
        }
    }
}
