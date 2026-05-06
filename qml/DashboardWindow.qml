// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import appimagemanager

Kirigami.ApplicationWindow {
    id: root
    title: i18n("AppImage Manager")
    width: Kirigami.Units.gridUnit * 50
    height: Kirigami.Units.gridUnit * 30
    minimumWidth: Kirigami.Units.gridUnit * 40
    minimumHeight: Kirigami.Units.gridUnit * 22

    Kirigami.Theme.colorSet: Kirigami.Theme.Window
    Kirigami.Theme.inherit: false

    // ── Role constants (AppImageListModel::Roles, starting at Qt::UserRole) ───
    readonly property int roleFilePath:       Qt.UserRole + 0
    readonly property int roleCleanName:      Qt.UserRole + 1
    readonly property int roleAppName:        Qt.UserRole + 2
    readonly property int roleVersion:        Qt.UserRole + 3
    readonly property int roleIconSource:     Qt.UserRole + 4
    readonly property int roleHasDesktopLink: Qt.UserRole + 5
    readonly property int roleMetadataLoaded: Qt.UserRole + 6
    readonly property int roleAppSize:        Qt.UserRole + 7
    readonly property int roleFormattedSize:  Qt.UserRole + 8
    readonly property int roleDisplayName:    Qt.UserRole + 10
    readonly property int roleCategories:     Qt.UserRole + 16
    readonly property int roleComment:        Qt.UserRole + 17

    // ── Selected item snapshot ────────────────────────────────────────────────
    property var currentItem: ({})

    function refreshCurrentItemAt(idx) {
        if (idx < 0) { currentItem = {}; return }
        const midx = proxyModel.index(idx, 0)
        currentItem = {
            filePath:       proxyModel.data(midx, roleFilePath)       ?? "",
            displayName:    proxyModel.data(midx, roleDisplayName)    ?? "",
            iconSource:     proxyModel.data(midx, roleIconSource)     ?? "application-x-executable",
            version:        proxyModel.data(midx, roleVersion)        ?? "",
            formattedSize:  proxyModel.data(midx, roleFormattedSize)  ?? "",
            metadataLoaded: proxyModel.data(midx, roleMetadataLoaded) ?? false,
            comment:        proxyModel.data(midx, roleComment)        ?? "",
            categories:     proxyModel.data(midx, roleCategories)     ?? "",
        }
    }

    // ── Dialogs ───────────────────────────────────────────────────────────────
    FileDialog {
        id: installFileDialog
        title: i18n("Select AppImage to Install")
        nameFilters: [i18n("AppImage files (*.AppImage *.appimage)"), i18n("All files (*)")]
        onAccepted: dashboardController.installFromPath(selectedFile)
    }

    UninstallDialog { id: uninstallDialog }
    SettingsDialog  { id: settingsDialog  }

    StorageDialog {
        id: storageDialog
        onOpenInFileManager: (path) => dashboardController.openInFileManager(path)
    }

    UpdateDialog {
        id: updateDialog
        onDownloadRequested: proxyModel.downloadUpdate(updateDialog.proxyRow)
    }

    Connections {
        target: listModel
        function onOpenUninstallWindow(filePath) {
            uninstallDialog.backend = dashboardController.createUninstallBackend(filePath)
            uninstallDialog.open()
        }
    }

    // ── Main page ─────────────────────────────────────────────────────────────
    Component {
        id: mainPage

        Kirigami.Page {
            id: pageRoot

            title: i18n("Installed Applications")

            property bool listWidthSet: false

            actions: [
                Kirigami.Action {
                    icon.name: "view-sort-ascending"
                    text: i18n("Sort")
                    displayHint: Kirigami.DisplayHint.IconOnly

                    Controls.ActionGroup {
                        id: sortActionGroup
                    }

                    Kirigami.Action {
                        text: i18n("By Name")
                        checkable: true
                        checked: proxyModel.sortRole === 0
                        Controls.ActionGroup.group: sortActionGroup
                        onTriggered: proxyModel.sortRole = 0
                    }
                    Kirigami.Action {
                        text: i18n("By Size")
                        checkable: true
                        checked: proxyModel.sortRole === 1
                        Controls.ActionGroup.group: sortActionGroup
                        onTriggered: proxyModel.sortRole = 1
                    }
                    Kirigami.Action {
                        text: i18n("By Category")
                        checkable: true
                        checked: proxyModel.sortRole === 5
                        Controls.ActionGroup.group: sortActionGroup
                        onTriggered: proxyModel.sortRole = 5
                    }
                },
                Kirigami.Action {
                    icon.name: "view-refresh"
                    text: i18n("Check for Updates")
                    displayHint: Kirigami.DisplayHint.IconOnly
                    onTriggered: listModel.checkForUpdates()
                },
                Kirigami.Action {
                    icon.name: "configure"
                    text: i18n("Settings")
                    displayHint: Kirigami.DisplayHint.IconOnly
                    onTriggered: settingsDialog.open()
                }
            ]

            // ── Text metrics for dynamic left-pane width ──────────────────────
            TextMetrics {
                id: nameMetrics
                font: Kirigami.Theme.defaultFont
            }

            function recalcListWidth() {
                let maxW = Kirigami.Units.gridUnit * 8
                const n = listView.count
                for (let i = 0; i < n; i++) {
                    nameMetrics.text = proxyModel.data(proxyModel.index(i, 0), root.roleDisplayName) || ""
                    maxW = Math.max(maxW, nameMetrics.boundingRect.width)
                }
                leftPane.Controls.SplitView.preferredWidth = maxW + Kirigami.Units.gridUnit * 4
                pageRoot.listWidthSet = true
            }

            Connections {
                target: listModel
                function onScanningChanged() {
                    if (!listModel.scanning && !pageRoot.listWidthSet && listView.count > 0)
                        pageRoot.recalcListWidth()
                }
            }

            Connections {
                target: proxyModel
                function onDataChanged(topLeft, bottomRight) {
                    if (listView.currentIndex >= topLeft.row && listView.currentIndex <= bottomRight.row)
                        root.refreshCurrentItemAt(listView.currentIndex)
                }
                function onModelReset() {
                    pageRoot.listWidthSet = false
                    leftPane.Controls.SplitView.preferredWidth = Kirigami.Units.gridUnit * 14
                }
            }

            // ── BusyIndicator: scanning, no items yet ─────────────────────────
            Controls.BusyIndicator {
                anchors.centerIn: parent
                running: listModel.scanning && listView.count === 0 && proxyModel.filterText === ""
                visible: running
                z: 1
            }

            // ── Empty state: no apps installed ────────────────────────────────
            Item {
                anchors.fill: parent
                visible: !listModel.scanning && listView.count === 0 && proxyModel.filterText === ""

                DropArea {
                    id: emptyDropArea
                    anchors.fill: parent
                    keys: ["text/uri-list"]
                    onDropped: (drop) => {
                        for (const url of drop.urls) {
                            if (url.toString().toLowerCase().endsWith(".appimage"))
                                dashboardController.installFromPath(url)
                        }
                    }
                }

                Rectangle {
                    anchors.centerIn: parent
                    width:  Kirigami.Units.gridUnit * 22
                    height: Kirigami.Units.gridUnit * 13
                    color:  Kirigami.Theme.alternateBackgroundColor
                    radius: Kirigami.Units.smallSpacing * 2
                    border.color: Kirigami.Theme.focusColor
                    border.width: emptyDropArea.containsDrag ? 2 : 1
                    Behavior on border.width { NumberAnimation { duration: Kirigami.Units.shortDuration } }

                    ColumnLayout {
                        anchors.centerIn: parent
                        spacing: Kirigami.Units.largeSpacing

                        Kirigami.Icon {
                            source: "application-x-executable"
                            implicitWidth:  Kirigami.Units.iconSizes.huge
                            implicitHeight: Kirigami.Units.iconSizes.huge
                            Layout.alignment: Qt.AlignHCenter
                        }

                        Controls.Label {
                            text: i18n("Drag AppImage here to install")
                            Layout.alignment: Qt.AlignHCenter
                        }
                    }
                }
            }

            // ── Populated state: master-detail SplitView ──────────────────────
            Controls.SplitView {
                id: splitView
                anchors.fill: parent
                visible: listView.count > 0 || proxyModel.filterText !== ""
                orientation: Qt.Horizontal

                handle: Item {
                    implicitWidth: 8

                    Rectangle {
                        anchors.centerIn: parent
                        width: 1
                        height: parent.height
                        visible: listView.currentIndex >= 0
                        color: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.15)
                    }
                }

                // ── Left pane: master list ────────────────────────────────────
                Item {
                    id: leftPane
                    Controls.SplitView.preferredWidth: Kirigami.Units.gridUnit * 14
                    Controls.SplitView.minimumWidth:   Kirigami.Units.gridUnit * 8

                    Kirigami.Theme.colorSet: Kirigami.Theme.Window
                    Kirigami.Theme.inherit: false


                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 0

                        Kirigami.SearchField {
                            id: searchField
                            Layout.fillWidth: true
                            Layout.margins: Kirigami.Units.smallSpacing
                            placeholderText: i18n("Search…")
                            onTextChanged: proxyModel.filterText = text
                        }

                        Controls.ProgressBar {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 2
                            indeterminate: true
                            visible: listModel.scanning
                            padding: 0
                        }
                        Kirigami.Separator {
                            Layout.fillWidth: true
                            visible: !listModel.scanning
                        }

                        Item {
                            Layout.fillWidth: true
                            Layout.fillHeight: true

                            ListView {
                                id: listView
                                anchors.fill: parent
                                model: proxyModel
                                clip: true
                                currentIndex: -1

                                onCurrentIndexChanged: root.refreshCurrentItemAt(currentIndex)

                                TapHandler {
                                    onTapped: (eventPoint) => {
                                        const hit = listView.itemAt(
                                            eventPoint.position.x,
                                            eventPoint.position.y + listView.contentY)
                                        if (!hit)
                                            listView.currentIndex = -1
                                    }
                                }

                                add: Transition {
                                    NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Kirigami.Units.longDuration }
                                }
                                remove: Transition {
                                    NumberAnimation { property: "opacity"; to: 0; duration: Kirigami.Units.shortDuration }
                                }
                                displaced: Transition {
                                    NumberAnimation { properties: "y"; duration: Kirigami.Units.longDuration; easing.type: Easing.OutCubic }
                                }

                                delegate: Item {
                                    id: delegateRoot
                                    width: listView.width
                                    height: Kirigami.Units.gridUnit * 2.5

                                    readonly property bool isCurrent: ListView.isCurrentItem

                                    Rectangle {
                                        anchors.fill: parent
                                        anchors.margins: 2
                                        radius: Kirigami.Units.smallSpacing * 2
                                        color: delegateRoot.isCurrent
                                               ? Kirigami.Theme.highlightColor
                                               : delegateMouse.containsMouse
                                                 ? Kirigami.Theme.hoverColor
                                                 : index % 2 === 0
                                                   ? Kirigami.Theme.backgroundColor
                                                   : Kirigami.Theme.alternateBackgroundColor
                                        Behavior on color { ColorAnimation { duration: Kirigami.Units.shortDuration } }

                                        RowLayout {
                                            anchors {
                                                fill: parent
                                                leftMargin: Kirigami.Units.largeSpacing
                                                rightMargin: Kirigami.Units.smallSpacing
                                            }
                                            spacing: Kirigami.Units.smallSpacing

                                            Kirigami.Icon {
                                                source: model.iconSource
                                                implicitWidth:  Kirigami.Units.iconSizes.smallMedium
                                                implicitHeight: Kirigami.Units.iconSizes.smallMedium
                                                Layout.alignment: Qt.AlignVCenter
                                            }

                                            Controls.Label {
                                                text: model.displayName
                                                color: delegateRoot.isCurrent
                                                       ? Kirigami.Theme.highlightedTextColor
                                                       : Kirigami.Theme.textColor
                                                elide: Text.ElideRight
                                                Layout.fillWidth: true
                                                verticalAlignment: Text.AlignVCenter
                                            }

                                            Kirigami.Chip {
                                                readonly property string cat: (model.categories ?? "")
                                                    .split(";").filter(s => s.length > 0)[0] ?? ""
                                                text: cat
                                                visible: listView.currentIndex < 0 && model.metadataLoaded && cat !== ""
                                                closable: false
                                                checkable: false
                                                Layout.alignment: Qt.AlignVCenter
                                            }

                                            Kirigami.Chip {
                                                text: model.version
                                                visible: listView.currentIndex < 0 && model.metadataLoaded && model.version !== ""
                                                closable: false
                                                checkable: false
                                                Layout.alignment: Qt.AlignVCenter
                                            }

                                            Kirigami.Chip {
                                                text: model.formattedSize
                                                visible: listView.currentIndex < 0 && model.metadataLoaded && model.appSize > 0
                                                closable: false
                                                checkable: false
                                                Layout.alignment: Qt.AlignVCenter
                                            }

                                            Controls.BusyIndicator {
                                                implicitWidth:  Kirigami.Units.iconSizes.small
                                                implicitHeight: Kirigami.Units.iconSizes.small
                                                running: !model.metadataLoaded
                                                visible: running
                                            }
                                        }
                                    }

                                    MouseArea {
                                        id: delegateMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        onClicked: listView.currentIndex = index
                                    }
                                }
                            }

                            Kirigami.PlaceholderMessage {
                                anchors.centerIn: parent
                                visible: listView.count === 0 && !listModel.scanning && proxyModel.filterText !== ""
                                icon.name: "edit-find"
                                text: i18n("No results for \"%1\"", proxyModel.filterText)
                            }
                        }
                    }
                }

                // ── Right pane: detail view ───────────────────────────────────
                Item {
                    id: rightPane
                    Controls.SplitView.fillWidth: true
                    Controls.SplitView.minimumWidth: Kirigami.Units.gridUnit * 15
                    visible: listView.currentIndex >= 0

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: Kirigami.Units.gridUnit
                        spacing: Kirigami.Units.largeSpacing

                        Kirigami.Icon {
                            source: root.currentItem.iconSource ?? "application-x-executable"
                            implicitWidth:  Kirigami.Units.iconSizes.enormous
                            implicitHeight: Kirigami.Units.iconSizes.enormous
                            Layout.alignment: Qt.AlignHCenter
                            Layout.topMargin: Kirigami.Units.gridUnit
                        }

                        Kirigami.Heading {
                            text: root.currentItem.displayName ?? ""
                            level: 2
                            horizontalAlignment: Text.AlignHCenter
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }

                        Controls.Label {
                            text: root.currentItem.comment ?? ""
                            visible: (root.currentItem.metadataLoaded ?? false)
                                     && (root.currentItem.comment ?? "") !== ""
                            horizontalAlignment: Text.AlignHCenter
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            opacity: 0.7
                        }

                        RowLayout {
                            id: chipsRow
                            Layout.alignment: Qt.AlignHCenter
                            spacing: Kirigami.Units.smallSpacing
                            visible: root.currentItem.metadataLoaded ?? false

                            Kirigami.Chip {
                                text: i18n("Version: %1", root.currentItem.version || "—")
                                closable: false
                                checkable: false
                            }

                            Kirigami.Chip {
                                text: i18n("Size: %1", root.currentItem.formattedSize || "—")
                                closable: false
                                checkable: false
                            }

                            Kirigami.Chip {
                                readonly property string cat: (root.currentItem.categories ?? "")
                                    .split(";").filter(s => s.length > 0)[0] ?? ""
                                text: cat
                                visible: (root.currentItem.metadataLoaded ?? false) && cat !== ""
                                closable: false
                                checkable: false
                            }
                        }

                        Item { Layout.fillHeight: true }

                        RowLayout {
                            Layout.alignment: Qt.AlignHCenter
                            Layout.bottomMargin: Kirigami.Units.gridUnit
                            Layout.preferredWidth: chipsRow.implicitWidth
                            spacing: Kirigami.Units.smallSpacing

                            Controls.Button {
                                text: i18n("Update")
                                icon.name: "system-software-update"
                                enabled: false
                                Layout.fillWidth: true
                            }

                            Controls.Button {
                                text: i18n("Remove")
                                icon.name: "edit-delete"
                                onClicked: proxyModel.requestRemoveAt(listView.currentIndex)
                                Layout.fillWidth: true
                            }
                        }
                    }
                }
            }
        }
    }

    pageStack.initialPage: mainPage

    // ── Global drag-and-drop install overlay ──────────────────────────────────
    DropArea {
        id: globalDropArea
        anchors.fill: parent
        keys: ["text/uri-list"]

        onDropped: (drop) => {
            for (const url of drop.urls) {
                if (url.toString().toLowerCase().endsWith(".appimage"))
                    dashboardController.installFromPath(url)
            }
        }

        Rectangle {
            anchors.fill: parent
            color: Kirigami.Theme.highlightColor
            opacity: globalDropArea.containsDrag ? 0.15 : 0.0
            Behavior on opacity { NumberAnimation { duration: Kirigami.Units.shortDuration } }
        }

        Kirigami.Heading {
            anchors.centerIn: parent
            level: 2
            text: i18n("Drop to install")
            visible: globalDropArea.containsDrag
            color: Kirigami.Theme.highlightedTextColor
        }
    }
}
