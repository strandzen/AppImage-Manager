// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard
import appimagemanager

Kirigami.Page {
    id: pageRoot
    title: i18n("About")

    ContributorsModel { id: contributorsModel }

    Component.onCompleted: contributorsModel.fetch()

    Controls.ScrollView {
        id: scrollView
        anchors.fill: parent
        clip: true
        Controls.ScrollBar.horizontal.policy: Controls.ScrollBar.AlwaysOff
        Controls.ScrollBar.vertical.policy: Controls.ScrollBar.AsNeeded

        ColumnLayout {
            width: scrollView.availableWidth - Kirigami.Units.largeSpacing * 2
            x: Kirigami.Units.largeSpacing
            spacing: 0

            // ── Card 1: App Header ───────────────────────────────────────────────
            FormCard.FormHeader {
                title: i18n("Application Info")
            }

            FormCard.FormCard {
                FormCard.AbstractFormDelegate {
                    background: null
                    contentItem: RowLayout {
                        spacing: Kirigami.Units.largeSpacing

                        Kirigami.Icon {
                            source: "appimagemanager"
                            fallback: "application-x-executable"
                            implicitWidth: Kirigami.Units.iconSizes.huge
                            implicitHeight: Kirigami.Units.iconSizes.huge
                            Layout.alignment: Qt.AlignVCenter
                        }

                        ColumnLayout {
                            spacing: Kirigami.Units.smallSpacing
                            Layout.fillWidth: true

                            Kirigami.Heading {
                                text: i18n("AppImage Manager %1", AppSettings.appVersion)
                                level: 2
                                Layout.fillWidth: true
                            }
                            
                            Controls.Label {
                                text: i18n("A lightweight AppImage manager for KDE Plasma 6.")
                                opacity: 0.8
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                            }
                        }
                    }
                }
            }

            // ── Card 2: Project Links and License ────────────────────────────────
            FormCard.FormHeader {
                title: i18n("Project Details")
            }

            FormCard.FormCard {
                FormCard.AbstractFormDelegate {
                    onClicked: Qt.openUrlExternally("https://www.gnu.org/licenses/old-licenses/gpl-2.0.html")
                    
                    contentItem: RowLayout {
                        spacing: Kirigami.Units.largeSpacing

                        ColumnLayout {
                            spacing: Kirigami.Units.smallSpacing
                            Layout.fillWidth: true

                            Controls.Label {
                                text: i18n("License")
                                opacity: 0.6
                                font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                            }
                            Controls.Label {
                                text: i18n("GPL v2 or later")
                                font.bold: true
                            }
                        }

                        Kirigami.Icon {
                            source: "go-next"
                            implicitWidth: Kirigami.Units.iconSizes.small
                            implicitHeight: Kirigami.Units.iconSizes.small
                            Layout.alignment: Qt.AlignVCenter
                        }
                    }
                }

                FormCard.FormDelegateSeparator {}

                FormCard.AbstractFormDelegate {
                    onClicked: Qt.openUrlExternally("https://github.com/strandzen/AppImage-Manager")
                    
                    contentItem: RowLayout {
                        spacing: Kirigami.Units.largeSpacing

                        Kirigami.Icon {
                            source: "get-involved"
                            fallback: "internet-services"
                            implicitWidth: Kirigami.Units.iconSizes.smallMedium
                            implicitHeight: Kirigami.Units.iconSizes.smallMedium
                            Layout.alignment: Qt.AlignVCenter
                        }

                        Controls.Label {
                            text: i18n("Contribute")
                            Layout.fillWidth: true
                            verticalAlignment: Text.AlignVCenter
                        }

                        Kirigami.Icon {
                            source: "link"
                            implicitWidth: Kirigami.Units.iconSizes.small
                            implicitHeight: Kirigami.Units.iconSizes.small
                            opacity: 0.5
                            Layout.alignment: Qt.AlignVCenter
                        }
                    }
                }

                FormCard.FormDelegateSeparator {}

                FormCard.AbstractFormDelegate {
                    onClicked: Qt.openUrlExternally("https://github.com/strandzen/AppImage-Manager/issues")
                    
                    contentItem: RowLayout {
                        spacing: Kirigami.Units.largeSpacing

                        Kirigami.Icon {
                            source: "tools-report-bug"
                            fallback: "dialog-warning"
                            implicitWidth: Kirigami.Units.iconSizes.smallMedium
                            implicitHeight: Kirigami.Units.iconSizes.smallMedium
                            Layout.alignment: Qt.AlignVCenter
                        }

                        Controls.Label {
                            text: i18n("Report an Issue")
                            Layout.fillWidth: true
                            verticalAlignment: Text.AlignVCenter
                        }

                        Kirigami.Icon {
                            source: "link"
                            implicitWidth: Kirigami.Units.iconSizes.small
                            implicitHeight: Kirigami.Units.iconSizes.small
                            opacity: 0.5
                            Layout.alignment: Qt.AlignVCenter
                        }
                    }
                }
            }

            // ── Card 3: Libraries ────────────────────────────────────────────────
            FormCard.FormHeader {
                title: i18n("Libraries in Use")
            }

            FormCard.FormCard {
                FormCard.AbstractFormDelegate {
                    background: null
                    contentItem: RowLayout {
                        Controls.Label {
                            text: i18n("Copy environment details to clipboard")
                            opacity: 0.7
                            font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                            Layout.fillWidth: true
                            Layout.alignment: Qt.AlignVCenter
                        }
                        Controls.Button {
                            icon.name: "edit-copy"
                            text: i18n("Copy to Clipboard")
                            Layout.alignment: Qt.AlignVCenter
                            onClicked: {
                                var specs = "AppImage Manager: " + AppSettings.appVersion + "\n" +
                                            "Qt Version: " + AppSettings.qtVersion + "\n" +
                                            "KDE Frameworks Version: " + AppSettings.kdeFrameworksVersion + "\n" +
                                            "Metadata Extractor: " + AppSettings.libappimageVersion;
                                AppSettings.copyToClipboard(specs);
                                showPassiveNotification(i18n("Environment details copied to clipboard."));
                            }
                        }
                    }
                }

                FormCard.FormDelegateSeparator {}

                FormCard.AbstractFormDelegate {
                    background: null
                    contentItem: RowLayout {
                        spacing: Kirigami.Units.largeSpacing
                        ColumnLayout {
                            spacing: Kirigami.Units.smallSpacing
                            Layout.fillWidth: true
                            Controls.Label { text: i18n("Qt %1", AppSettings.qtVersion); font.bold: true }
                            Controls.Label {
                                text: i18n("Cross-platform application development framework.")
                                opacity: 0.6; wrapMode: Text.WordWrap; Layout.fillWidth: true
                                font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                            }
                        }
                        Controls.Button {
                            icon.name: "internet-services"; display: Controls.AbstractButton.IconOnly
                            onClicked: Qt.openUrlExternally("https://www.qt.io/")
                            Controls.ToolTip.text: i18n("Visit Qt Website"); Controls.ToolTip.visible: hovered
                            Layout.alignment: Qt.AlignVCenter
                        }
                    }
                }

                FormCard.FormDelegateSeparator {}

                FormCard.AbstractFormDelegate {
                    background: null
                    contentItem: RowLayout {
                        spacing: Kirigami.Units.largeSpacing
                        ColumnLayout {
                            spacing: Kirigami.Units.smallSpacing
                            Layout.fillWidth: true
                            Controls.Label { text: i18n("KDE Frameworks %1", AppSettings.kdeFrameworksVersion); font.bold: true }
                            Controls.Label {
                                text: i18n("Collection of libraries created by the KDE Community to extend Qt.")
                                opacity: 0.6; wrapMode: Text.WordWrap; Layout.fillWidth: true
                                font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                            }
                        }
                        Controls.Button {
                            icon.name: "internet-services"; display: Controls.AbstractButton.IconOnly
                            onClicked: Qt.openUrlExternally("https://kde.org/products/frameworks/")
                            Controls.ToolTip.text: i18n("Visit KDE Website"); Controls.ToolTip.visible: hovered
                            Layout.alignment: Qt.AlignVCenter
                        }
                    }
                }

                FormCard.FormDelegateSeparator {}

                FormCard.AbstractFormDelegate {
                    background: null
                    contentItem: RowLayout {
                        spacing: Kirigami.Units.largeSpacing
                        ColumnLayout {
                            spacing: Kirigami.Units.smallSpacing
                            Layout.fillWidth: true
                            Controls.Label { text: AppSettings.libappimageVersion; font.bold: true }
                            Controls.Label {
                                text: i18n("SquashFS extraction module used for reading AppImage configurations.")
                                opacity: 0.6; wrapMode: Text.WordWrap; Layout.fillWidth: true
                                font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                            }
                        }
                    }
                }
            }

            // ── Card 4: Contributors ─────────────────────────────────────────────
            FormCard.FormHeader {
                title: i18n("Contributors")
            }

            FormCard.FormCard {
                FormCard.AbstractFormDelegate {
                    background: null
                    visible: contributorsModel.isLoading
                    contentItem: Controls.BusyIndicator {
                        running: contributorsModel.isLoading
                        implicitHeight: Kirigami.Units.gridUnit * 3
                    }
                }

                Repeater {
                    model: contributorsModel.isLoading ? 0 : contributorsModel.contributors.length > 0 ? contributorsModel.contributors : contributorsModel.fallbackContributors
                    delegate: ColumnLayout {
                        width: parent.width
                        spacing: 0

                        FormCard.AbstractFormDelegate {
                            background: null
                            Layout.fillWidth: true
                            contentItem: RowLayout {
                                spacing: Kirigami.Units.largeSpacing

                                Rectangle {
                                    implicitWidth: Kirigami.Units.gridUnit * 2
                                    implicitHeight: Kirigami.Units.gridUnit * 2
                                    radius: implicitWidth / 2
                                    color: Kirigami.Theme.highlightColor
                                    Controls.Label {
                                        anchors.centerIn: parent
                                        text: modelData.initials
                                        color: Kirigami.Theme.highlightedTextColor
                                        font.bold: true
                                    }
                                }

                                ColumnLayout {
                                    spacing: 0
                                    Layout.fillWidth: true
                                    Layout.alignment: Qt.AlignVCenter
                                    Controls.Label { text: modelData.name; font.bold: true }
                                    Controls.Label {
                                        text: modelData.role
                                        opacity: 0.6
                                        font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                                    }
                                }

                                RowLayout {
                                    spacing: Kirigami.Units.mediumSpacing
                                    Controls.Button {
                                        icon.name: "mail-send"
                                        display: Controls.AbstractButton.IconOnly
                                        visible: modelData.email !== ""
                                        onClicked: Qt.openUrlExternally("mailto:" + modelData.email)
                                        Controls.ToolTip.text: i18n("Email Contributor")
                                        Controls.ToolTip.visible: hovered
                                    }
                                    Controls.Button {
                                        icon.name: "internet-services"
                                        display: Controls.AbstractButton.IconOnly
                                        onClicked: Qt.openUrlExternally(modelData.web)
                                        Controls.ToolTip.text: i18n("Visit Profile")
                                        Controls.ToolTip.visible: hovered
                                    }
                                }
                            }
                        }

                        FormCard.FormDelegateSeparator {
                            visible: index < (contributorsModel.contributors.length > 0 ? contributorsModel.contributors.length : contributorsModel.fallbackContributors.length) - 1
                        }
                    }
                }
            }

            Item {
                height: Kirigami.Units.largeSpacing
            }
        }
    }
}
