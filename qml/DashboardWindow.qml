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
    title: i18n("AppImage Dashboard")
    width: Kirigami.Units.gridUnit * 50
    height: Kirigami.Units.gridUnit * 30
    minimumWidth: Kirigami.Units.gridUnit * 40
    minimumHeight: Kirigami.Units.gridUnit * 22

    Kirigami.Theme.colorSet: Kirigami.Theme.Window
    Kirigami.Theme.inherit: false

    readonly property color cardBorderColor: Theme.cardBorderColor

    function handleDrop(drop) {
        for (const url of drop.urls) {
            if (url.toString().toLowerCase().endsWith(".appimage"))
                dashboardController.installFromPath(url)
        }
    }

    // ── Selected item snapshot ────────────────────────────────────────────────
    property var currentItem: ({})

    function refreshCurrentItemAt(idx) {
        if (idx < 0) { currentItem = {}; return }
        // One C++ call returns every role keyed by name; the previous form
        // crossed the QML/C++ boundary 18 times per selection change.
        currentItem = proxyModel.itemData(idx)
    }

    // ── Update check result ───────────────────────────────────────────────────
    Connections {
        target: listModel
        function onUpdateCheckFinished(updatesFound, networkFailures) {
            if (updatesFound < 0)
                checkTimedOutDialog.open()
            else if (networkFailures > 0 && updatesFound === 0)
                networkErrorDialog.open()
            else if (updatesFound === 0)
                noUpdatesDialog.open()
        }
    }

    Kirigami.PromptDialog {
        id: noUpdatesDialog
        title: i18n("No Updates Found")
        subtitle: i18n("All your AppImages are up to date.")
        standardButtons: Kirigami.Dialog.Ok
        Kirigami.Theme.colorSet: Kirigami.Theme.Window
        Kirigami.Theme.inherit: false
    }

    Kirigami.PromptDialog {
        id: checkTimedOutDialog
        title: i18n("Update Check Failed")
        subtitle: i18n("The update check timed out. Check your network connection and try again.")
        standardButtons: Kirigami.Dialog.Ok
        Kirigami.Theme.colorSet: Kirigami.Theme.Window
        Kirigami.Theme.inherit: false
    }

    Kirigami.PromptDialog {
        id: networkErrorDialog
        title: i18n("Network Error")
        subtitle: i18n("Could not check for updates. Verify your network connection and try again.")
        standardButtons: Kirigami.Dialog.Ok
        Kirigami.Theme.colorSet: Kirigami.Theme.Window
        Kirigami.Theme.inherit: false
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
    AboutDialog     { id: aboutDialog     }

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
    pageStack.initialPage: Kirigami.Page {
        id: pageRoot

            title: {
                const total = listModel.rowCount()
                if (proxyModel.filterText !== "" && listView.count !== total)
                    return i18n("Installed Applications (%1 of %2)", listView.count, total)
                return total > 0 ? i18n("Installed Applications (%1)", total) : i18n("Installed Applications")
            }

            actions: [
                Kirigami.Action {
                    icon.name: "view-sort-ascending"
                    text: i18n("Sort")
                    displayHint: Kirigami.DisplayHint.IconOnly


                    Kirigami.Action {
                        text: i18n("By Name")
                        checkable: true
                        checked: proxyModel.sortField === AppImageSortFilterModel.SortByName
                        onTriggered: proxyModel.sortField = AppImageSortFilterModel.SortByName
                    }
                    Kirigami.Action {
                        text: i18n("By Size")
                        checkable: true
                        checked: proxyModel.sortField === AppImageSortFilterModel.SortBySize
                        onTriggered: proxyModel.sortField = AppImageSortFilterModel.SortBySize
                    }
                    Kirigami.Action {
                        text: i18n("By Category")
                        checkable: true
                        checked: proxyModel.sortField === AppImageSortFilterModel.SortByCategory
                        onTriggered: proxyModel.sortField = AppImageSortFilterModel.SortByCategory
                    }
                    Kirigami.Action {
                        text: i18n("By Date Added")
                        checkable: true
                        checked: proxyModel.sortField === AppImageSortFilterModel.SortByDate
                        onTriggered: proxyModel.sortField = AppImageSortFilterModel.SortByDate
                    }
                },
                Kirigami.Action {
                    icon.name: listModel.checkingUpdates ? "process-working" : "view-refresh"
                    text: listModel.checkingUpdates ? i18n("Checking…") : i18n("Check for Updates")
                    displayHint: Kirigami.DisplayHint.IconOnly
                    enabled: !listModel.checkingUpdates
                    onTriggered: listModel.checkForUpdates()
                },
                Kirigami.Action {
                    icon.name: "configure"
                    text: i18n("Settings")
                    displayHint: Kirigami.DisplayHint.IconOnly
                    onTriggered: settingsDialog.open()
                },
                Kirigami.Action {
                    icon.name: "help-about"
                    text: i18n("About")
                    displayHint: Kirigami.DisplayHint.IconOnly
                    onTriggered: aboutDialog.open()
                }
            ]

            Connections {
                target: proxyModel
                function onDataChanged(topLeft, bottomRight) {
                    if (listView.currentIndex >= topLeft.row && listView.currentIndex <= bottomRight.row)
                        root.refreshCurrentItemAt(listView.currentIndex)
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
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Kirigami.Units.gridUnit * 2
                spacing: Kirigami.Units.largeSpacing
                visible: !listModel.scanning && listView.count === 0 && proxyModel.filterText === ""

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    DropArea {
                        id: emptyDropArea
                        anchors.fill: parent
                        keys: ["text/uri-list"]
                        onDropped: (drop) => root.handleDrop(drop)
                    }

                    Rectangle {
                        anchors.fill: parent
                        color:  Kirigami.Theme.alternateBackgroundColor
                        radius: Kirigami.Units.smallSpacing * 2
                        border.color: root.cardBorderColor
                        border.width: globalDropArea.containsDrag ? 2 : 1
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

                Kirigami.InlineMessage {
                    Layout.fillWidth: true
                    visible: AppSettings.showDisclaimer
                    type: Kirigami.MessageType.Warning
                    text: i18n("AppImages are unverified executables. Only install from sources you trust.")
                    showCloseButton: false
                }
            }

            // ── Populated state: master-detail ────────────────────────────────
            RowLayout {
                id: masterDetailRow
                anchors.fill: parent
                spacing: 0
                visible: listView.count > 0 || proxyModel.filterText !== ""

                // ── Left pane: master list ────────────────────────────────────
                Item {
                    id: leftPane
                    Layout.fillHeight: true
                    // When nothing selected, fill the entire row; when selected, fixed width.
                    Layout.fillWidth: listView.currentIndex < 0
                    Layout.preferredWidth: Kirigami.Units.gridUnit * 18

                    Kirigami.Theme.colorSet: Kirigami.Theme.Window
                    Kirigami.Theme.inherit: false

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 0

                        Kirigami.SearchField {
                            id: searchField
                            Layout.fillWidth: true
                            Layout.leftMargin: Kirigami.Units.smallSpacing
                            Layout.rightMargin: Kirigami.Units.smallSpacing
                            Layout.bottomMargin: Kirigami.Units.smallSpacing
                            placeholderText: i18n("Search")
                            onTextChanged: proxyModel.filterText = text
                        }

                        Controls.ProgressBar {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 2
                            indeterminate: true
                            visible: listModel.scanning
                            padding: 0
                        }

                        Controls.Label {
                            Layout.fillWidth: true
                            Layout.rightMargin: Kirigami.Units.smallSpacing
                            visible: listModel.pendingLoads > 0
                            text: i18n("Loading %1…", listModel.pendingLoads)
                            font: Kirigami.Theme.smallFont
                            horizontalAlignment: Text.AlignRight
                            color: Kirigami.Theme.disabledTextColor
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

                                // Animations are noisy during the initial scan (N inserts → N
                                // simultaneous transitions). Gate add/displaced on !scanning so
                                // bulk fills snap into place; single user-driven adds still animate
                                // once scanning settles. Remove stays animated regardless.
                                add: Transition {
                                    enabled: !listModel.scanning
                                    NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Kirigami.Units.longDuration }
                                }
                                remove: Transition {
                                    NumberAnimation { property: "opacity"; to: 0; duration: Kirigami.Units.shortDuration }
                                }
                                displaced: Transition {
                                    enabled: !listModel.scanning
                                    NumberAnimation { properties: "y"; duration: Kirigami.Units.longDuration; easing.type: Easing.OutCubic }
                                }

                                delegate: Controls.ItemDelegate {
                                    id: delegateRoot
                                    width: listView.width
                                    height: Kirigami.Units.gridUnit * 2.5
                                    highlighted: ListView.isCurrentItem
                                    horizontalPadding: Kirigami.Units.smallSpacing
                                    // Plasma style applies position-aware insets — last item gets
                                    // a larger bottomInset, making its highlight appear squashed.
                                    // Pin vertical insets to largeSpacing/2 so every row renders
                                    // identically; verticalPadding matches so content fills the
                                    // background area and centers via Qt.AlignVCenter.
                                    topInset: Kirigami.Units.largeSpacing / 2
                                    bottomInset: Kirigami.Units.largeSpacing / 2
                                    leftInset: 0
                                    rightInset: 0
                                    verticalPadding: Kirigami.Units.largeSpacing / 2

                                    // listView.currentIndex is out of scope inside inline
                                    // contentItem bindings — read it through ListView.view here.
                                    readonly property bool noSelection: ListView.view.currentIndex < 0

                                    contentItem: RowLayout {
                                        spacing: Kirigami.Units.smallSpacing

                                        Kirigami.Icon {
                                            source: model.iconSource
                                            implicitWidth:  Kirigami.Units.iconSizes.smallMedium
                                            implicitHeight: Kirigami.Units.iconSizes.smallMedium
                                            Layout.alignment: Qt.AlignVCenter
                                        }

                                        Controls.Label {
                                            text: model.displayName
                                            color: delegateRoot.highlighted
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
                                            visible: (delegateRoot.noSelection || proxyModel.sortField === AppImageSortFilterModel.SortByCategory) && model.metadataLoaded && cat !== ""
                                            closable: false
                                            checkable: false
                                            Layout.alignment: Qt.AlignVCenter
                                        }

                                        Kirigami.Chip {
                                            text: model.version
                                            visible: delegateRoot.noSelection && model.metadataLoaded && model.version !== ""
                                            closable: false
                                            checkable: false
                                            Layout.alignment: Qt.AlignVCenter
                                        }

                                        Kirigami.Chip {
                                            text: model.formattedSize
                                            visible: (delegateRoot.noSelection || proxyModel.sortField === AppImageSortFilterModel.SortBySize) && model.metadataLoaded && model.appSize > 0
                                            closable: false
                                            checkable: false
                                            Layout.alignment: Qt.AlignVCenter
                                        }

                                        Kirigami.Chip {
                                            readonly property var d: model.addedDate
                                            text: d ? Qt.formatDate(d, "d MMM yyyy") : ""
                                            visible: proxyModel.sortField === AppImageSortFilterModel.SortByDate && model.metadataLoaded && !!d
                                            closable: false
                                            checkable: false
                                            Layout.alignment: Qt.AlignVCenter
                                        }

                                        Kirigami.Icon {
                                            source: "software-update-available"
                                            implicitWidth:  Kirigami.Units.iconSizes.small
                                            implicitHeight: Kirigami.Units.iconSizes.small
                                            color: Kirigami.Theme.positiveTextColor
                                            visible: model.updateAvailable && !model.isUpdating
                                            Layout.alignment: Qt.AlignVCenter
                                            HoverHandler { id: updateIconHover }
                                            Controls.ToolTip.text: i18n("Update available: %1", model.updateVersion)
                                            Controls.ToolTip.visible: updateIconHover.hovered
                                            Controls.ToolTip.delay: Kirigami.Units.toolTipDelay
                                        }

                                        Controls.BusyIndicator {
                                            implicitWidth:  Kirigami.Units.iconSizes.small
                                            implicitHeight: Kirigami.Units.iconSizes.small
                                            running: !model.metadataLoaded || model.isUpdating
                                            visible: running
                                        }
                                    }
                                    onClicked: listView.currentIndex = index
                                }

                                footer: Component {
                                    Item {
                                        width: listView.width
                                        readonly property real remaining: listView.height - listView.count * (Kirigami.Units.gridUnit * 2.5)
                                        height: AppSettings.showInstallBox
                                                ? Math.max(Kirigami.Units.gridUnit * 8, remaining)
                                                : 0
                                        clip: true

                                        DropArea {
                                            id: listFooterDropArea
                                            anchors.fill: parent
                                            keys: ["text/uri-list"]
                                            onDropped: (drop) => root.handleDrop(drop)
                                        }

                                        Rectangle {
                                            anchors.fill: parent
                                            anchors.topMargin: Kirigami.Units.largeSpacing
                                            anchors.leftMargin: Kirigami.Units.smallSpacing
                                            anchors.rightMargin: Kirigami.Units.smallSpacing
                                            anchors.bottomMargin: Kirigami.Units.smallSpacing
                                            visible: AppSettings.showInstallBox
                                            color: Kirigami.Theme.alternateBackgroundColor
                                            radius: Kirigami.Units.smallSpacing * 2
                                            border.color: root.cardBorderColor
                                            border.width: globalDropArea.containsDrag ? 2 : 1
                                            opacity: globalDropArea.containsDrag ? 1.0 : 0.5
                                            Behavior on border.width { NumberAnimation { duration: Kirigami.Units.shortDuration } }
                                            Behavior on opacity    { NumberAnimation { duration: Kirigami.Units.shortDuration } }

                                            ColumnLayout {
                                                anchors.centerIn: parent
                                                spacing: Kirigami.Units.smallSpacing
                                                Kirigami.Icon {
                                                    source: "list-add"
                                                    implicitWidth:  Kirigami.Units.iconSizes.medium
                                                    implicitHeight: Kirigami.Units.iconSizes.medium
                                                    Layout.alignment: Qt.AlignHCenter
                                                }
                                                Controls.Label {
                                                    text: i18n("Drop to install")
                                                    Layout.alignment: Qt.AlignHCenter
                                                    font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                                                }
                                            }
                                        }
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

                        Kirigami.InlineMessage {
                            Layout.fillWidth: true
                            Layout.leftMargin: Kirigami.Units.smallSpacing
                            Layout.rightMargin: Kirigami.Units.smallSpacing
                            Layout.topMargin: Kirigami.Units.smallSpacing
                            visible: AppSettings.showInstallBox && AppSettings.showDisclaimer
                            type: Kirigami.MessageType.Warning
                            text: i18n("AppImages are unverified executables. Only install from sources you trust.")
                            showCloseButton: false
                        }
                    }
                }

                // ── Right pane: detail view ───────────────────────────────────
                Item {
                    id: rightPane
                    visible: listView.currentIndex >= 0
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 0

                        // Scrollable content — use detailScrollView.availableWidth
                        // (not parent.width) to get the actual viewport width so
                        // content doesn't clip and wrapping works correctly.
                        Controls.ScrollView {
                            id: detailScrollView
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            Layout.leftMargin: Kirigami.Units.largeSpacing
                            Layout.rightMargin: Kirigami.Units.largeSpacing
                            clip: true
                            Controls.ScrollBar.horizontal.policy: Controls.ScrollBar.AlwaysOff
                            Controls.ScrollBar.vertical.policy: Controls.ScrollBar.AlwaysOff

                            ColumnLayout {
                                width: detailScrollView.availableWidth
                                spacing: 0

                                // Loading indicator while metadata is in-flight
                                Controls.BusyIndicator {
                                    Layout.alignment: Qt.AlignHCenter
                                    Layout.topMargin: Kirigami.Units.gridUnit * 3
                                    running: !(root.currentItem.metadataLoaded ?? false)
                                    visible: running
                                }

                                // Hero: icon + name + developer + chips
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    Layout.topMargin: Kirigami.Units.largeSpacing
                                    spacing: Kirigami.Units.smallSpacing
                                    visible: root.currentItem.metadataLoaded ?? false

                                    Kirigami.Icon {
                                        source: root.currentItem.iconSource ?? "application-x-executable"
                                        implicitWidth: Kirigami.Units.iconSizes.enormous
                                        implicitHeight: Kirigami.Units.iconSizes.enormous
                                        Layout.alignment: Qt.AlignHCenter
                                    }

                                    Kirigami.Heading {
                                        text: root.currentItem.displayName ?? ""
                                        level: 2
                                        horizontalAlignment: Text.AlignHCenter
                                        Layout.fillWidth: true
                                        elide: Text.ElideRight
                                    }

                                    RowLayout {
                                        Layout.alignment: Qt.AlignHCenter
                                        spacing: Kirigami.Units.smallSpacing
                                        visible: (root.currentItem.developerName ?? "") !== ""

                                        Controls.Label {
                                            text: root.currentItem.developerName ?? ""
                                            opacity: 0.6
                                            font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                                        }

                                        Controls.Label {
                                            readonly property string host: {
                                                const url = root.currentItem.homepage ?? ""
                                                if (!url) return ""
                                                const m = url.match(/^https?:\/\/([^/]+)/)
                                                return m ? m[1] : url
                                            }
                                            text: "<a href='" + (root.currentItem.homepage ?? "") + "'>" + host + "</a>"
                                            visible: host !== ""
                                            textFormat: Text.StyledText
                                            font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                                            onLinkActivated: Qt.openUrlExternally(root.currentItem.homepage)
                                            HoverHandler { cursorShape: Qt.PointingHandCursor }
                                        }
                                    }

                                    // Metadata chips — centered relative to description card width
                                    Flow {
                                        Layout.alignment: Qt.AlignHCenter
                                        Layout.topMargin: Kirigami.Units.smallSpacing
                                        spacing: Kirigami.Units.smallSpacing

                                        Kirigami.Chip {
                                            text: root.currentItem.version ?? ""
                                            visible: (root.currentItem.version ?? "") !== ""
                                            closable: false; checkable: false
                                        }
                                        Kirigami.Chip {
                                            text: root.currentItem.formattedSize ?? ""
                                            visible: (root.currentItem.formattedSize ?? "") !== ""
                                            closable: false; checkable: false
                                        }
                                        Kirigami.Chip {
                                            readonly property string cat: (root.currentItem.categories ?? "")
                                                .split(";").filter(s => s.length > 0)[0] ?? ""
                                            text: cat
                                            visible: cat !== ""
                                            closable: false; checkable: false
                                        }
                                        Kirigami.Chip {
                                            readonly property var d: root.currentItem.addedDate
                                            text: d ? Qt.formatDate(d, "d MMM yyyy") : ""
                                            visible: !!d
                                            closable: false; checkable: false
                                        }
                                    }
                                }

                                // Description card — height capped so the checkbox below stays
                                // visible near the action bar without excessive scrolling.
                                // Text overflows inside the card's own ScrollView.
                                Kirigami.Card {
                                    Layout.fillWidth: true
                                    Layout.topMargin: Kirigami.Units.largeSpacing
                                    // Scale down to ~separator line: subtract hero + checkbox + margins
                                    Layout.maximumHeight: Math.max(
                                        Kirigami.Units.gridUnit * 8,
                                        detailScrollView.availableHeight - Kirigami.Units.gridUnit * 15
                                    )
                                    visible: (root.currentItem.metadataLoaded ?? false)
                                             && (root.currentItem.description ?? "") !== ""

                                    contentItem: ColumnLayout {
                                        spacing: Kirigami.Units.smallSpacing

                                        Kirigami.Heading {
                                            text: i18n("About")
                                            level: 4
                                        }

                                        Controls.ScrollView {
                                            id: descriptionScroller
                                            Layout.fillWidth: true
                                            Layout.fillHeight: true
                                            clip: true
                                            Controls.ScrollBar.horizontal.policy: Controls.ScrollBar.AlwaysOff

                                            Controls.Label {
                                                width: descriptionScroller.availableWidth
                                                text: root.currentItem.description ?? ""
                                                wrapMode: Text.WordWrap
                                                opacity: 0.85
                                                textFormat: Text.StyledText
                                            }
                                        }
                                    }
                                }

                                // "Show in app menu" — position 2 when description fits,
                                // position 3 below the card when description is large.
                                Controls.CheckBox {
                                    text: i18n("Show in app menu")
                                    checked: root.currentItem.hasDesktopLink ?? false
                                    enabled: root.currentItem.metadataLoaded ?? false
                                    visible: root.currentItem.metadataLoaded ?? false
                                    Layout.alignment: Qt.AlignHCenter
                                    Layout.topMargin: Kirigami.Units.smallSpacing
                                    Layout.bottomMargin: Kirigami.Units.largeSpacing
                                    onToggled: proxyModel.toggleDesktopLink(listView.currentIndex, checked)
                                }

                                // Bottom padding for scroll overshoot
                                Item { height: Kirigami.Units.gridUnit }
                            }
                        }

                        // Sticky bottom action bar
                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.margins: Kirigami.Units.largeSpacing
                            spacing: Kirigami.Units.smallSpacing

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: Kirigami.Units.smallSpacing

                                Controls.Button {
                                    text: i18n("Launch")
                                    icon.name: "media-playback-start"
                                    onClicked: proxyModel.requestLaunch(listView.currentIndex)
                                    Layout.fillWidth: true
                                }

                                Controls.Button {
                                    text: (root.currentItem.isUpdating ?? false)
                                          ? i18n("Updating…")
                                          : i18n("Update")
                                    icon.name: "system-software-update"
                                    enabled: (root.currentItem.updateAvailable ?? false)
                                             && !(root.currentItem.isUpdating ?? false)
                                    Layout.fillWidth: true
                                    Controls.ToolTip.text: (root.currentItem.updateAvailable ?? false)
                                        ? i18n("Update to version %1", root.currentItem.updateVersion ?? "")
                                        : i18n("No update available")
                                    Controls.ToolTip.visible: hovered
                                    Controls.ToolTip.delay: Kirigami.Units.toolTipDelay
                                    onClicked: {
                                        updateDialog.proxyRow       = listView.currentIndex
                                        updateDialog.appName        = root.currentItem.displayName ?? ""
                                        updateDialog.currentVersion = root.currentItem.version ?? ""
                                        updateDialog.newVersion     = root.currentItem.updateVersion ?? ""
                                        updateDialog.open()
                                    }
                                }

                                Controls.Button {
                                    text: i18n("Remove")
                                    icon.name: "trash-empty"
                                    onClicked: proxyModel.requestRemoveAt(listView.currentIndex)
                                    Layout.fillWidth: true
                                }
                            }

                            Controls.ProgressBar {
                                Layout.fillWidth: true
                                visible: root.currentItem.isUpdating ?? false
                                value: (root.currentItem.updateProgress ?? 0) / 100.0
                            }
                        }
                    }
                }
            }

            // ── Keyboard shortcuts ────────────────────────────────────────────────
            Shortcut {
                sequence: "Return"
                enabled: (typeof listView !== 'undefined') && listView.currentIndex >= 0
                onActivated: proxyModel.requestLaunch(listView.currentIndex)
            }
            Shortcut {
                sequence: "Delete"
                enabled: (typeof listView !== 'undefined') && listView.currentIndex >= 0
                onActivated: proxyModel.requestRemoveAt(listView.currentIndex)
            }
            Shortcut {
                sequence: "Escape"
                enabled: searchField.text !== ""
                onActivated: { searchField.text = ""; proxyModel.filterText = "" }
            }
            Shortcut {
                sequence: "Escape"
                enabled: searchField.text === "" && (typeof listView !== 'undefined') && listView.currentIndex >= 0
                onActivated: listView.currentIndex = -1
            }
        }

    Shortcut {
        sequence: "Ctrl+F"
        onActivated: {
            // Find searchField inside the current page if it exists
            const page = pageStack.currentItem
            if (page && page.searchField) {
                page.searchField.forceActiveFocus()
            }
        }
    }



    // ── Global drag-and-drop install overlay ──────────────────────────────────
    DropArea {
        id: globalDropArea
        anchors.fill: parent
        keys: ["text/uri-list"]

        onDropped: (drop) => root.handleDrop(drop)

        readonly property bool noInnerBox: (proxyModel.rowCount() > 0 || proxyModel.filterText !== "") && !AppSettings.showInstallBox

        Rectangle {
            anchors.fill: parent
            color: Kirigami.Theme.highlightColor
            opacity: globalDropArea.containsDrag && parent.noInnerBox ? 0.15 : 0.0
            Behavior on opacity { NumberAnimation { duration: Kirigami.Units.shortDuration } }
        }

        Kirigami.Heading {
            anchors.centerIn: parent
            level: 2
            text: i18n("Drop to install")
            visible: globalDropArea.containsDrag && parent.noInnerBox
            color: Kirigami.Theme.highlightedTextColor
        }
    }
}
