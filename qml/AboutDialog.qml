// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import appimagemanager

Kirigami.Dialog {
    id: aboutDialog
    title: i18n("About AppImage Manager")
    padding: Kirigami.Units.largeSpacing
    standardButtons: Kirigami.Dialog.Close
    
    preferredWidth: Kirigami.Units.gridUnit * 24
    preferredHeight: Kirigami.Units.gridUnit * 28

    readonly property color cardBorderColor: AppSettings.accentBorders 
        ? aboutDialog.Kirigami.Theme.focusColor 
        : (aboutDialog.Kirigami.Theme.textColor && aboutDialog.Kirigami.Theme.textColor.r !== undefined
            ? Qt.rgba(aboutDialog.Kirigami.Theme.textColor.r, aboutDialog.Kirigami.Theme.textColor.g, aboutDialog.Kirigami.Theme.textColor.b, 0.15)
            : Qt.rgba(0.5, 0.5, 0.5, 0.15))

    property var fallbackContributors: [
        {
            "name": "Herman",
            "role": i18n("Lead Developer"),
            "initials": "H",
            "email": "strandzen2@gmail.com",
            "web": "https://github.com/strandzen"
        },
        {
            "name": "Heimen Stoffels",
            "role": i18n("Dutch Translator / Contributor"),
            "initials": "HS",
            "email": "vistausss@fastmail.com",
            "web": "https://github.com/vistausss"
        }
    ]

    property var contributors: []
    property bool isLoadingContributors: false

    function getInitials(name) {
        if (!name) return "?";
        var parts = name.split(" ").filter(function(p) { return p.length > 0; });
        if (parts.length === 0) return "?";
        if (parts.length === 1) return parts[0].substring(0, Math.min(2, parts[0].length)).toUpperCase();
        return (parts[0][0] + parts[1][0]).toUpperCase();
    }

    function fetchContributors() {
        isLoadingContributors = true;
        var xhr = new XMLHttpRequest();
        xhr.open("GET", "https://api.github.com/repos/strandzen/AppImage-Manager/contributors", true);
        xhr.timeout = 8000;

        function handleFailure() {
            isLoadingContributors = false;
            contributors = fallbackContributors;
        }

        xhr.onerror   = handleFailure;
        xhr.ontimeout = handleFailure;

        xhr.onreadystatechange = function() {
            if (xhr.readyState === XMLHttpRequest.DONE) {
                isLoadingContributors = false;
                if (xhr.status === 200) {
                    try {
                        var rawList = JSON.parse(xhr.responseText);
                        var parsed = [];
                        for (var i = 0; i < rawList.length; i++) {
                            var item = rawList[i];
                            var login = item.login ? item.login.toLowerCase() : "";

                            // Exclude Claude and other AI / bots / assistants
                            if (login.indexOf("claude") !== -1 ||
                                login.indexOf("gemini") !== -1 ||
                                login.indexOf("antigravity") !== -1 ||
                                login.indexOf("copilot") !== -1 ||
                                login.indexOf("bot") !== -1 ||
                                login.indexOf("assistant") !== -1) {
                                continue;
                            }

                            var dispName = item.login || "Contributor";
                            parsed.push({
                                "name": dispName,
                                "role": i === 0 ? i18n("Lead Developer") : i18n("Contributor"),
                                "initials": getInitials(dispName),
                                "email": "",
                                "web": item.html_url || "https://github.com/strandzen/AppImage-Manager"
                            });
                        }

                        contributors = parsed.length > 0 ? parsed : fallbackContributors;
                    } catch (e) {
                        contributors = fallbackContributors;
                    }
                } else {
                    contributors = fallbackContributors;
                }
            }
        }
        xhr.send();
    }

    onOpened: fetchContributors()

    Controls.ScrollView {
        id: scrollView
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        Controls.ScrollBar.horizontal.policy: Controls.ScrollBar.AlwaysOff
        Controls.ScrollBar.vertical.policy: Controls.ScrollBar.AsNeeded

        ColumnLayout {
            id: contentLayout
            spacing: Kirigami.Units.largeSpacing
            width: scrollView.availableWidth - Kirigami.Units.largeSpacing
            x: Kirigami.Units.largeSpacing / 2

        // ── Card 1: App Header ───────────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: headerLayout.implicitHeight + Kirigami.Units.largeSpacing * 2
            color: Kirigami.Theme.alternateBackgroundColor
            border.color: aboutDialog.cardBorderColor
            border.width: 1
            radius: Kirigami.Units.smallSpacing * 2

            RowLayout {
                id: headerLayout
                anchors.fill: parent
                anchors.margins: Kirigami.Units.largeSpacing
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

        // ── Card 2: License ──────────────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: licenseLayout.implicitHeight + Kirigami.Units.mediumSpacing * 2
            color: Kirigami.Theme.alternateBackgroundColor
            border.color: aboutDialog.cardBorderColor
            border.width: 1
            radius: Kirigami.Units.smallSpacing * 2

            MouseArea {
                id: licenseMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: Qt.openUrlExternally("https://www.gnu.org/licenses/old-licenses/gpl-2.0.html")
            }

            Rectangle {
                anchors.fill: parent
                color: Kirigami.Theme.hoverColor
                opacity: licenseMouse.containsMouse ? 0.3 : 0.0
                radius: Kirigami.Units.smallSpacing * 2
                Behavior on opacity { NumberAnimation { duration: Kirigami.Units.shortDuration } }
            }

            RowLayout {
                id: licenseLayout
                anchors.fill: parent
                anchors.margins: Kirigami.Units.mediumSpacing * 1.5
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

        // ── Card 3: Action Links ─────────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: actionsLayout.implicitHeight + Kirigami.Units.largeSpacing * 2
            color: Kirigami.Theme.alternateBackgroundColor
            border.color: aboutDialog.cardBorderColor
            border.width: 1
            radius: Kirigami.Units.smallSpacing * 2

            ColumnLayout {
                id: actionsLayout
                anchors.fill: parent
                anchors.margins: Kirigami.Units.largeSpacing
                spacing: Kirigami.Units.mediumSpacing

                // Get Involved (GitHub Repo)
                Item {
                    Layout.fillWidth: true
                    implicitHeight: Kirigami.Units.gridUnit * 1.5

                    MouseArea {
                        id: gitMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: Qt.openUrlExternally("https://github.com/strandzen/AppImage-Manager")
                    }

                    RowLayout {
                        anchors.fill: parent
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
                            font.bold: gitMouse.containsMouse
                            Layout.fillWidth: true
                            Layout.alignment: Qt.AlignVCenter
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

                Kirigami.Separator { Layout.fillWidth: true }

                // Report a Bug (GitHub Issues)
                Item {
                    Layout.fillWidth: true
                    implicitHeight: Kirigami.Units.gridUnit * 1.5

                    MouseArea {
                        id: bugMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: Qt.openUrlExternally("https://github.com/strandzen/AppImage-Manager/issues")
                    }

                    RowLayout {
                        anchors.fill: parent
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
                            font.bold: bugMouse.containsMouse
                            Layout.fillWidth: true
                            Layout.alignment: Qt.AlignVCenter
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
        }

        // ── Headline: Libraries in use ───────────────────────────────────────
        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: Kirigami.Units.mediumSpacing

            Kirigami.Heading {
                text: i18n("Libraries in use")
                level: 3
                Layout.fillWidth: true
            }

            Controls.Button {
                icon.name: "edit-copy"
                text: i18n("Copy to Clipboard")
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

        // ── Card 4: Libraries ────────────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: libsLayout.implicitHeight + Kirigami.Units.largeSpacing * 2
            color: Kirigami.Theme.alternateBackgroundColor
            border.color: aboutDialog.cardBorderColor
            border.width: 1
            radius: Kirigami.Units.smallSpacing * 2

            ColumnLayout {
                id: libsLayout
                anchors.fill: parent
                anchors.margins: Kirigami.Units.largeSpacing
                spacing: Kirigami.Units.mediumSpacing

                // Qt
                RowLayout {
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.largeSpacing

                    ColumnLayout {
                        spacing: Kirigami.Units.smallSpacing
                        Layout.fillWidth: true

                        Controls.Label {
                            text: i18n("Qt %1", AppSettings.qtVersion)
                            font.bold: true
                        }
                        Controls.Label {
                            text: i18n("Cross-platform application development framework.")
                            opacity: 0.6
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                            font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                        }
                    }

                    Kirigami.Icon {
                        source: "internet-services"
                        fallback: "globe"
                        implicitWidth: Kirigami.Units.iconSizes.small
                        implicitHeight: Kirigami.Units.iconSizes.small
                        opacity: 0.6
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: Qt.openUrlExternally("https://www.qt.io/")
                        }
                    }
                }

                Kirigami.Separator { Layout.fillWidth: true }

                // KDE Frameworks
                RowLayout {
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.largeSpacing

                    ColumnLayout {
                        spacing: Kirigami.Units.smallSpacing
                        Layout.fillWidth: true

                        Controls.Label {
                            text: i18n("KDE Frameworks %1", AppSettings.kdeFrameworksVersion)
                            font.bold: true
                        }
                        Controls.Label {
                            text: i18n("Collection of libraries created by the KDE Community to extend Qt.")
                            opacity: 0.6
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                            font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                        }
                    }

                    Kirigami.Icon {
                        source: "internet-services"
                        fallback: "globe"
                        implicitWidth: Kirigami.Units.iconSizes.small
                        implicitHeight: Kirigami.Units.iconSizes.small
                        opacity: 0.6
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: Qt.openUrlExternally("https://kde.org/products/frameworks/")
                        }
                    }
                }

                Kirigami.Separator { Layout.fillWidth: true }

                // Metadata Extractor
                RowLayout {
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.largeSpacing

                    ColumnLayout {
                        spacing: Kirigami.Units.smallSpacing
                        Layout.fillWidth: true

                        Controls.Label {
                            text: AppSettings.libappimageVersion
                            font.bold: true
                        }
                        Controls.Label {
                            text: i18n("SquashFS extraction module used for reading AppImage configurations.")
                            opacity: 0.6
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                            font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                        }
                    }
                }
            }
        }

        // ── Headline: Authors ────────────────────────────────────────────────
        Kirigami.Heading {
            text: i18n("Contributors")
            level: 3
            Layout.topMargin: Kirigami.Units.mediumSpacing
        }

        // ── Card 5: Authors List ─────────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: authorsLayout.implicitHeight + Kirigami.Units.largeSpacing * 2
            color: Kirigami.Theme.alternateBackgroundColor
            border.color: aboutDialog.cardBorderColor
            border.width: 1
            radius: Kirigami.Units.smallSpacing * 2

            ColumnLayout {
                id: authorsLayout
                anchors.fill: parent
                anchors.margins: Kirigami.Units.largeSpacing
                spacing: Kirigami.Units.mediumSpacing

                Controls.BusyIndicator {
                    running: aboutDialog.isLoadingContributors
                    visible: running
                    Layout.alignment: Qt.AlignHCenter
                }

                Repeater {
                    model: aboutDialog.isLoadingContributors ? 0 : aboutDialog.contributors.length > 0 ? aboutDialog.contributors : aboutDialog.fallbackContributors
                    delegate: ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Kirigami.Units.smallSpacing

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Kirigami.Units.largeSpacing

                            // Avatar Circle
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

                            // Author Details
                            ColumnLayout {
                                spacing: 0
                                Layout.fillWidth: true
                                Layout.alignment: Qt.AlignVCenter

                                Controls.Label {
                                    text: modelData.name
                                    font.bold: true
                                }
                                Controls.Label {
                                    text: modelData.role
                                    opacity: 0.6
                                    font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                                }
                            }

                            // Action Icons
                            RowLayout {
                                spacing: Kirigami.Units.mediumSpacing

                                Kirigami.Icon {
                                    source: "mail-send"
                                    implicitWidth: Kirigami.Units.iconSizes.smallMedium
                                    implicitHeight: Kirigami.Units.iconSizes.smallMedium
                                    visible: modelData.email !== ""
                                    opacity: 0.6
                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: Qt.openUrlExternally("mailto:" + modelData.email)
                                    }
                                }

                                Kirigami.Icon {
                                    source: "internet-services"
                                    fallback: "globe"
                                    implicitWidth: Kirigami.Units.iconSizes.smallMedium
                                    implicitHeight: Kirigami.Units.iconSizes.smallMedium
                                    opacity: 0.6
                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: Qt.openUrlExternally(modelData.web)
                                    }
                                }
                            }
                        }

                        Kirigami.Separator {
                            Layout.fillWidth: true
                            visible: index < (aboutDialog.contributors.length > 0 ? aboutDialog.contributors.length : aboutDialog.fallbackContributors.length) - 1
                        }
                    }
                }
            }
        }
    }
}
}
