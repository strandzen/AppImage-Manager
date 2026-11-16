// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// Release notes of an app with a GitHub or Forgejo/Codeberg/Gitea update source: every release
// newer than the installed one (or the newest), fetched when opened (listModel.fetchReleaseNotes).
Kirigami.Dialog {
    id: dialog

    property string filePath: ""
    property string appName: ""
    property string updateVersion: ""   // set when an update waits
    property bool   loading: false
    property string error: ""
    property string pageUrl: ""
    property var    notes: []

    title: dialog.updateVersion !== "" ? i18n("What's New in %1", dialog.appName)
                                      : i18n("Release Notes — %1", dialog.appName)
    padding: Kirigami.Units.largeSpacing * 2
    // Fixed: bound to the window's height, Kirigami's Dialog loops on its own position.
    preferredWidth: Kirigami.Units.gridUnit * 34
    preferredHeight: Kirigami.Units.gridUnit * 30
    standardButtons: Kirigami.Dialog.Close

    Kirigami.Theme.colorSet: Kirigami.Theme.Window
    Kirigami.Theme.inherit: false

    customFooterActions: [
        Kirigami.Action {
            text: dialog.pageUrl.indexOf("github.com") >= 0 ? i18n("Open on GitHub") : i18n("Open in Browser")
            icon.name: "internet-services"
            enabled: dialog.pageUrl !== ""
            // The newest release's own page when there is one, else the releases page.
            onTriggered: Qt.openUrlExternally(dialog.notes.length > 0 && dialog.notes[0].url ? dialog.notes[0].url
                                                                                             : dialog.pageUrl)
        }
    ]

    function openFor(row, item) {
        filePath = item.filePath ?? ""
        appName = item.displayName ?? ""
        updateVersion = item.updateAvailable ? (item.updateVersion ?? "") : ""
        notes = []
        error = ""
        pageUrl = item.updateSourceUrl ? item.updateSourceUrl + "/releases" : ""
        loading = true
        open()
        proxyModel.fetchReleaseNotes(row)
    }

    onClosed: {
        filePath = ""
        notes = []
    }

    Connections {
        target: listModel
        function onReleaseNotesReady(path, notes, pageUrl, error) {
            if (path !== dialog.filePath)
                return
            dialog.loading = false
            dialog.notes = notes
            dialog.error = error
            if (pageUrl !== "")
                dialog.pageUrl = pageUrl
        }
    }

    ColumnLayout {
        width: dialog.availableWidth
        spacing: Kirigami.Units.largeSpacing

        Controls.BusyIndicator {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: Kirigami.Units.gridUnit * 6
            visible: dialog.loading
            running: visible
        }

        Kirigami.PlaceholderMessage {
            Layout.fillWidth: true
            Layout.topMargin: Kirigami.Units.gridUnit * 4
            visible: !dialog.loading && dialog.notes.length === 0
            icon.name: "documentation"
            text: i18n("No release notes")
            explanation: dialog.error
        }

        // Several releases: say what the list covers.
        Controls.Label {
            Layout.fillWidth: true
            visible: !dialog.loading && dialog.notes.length > 1
            wrapMode: Text.WordWrap
            color: Kirigami.Theme.disabledTextColor
            // 20 is what one request returns: there may be more.
            text: dialog.notes.length >= 20
                  ? i18n("The %1 newest releases", dialog.notes.length)
                  : i18np("%1 release since the installed version", "%1 releases since the installed version",
                          dialog.notes.length)
        }

        Repeater {
            model: dialog.loading ? [] : dialog.notes
            delegate: ColumnLayout {
                id: release
                required property var modelData
                required property int index
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing

                Kirigami.Separator {
                    Layout.fillWidth: true
                    Layout.bottomMargin: Kirigami.Units.largeSpacing
                    visible: release.index > 0
                }

                Kirigami.Heading {
                    Layout.fillWidth: true
                    level: 2
                    wrapMode: Text.WordWrap
                    text: release.modelData.name || release.modelData.tag
                }
                Controls.Label {
                    Layout.fillWidth: true
                    visible: text !== ""
                    color: Kirigami.Theme.disabledTextColor
                    text: [release.modelData.name && release.modelData.name !== release.modelData.tag ? release.modelData.tag : "",
                           release.modelData.date,
                           release.modelData.prerelease ? i18n("Pre-release") : ""].filter(s => s).join(" · ")
                }

                Controls.Label {
                    id: body
                    Layout.fillWidth: true
                    Layout.topMargin: Kirigami.Units.smallSpacing
                    textFormat: release.modelData.body ? Text.MarkdownText : Text.PlainText
                    text: release.modelData.body || i18n("No notes were written for this release.")
                    color: release.modelData.body ? Kirigami.Theme.textColor : Kirigami.Theme.disabledTextColor
                    wrapMode: Text.WordWrap
                    onLinkActivated: (link) => Qt.openUrlExternally(link)
                    HoverHandler {
                        cursorShape: body.hoveredLink !== "" ? Qt.PointingHandCursor : Qt.ArrowCursor
                    }
                }
            }
        }
    }
}
