// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// Per-AppImage update source that overrides (or supplies) the AppImage's embedded
// update information: a GitHub, Codeberg or Forgejo/Gitea repository, a .zsync file
// or a direct download URL. One field; the kind is detected and can be overridden.
Kirigami.Dialog {
    id: dialog

    property int    proxyRow: -1
    property string appName: ""
    property string embedded: ""       // the AppImage's own update info
    property string embeddedLabel: ""  // e.g. "GitHub · owner/repo"
    property bool   hasOverride: false
    property string forcedType: ""     // "" = detect

    signal saved(bool cleared)

    readonly property string embeddedRepo: {
        const parts = embedded.split("|")
        return parts[0] === "gh-releases-zsync" && parts.length >= 3 ? parts[1] + "/" + parts[2] : ""
    }
    // listModel is the dashboard's context property (absent in isolated tests).
    readonly property var detected: typeof listModel !== "undefined"
        ? listModel.describeUpdateSourceInput(repoField.text, forcedType)
        : ({ valid: false, type: "", label: "" })
    readonly property bool isRepository: detected.type === "github" || detected.type === "forgejo"

    readonly property var typeNames: ({
        "": i18n("Detect Automatically"),
        "github": i18n("GitHub Repository"),
        "forgejo": i18n("Codeberg, Forgejo or Gitea Repository"),
        "zsync": i18n("zsync File"),
        "url": i18n("Direct Download")
    })

    title: dialog.appName ? i18n("Update Source — %1", dialog.appName) : i18n("Update Source")
    padding: Kirigami.Units.largeSpacing
    standardButtons: Kirigami.Dialog.Cancel
    preferredWidth: Kirigami.Units.gridUnit * 26

    Kirigami.Theme.colorSet: Kirigami.Theme.Window
    Kirigami.Theme.inherit: false

    customFooterActions: [
        Kirigami.Action {
            text: i18n("Use Built-in")
            icon.name: "edit-undo"
            visible: dialog.hasOverride
            onTriggered: {
                repoField.text = ""
                prereleaseBox.checked = false
                dialog.forcedType = ""
                dialog.save()
            }
        },
        Kirigami.Action {
            text: i18n("Save")
            icon.name: "document-save"
            enabled: dialog.proxyRow >= 0 && (repoField.text.trim() === "" || dialog.detected.valid)
            onTriggered: dialog.save()
        }
    ]

    function openFor(row, name) {
        const src = proxyModel.updateSource(row)
        proxyRow = row
        appName = name
        embedded = src.embedded ?? ""
        embeddedLabel = src.embeddedLabel ?? ""
        hasOverride = (src.type ?? "") !== ""
        forcedType = ""
        repoField.text = hasOverride ? src.input : embeddedRepo
        prereleaseBox.checked = src.prerelease ?? false
        errorMessage.visible = false
        open()
    }

    function save() {
        const trimmed = repoField.text.trim()
        // Unchanged built-in GitHub repo without pre-releases needs no override.
        const unchanged = trimmed === embeddedRepo && !prereleaseBox.checked && forcedType === ""
        const value = unchanged ? "" : trimmed
        if (!proxyModel.setUpdateSource(proxyRow, value, forcedType, isRepository && prereleaseBox.checked)) {
            errorMessage.visible = true
            return
        }
        saved(value === "")
        close()
    }

    onClosed: {
        proxyRow = -1
        appName = ""
        embedded = ""
        embeddedLabel = ""
        hasOverride = false
        forcedType = ""
    }

    ColumnLayout {
        spacing: Kirigami.Units.largeSpacing

        Controls.Label {
            Layout.fillWidth: true
            Layout.preferredWidth: 0
            wrapMode: Text.WordWrap
            color: Kirigami.Theme.disabledTextColor
            text: dialog.embeddedLabel !== ""
                ? i18n("Built-in source: %1", dialog.embeddedLabel)
                : i18n("This AppImage has no built-in update source. Enter where it is released.")
        }

        ColumnLayout {
            spacing: Kirigami.Units.smallSpacing

            Controls.Label { text: i18n("Source") }

            Controls.TextField {
                id: repoField
                Layout.fillWidth: true
                placeholderText: i18n("owner/repo, a repository URL, or a download URL")
                onTextEdited: errorMessage.visible = false
                onAccepted: dialog.save()
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing

                Kirigami.Icon {
                    source: dialog.detected.valid ? "dialog-ok" : "dialog-warning"
                    implicitWidth: Kirigami.Units.iconSizes.small
                    implicitHeight: Kirigami.Units.iconSizes.small
                    visible: repoField.text.trim() !== ""
                }

                Controls.Label {
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                    color: Kirigami.Theme.disabledTextColor
                    text: repoField.text.trim() === ""
                        ? i18n("Empty: use the built-in source")
                        : dialog.detected.valid ? dialog.detected.label
                                                : i18n("Not a repository or download URL")
                }

                Controls.ToolButton {
                    id: typeButton
                    text: i18n("Type: %1", dialog.typeNames[dialog.forcedType])
                    font: Kirigami.Theme.smallFont
                    down: typeMenu.visible
                    onPressed: typeMenu.visible ? typeMenu.close() : typeMenu.popup(typeButton, 0, typeButton.height)

                    Controls.Menu {
                        id: typeMenu
                        Repeater {
                            model: ["", "github", "forgejo", "zsync", "url"]
                            Controls.MenuItem {
                                required property string modelData
                                text: dialog.typeNames[modelData]
                                checkable: true
                                autoExclusive: true
                                checked: dialog.forcedType === modelData
                                onTriggered: {
                                    dialog.forcedType = modelData
                                    errorMessage.visible = false
                                }
                            }
                        }
                    }
                }
            }

            Controls.CheckBox {
                id: prereleaseBox
                visible: dialog.isRepository
                text: i18n("Include pre-releases")
            }
        }

        Kirigami.InlineMessage {
            id: errorMessage
            Layout.fillWidth: true
            type: Kirigami.MessageType.Error
            text: i18n("That cannot be used as an update source. Enter owner/repo, a repository URL, or a download URL.")
            visible: false
        }

        Controls.Label {
            Layout.fillWidth: true
            Layout.preferredWidth: 0
            wrapMode: Text.WordWrap
            color: Kirigami.Theme.disabledTextColor
            font: Kirigami.Theme.smallFont
            text: dialog.isRepository
                ? i18n("The release file matching this computer's architecture is downloaded. Delta updates are used when the release also publishes a .zsync file.")
                : dialog.detected.type === "url"
                    ? i18n("The file is downloaded again whenever the server reports a newer build. Web pages are refused.")
                    : ""
            visible: text !== ""
        }
    }
}
