// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Kirigami.Dialog {
    id: dialog

    property string appName: ""
    property string currentVersion: ""
    property string newVersion: ""
    property int    proxyRow: -1

    signal downloadRequested()

    title: dialog.appName ? i18n("Update — %1", dialog.appName) : i18n("Update Available")
    padding: Kirigami.Units.largeSpacing
    standardButtons: Kirigami.Dialog.NoButton
    preferredWidth: Kirigami.Units.gridUnit * 22

    Kirigami.Theme.colorSet: Kirigami.Theme.Window
    Kirigami.Theme.inherit: false

    onClosed: {
        proxyRow = -1
        appName = ""
        currentVersion = ""
        newVersion = ""
    }

    ColumnLayout {
        spacing: Kirigami.Units.largeSpacing

        Controls.Label {
            Layout.fillWidth: true
            text: dialog.appName
            font.bold: true
            elide: Text.ElideRight
        }

        // Version comparison row
        RowLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.largeSpacing

            ColumnLayout {
                spacing: Kirigami.Units.smallSpacing

                Controls.Label {
                    text: i18n("Current version")
                    opacity: 0.7
                    font: Kirigami.Theme.smallFont
                }
                Controls.Label {
                    text: dialog.currentVersion || "—"
                    font.bold: true
                }
            }

            Kirigami.Icon {
                source: "arrow-right"
                implicitWidth:  Kirigami.Units.iconSizes.small
                implicitHeight: Kirigami.Units.iconSizes.small
                Layout.alignment: Qt.AlignVCenter
            }

            ColumnLayout {
                spacing: Kirigami.Units.smallSpacing

                Controls.Label {
                    text: i18n("New version")
                    opacity: 0.7
                    font: Kirigami.Theme.smallFont
                }
                Controls.Label {
                    text: dialog.newVersion || "—"
                    font.bold: true
                    color: Kirigami.Theme.positiveTextColor
                }
            }

            Item { Layout.fillWidth: true }
        }

        Kirigami.Separator { Layout.fillWidth: true }

        // Action buttons
        RowLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing

            Item { Layout.fillWidth: true }

            Controls.Button {
                text: i18n("Cancel")
                onClicked: dialog.close()
            }

            Controls.Button {
                text: i18n("Download Update")
                icon.name: "download"
                enabled: dialog.proxyRow >= 0
                palette.button:     Kirigami.Theme.positiveBackgroundColor
                palette.buttonText: Kirigami.Theme.positiveTextColor
                onClicked: {
                    dialog.downloadRequested()
                    dialog.close()
                }
            }
        }
    }
}
