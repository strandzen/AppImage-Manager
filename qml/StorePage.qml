// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import appimagemanager

Kirigami.Page {
    id: pageRoot

    property alias searchField: searchField

    padding: 0

    // Lazy init — trigger once when this page is first visible with Discover enabled
    // (Discover is opt-in: no catalog network traffic until the user enables it).
    property bool storeInitialized: false
    function initializeIfNeeded() {
        if (visible && AppSettings.discoverEnabled && !storeInitialized) {
            storeInitialized = true
            amStoreModel.initialize()
        }
    }
    onVisibleChanged: initializeIfNeeded()
    Connections {
        target: AppSettings
        function onDiscoverEnabledChanged() { pageRoot.initializeIfNeeded() }
    }

    Kirigami.PlaceholderMessage {
        anchors.centerIn: parent
        width: parent.width - Kirigami.Units.gridUnit * 4
        visible: !AppSettings.discoverEnabled
        icon.name: "plasma-discover"
        text: i18n("Discover is turned off")
        explanation: i18n("Discover lets you search app catalogs such as AppImage Hub and install apps from them. It downloads the catalogs from the internet, so it stays off until you turn it on.")
        helpfulAction: Kirigami.Action {
            icon.name: "checkmark"
            text: i18n("Turn On Discover")
            onTriggered: AppSettings.discoverEnabled = true
        }
    }

    // Snapshot of the currently selected item for the detail pane
    property var currentItem: ({})
    // What the detail pane is currently rendering (trails currentItem with a fade)
    property var displayedItem: ({})

    function refreshCurrentItemAt(idx) {
        if (idx < 0) { currentItem = {}; return }
        currentItem = amStoreModel.itemData(idx)
    }

    // ── Filters ───────────────────────────────────────────────────────────
    readonly property bool categoryActive: amStoreModel.categoryFilter !== "" && amStoreModel.categoryFilter !== "All"
    readonly property string sourceName: {
        const sources = amStoreModel.sources
        const i = amStoreModel.storeSource - 1
        return i >= 0 && i < sources.length ? sources[i].name : ""
    }
    readonly property bool filtersActive: categoryActive || sourceName !== ""

    function clearFilters() {
        amStoreModel.categoryFilter = ""
        amStoreModel.storeSource = 0
    }

    function iconFor(source) {
        return source && source !== "" ? source : "application-x-executable"
    }

    // ── Installation ──────────────────────────────────────────────────────
    ListModel { id: logModel }

    property string activePackage: ""
    property bool showLog: false
    readonly property string statusLine: logModel.count > 0 ? logModel.get(logModel.count - 1).text : ""

    function install(viaDownload) {
        logModel.clear()
        showLog = false
        activePackage = displayedItem.packageName
        amStoreModel.installApp(displayedItem.packageName, viaDownload)
    }

    Connections {
        target: amStoreModel
        // Filtering resets the model; the old row index would point at another app.
        function onModelReset() { listView.currentIndex = -1 }
        function onLogReceived(packageName, line) {
            if (packageName === pageRoot.activePackage) {
                logModel.append({ "text": line })
                if (logModel.count > 500)
                    logModel.remove(0, logModel.count - 500)
            }
        }
        function onInstallationFinished(packageName, success) {
            if (packageName === pageRoot.activePackage) {
                logModel.append({ "text": success
                    ? i18n("Installation completed.")
                    : i18n("Installation failed.") })
                if (success) {
                    showPassiveNotification(i18n("Installed %1", pageRoot.displayedItem.displayName ?? ""))
                } else {
                    pageRoot.showLog = true
                    showPassiveNotification(i18n("Could not install %1", pageRoot.displayedItem.displayName ?? ""))
                }
            }
        }
    }

    // ── Detail pane fade transition ────────────────────────────────────────
    SequentialAnimation {
        id: detailTransition
        NumberAnimation {
            target: detailInner; property: "opacity"
            to: 0; duration: 80; easing.type: Easing.OutQuad
        }
        ScriptAction { script: { pageRoot.displayedItem = pageRoot.currentItem } }
        NumberAnimation {
            target: detailInner; property: "opacity"
            to: 1; duration: 150; easing.type: Easing.InQuad
        }
    }

    // ── Master-detail row ─────────────────────────────────────────────────
    RowLayout {
        id: masterDetailRow
        visible: AppSettings.discoverEnabled
        anchors.fill: parent
        spacing: 0

        // ── Left pane ──────────────────────────────────────────────────────
        // List pane: the View color set sets it apart from the details, like a sidebar.
        Rectangle {
            id: leftPane
            Layout.fillHeight: true
            Layout.fillWidth: listView.currentIndex < 0
            Layout.preferredWidth: Kirigami.Units.gridUnit * 16
            Kirigami.Theme.colorSet: Kirigami.Theme.View
            Kirigami.Theme.inherit: false
            color: Kirigami.Theme.backgroundColor

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                // Search + one filter menu (sort, source, category)
                RowLayout {
                    Layout.fillWidth: true
                    Layout.margins: Kirigami.Units.smallSpacing
                    spacing: Kirigami.Units.smallSpacing

                    Kirigami.SearchField {
                        id: searchField
                        Layout.fillWidth: true
                        placeholderText: i18n("Search apps")
                        onTextChanged: amStoreModel.filterText = text
                    }

                    Controls.ToolButton {
                        id: filterButton
                        icon.name: "view-filter"
                        text: i18n("Sort and Filter")
                        display: Controls.AbstractButton.IconOnly
                        down: filterMenu.visible
                        onPressed: filterMenu.visible ? filterMenu.close() : filterMenu.popup(filterButton, 0, filterButton.height)
                        Controls.ToolTip.visible: hovered && !filterMenu.visible
                        Controls.ToolTip.text: text
                        Controls.ToolTip.delay: Kirigami.Units.toolTipDelay

                        Controls.Menu {
                            id: filterMenu

                            Controls.MenuItem {
                                text: i18n("Sort A–Z")
                                checkable: true; autoExclusive: true
                                checked: amStoreModel.sortAscending
                                onTriggered: amStoreModel.sortAscending = true
                            }
                            Controls.MenuItem {
                                text: i18n("Sort Z–A")
                                checkable: true; autoExclusive: true
                                checked: !amStoreModel.sortAscending
                                onTriggered: amStoreModel.sortAscending = false
                            }

                            Controls.MenuSeparator {}

                            Controls.Menu {
                                id: sourceMenu
                                title: i18n("Source")

                                Controls.MenuItem {
                                    text: i18n("All Sources")
                                    checkable: true
                                    checked: amStoreModel.storeSource === 0
                                    onTriggered: amStoreModel.storeSource = 0
                                }
                                Controls.MenuSeparator {}
                                Instantiator {
                                    model: amStoreModel.sources
                                    onObjectAdded: (index, object) => sourceMenu.insertItem(index + 2, object)
                                    onObjectRemoved: (index, object) => sourceMenu.removeItem(object)
                                    delegate: Controls.MenuItem {
                                        text: modelData.name
                                        visible: modelData.enabled
                                        height: visible ? implicitHeight : 0
                                        checkable: true
                                        checked: amStoreModel.storeSource === index + 1
                                        onTriggered: amStoreModel.storeSource = index + 1
                                    }
                                }
                            }

                            Controls.Menu {
                                id: categoryMenu
                                title: i18n("Category")

                                Controls.MenuItem {
                                    text: i18n("All Categories")
                                    checkable: true
                                    checked: !pageRoot.categoryActive
                                    onTriggered: amStoreModel.categoryFilter = ""
                                }
                                Controls.MenuSeparator {}
                                Instantiator {
                                    model: amStoreModel.availableCategories
                                    onObjectAdded: (index, object) => categoryMenu.insertItem(index + 2, object)
                                    onObjectRemoved: (index, object) => categoryMenu.removeItem(object)
                                    delegate: Controls.MenuItem {
                                        text: modelData
                                        checkable: true
                                        checked: amStoreModel.categoryFilter === modelData
                                        onTriggered: amStoreModel.categoryFilter = modelData
                                    }
                                }
                            }
                        }
                    }
                }

                // Active filters, dimmed, with a clear button
                RowLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: Kirigami.Units.largeSpacing
                    Layout.rightMargin: Kirigami.Units.smallSpacing
                    visible: pageRoot.filtersActive
                    spacing: Kirigami.Units.smallSpacing

                    Controls.Label {
                        Layout.fillWidth: true
                        text: [pageRoot.sourceName, pageRoot.categoryActive ? amStoreModel.categoryFilter : ""]
                              .filter(s => s !== "").join(" · ")
                        color: Kirigami.Theme.disabledTextColor
                        elide: Text.ElideRight
                    }

                    Controls.ToolButton {
                        icon.name: "dialog-close"
                        text: i18n("Clear Filters")
                        display: Controls.AbstractButton.IconOnly
                        onClicked: pageRoot.clearFilters()
                        Controls.ToolTip.visible: hovered
                        Controls.ToolTip.text: text
                        Controls.ToolTip.delay: Kirigami.Units.toolTipDelay
                    }
                }

                // App list
                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    Controls.BusyIndicator {
                        anchors.centerIn: parent
                        running: amStoreModel.loading
                        visible: running
                        z: 1
                    }

                    // The desktop ScrollView reserves room for the scrollbar beside the rows (no overlap)
                    // and adds Kirigami.WheelHandler for desktop-speed wheel scrolling.
                    Controls.ScrollView {
                        anchors.fill: parent

                        ListView {
                            id: listView
                            model: amStoreModel
                            clip: true
                            currentIndex: -1
                            // Pre-create delegates for ±20 items outside the visible area so
                            // images start loading before rows scroll into view.
                            cacheBuffer: Math.round(Kirigami.Units.gridUnit * 2.5 * 20)

                            onCurrentIndexChanged: {
                                pageRoot.refreshCurrentItemAt(currentIndex)
                                // Clear stale log when selection changes
                                if (currentIndex >= 0) {
                                    logModel.clear()
                                    pageRoot.activePackage = ""
                                    pageRoot.showLog = false
                                    detailTransition.restart()
                                } else {
                                    detailTransition.stop()
                                    detailInner.opacity = 1.0
                                    pageRoot.displayedItem = {}
                                }
                            }

                            // Grouped by category unless one is filtered (the model sorts the same way).
                            section.property: pageRoot.categoryActive ? "" : "sectionCategory"
                            section.delegate: Kirigami.ListSectionHeader {
                                required property string section
                                width: ListView.view.width
                                text: section
                            }

                            TapHandler {
                                onTapped: (eventPoint) => {
                                    const hit = listView.itemAt(
                                        eventPoint.position.x,
                                        eventPoint.position.y + listView.contentY)
                                    if (!hit) listView.currentIndex = -1
                                }
                            }

                            delegate: Controls.ItemDelegate {
                                id: delegateRoot
                                width: listView.width
                                highlighted: ListView.isCurrentItem
                                // Same geometry for every row (the style gives the last row a larger bottom inset).
                                topInset: 0
                                bottomInset: 0
                                leftInset: Kirigami.Units.smallSpacing
                                rightInset: Kirigami.Units.smallSpacing
                                verticalPadding: Kirigami.Units.smallSpacing
                                horizontalPadding: Kirigami.Units.largeSpacing
                                onClicked: listView.currentIndex = index

                                contentItem: RowLayout {
                                    id: storeRow
                                    spacing: Kirigami.Units.largeSpacing

                                    readonly property string iconSource: model.iconSource ?? ""
                                    readonly property bool remoteIcon: iconSource.startsWith("http")

                                    Item {
                                        implicitWidth: Kirigami.Units.iconSizes.medium
                                        implicitHeight: Kirigami.Units.iconSizes.medium

                                        // Remote icons are often 512 px: decode off the UI thread at the shown size,
                                        // so scrolling stays smooth (Kirigami.Icon decodes on the UI thread).
                                        Image {
                                            id: remoteIconImage
                                            anchors.fill: parent
                                            visible: storeRow.remoteIcon && status === Image.Ready
                                            source: storeRow.remoteIcon ? storeRow.iconSource : ""
                                            sourceSize.width: Kirigami.Units.iconSizes.medium * 2
                                            sourceSize.height: Kirigami.Units.iconSizes.medium * 2
                                            fillMode: Image.PreserveAspectFit
                                            asynchronous: true
                                        }
                                        Kirigami.Icon {
                                            anchors.fill: parent
                                            visible: !remoteIconImage.visible
                                            source: storeRow.remoteIcon ? "application-x-executable" : pageRoot.iconFor(storeRow.iconSource)
                                            fallback: "application-x-executable"
                                            selected: delegateRoot.highlighted
                                        }
                                    }

                                    Kirigami.TitleSubtitle {
                                        Layout.fillWidth: true
                                        title: model.displayName
                                        subtitle: model.author ?? ""
                                        selected: delegateRoot.highlighted
                                        reserveSpaceForSubtitle: true
                                    }
                                }
                            }
                        }
                    }

                    Kirigami.PlaceholderMessage {
                        anchors.centerIn: parent
                        width: parent.width - Kirigami.Units.gridUnit * 2
                        visible: !amStoreModel.loading && listView.count === 0
                                 && (amStoreModel.filterText !== "" || pageRoot.filtersActive)
                        icon.name: "edit-find"
                        text: amStoreModel.filterText !== ""
                              ? i18n("No results for \"%1\"", amStoreModel.filterText)
                              : i18n("No apps match these filters")
                        helpfulAction: Kirigami.Action {
                            icon.name: "edit-clear"
                            text: i18n("Clear Filters")
                            enabled: pageRoot.filtersActive
                            onTriggered: pageRoot.clearFilters()
                        }
                    }

                    Kirigami.PlaceholderMessage {
                        anchors.centerIn: parent
                        width: parent.width - Kirigami.Units.gridUnit * 2
                        visible: !amStoreModel.loading && listView.count === 0
                                 && amStoreModel.filterText === "" && !pageRoot.filtersActive
                        icon.name: "network-offline"
                        text: i18n("Could not load app catalog")
                        explanation: i18n("Check your network connection and try again.")
                        helpfulAction: Kirigami.Action {
                            text: i18n("Retry")
                            icon.name: "view-refresh"
                            enabled: !amStoreModel.loading
                            onTriggered: amStoreModel.sync()
                        }
                    }
                }
            }
        }

        Kirigami.Separator {
            Layout.fillHeight: true
            visible: rightPane.visible
        }

        // ── Right pane: detail view ────────────────────────────────────────
        Controls.ScrollView {
            id: rightPane
            visible: listView.currentIndex >= 0
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            Controls.ScrollBar.horizontal.policy: Controls.ScrollBar.AlwaysOff

            ColumnLayout {
                id: detailInner
                x: Kirigami.Units.gridUnit
                width: rightPane.availableWidth - Kirigami.Units.gridUnit * 2
                spacing: Kirigami.Units.largeSpacing

                readonly property var item: pageRoot.displayedItem
                readonly property bool hasAm: item.hasAmScript ?? false
                readonly property bool hasHub: (item.hasFeedData ?? false) && (item.downloadUrl ?? "") !== ""
                readonly property bool hasBoth: hasAm && hasHub
                readonly property bool primaryIsAm: hasBoth ? (item.preferAm ?? false) : hasAm

                Item { implicitHeight: Kirigami.Units.largeSpacing }

                // Icon, name, author · link, one line of metadata
                RowLayout {
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.largeSpacing

                    Kirigami.Icon {
                        source: pageRoot.iconFor(detailInner.item.iconSource)
                        fallback: "application-x-executable"
                        implicitWidth: Kirigami.Units.iconSizes.huge
                        implicitHeight: Kirigami.Units.iconSizes.huge
                        Layout.alignment: Qt.AlignTop
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 0

                        Kirigami.Heading {
                            Layout.fillWidth: true
                            text: detailInner.item.displayName ?? ""
                            level: 2
                            elide: Text.ElideRight
                        }

                        Controls.Label {
                            readonly property string author: detailInner.item.author ?? ""
                            readonly property string homepage: detailInner.item.homepage ?? ""
                            Layout.fillWidth: true
                            visible: author !== "" || homepage !== ""
                            text: {
                                const link = homepage !== ""
                                    ? "<a href=\"" + homepage + "\">" + (detailInner.hasAm ? i18n("Install script") : i18n("Website")) + "</a>"
                                    : ""
                                return [author, link].filter(s => s !== "").join(" · ")
                            }
                            textFormat: Text.StyledText
                            color: Kirigami.Theme.disabledTextColor
                            elide: Text.ElideRight
                            onLinkActivated: (link) => Qt.openUrlExternally(link)
                            HoverHandler { cursorShape: parent.hoveredLink ? Qt.PointingHandCursor : Qt.ArrowCursor }
                        }

                        Controls.Label {
                            Layout.fillWidth: true
                            text: {
                                const parts = (detailInner.item.categories ?? "").split(";").filter(s => s.length > 0).slice(0, 2)
                                if (detailInner.item.license) parts.push(detailInner.item.license)
                                const sources = []
                                if (detailInner.item.hasFeedData) sources.push(i18n("AppImage Hub"))
                                if (detailInner.hasAm) sources.push(i18n("AM"))
                                if (sources.length > 0) parts.push(sources.join(", "))
                                return parts.join(" · ")
                            }
                            color: Kirigami.Theme.disabledTextColor
                            elide: Text.ElideRight
                        }
                    }
                }

                // Actions: primary install (or progress + Cancel), overflow menu
                RowLayout {
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.smallSpacing

                    Controls.Button {
                        visible: !amStoreModel.isInstalling && (detailInner.hasAm || detailInner.hasHub)
                        text: detailInner.primaryIsAm ? i18n("Install") : i18n("Download")
                        icon.name: detailInner.primaryIsAm ? "document-import" : "folder-download"
                        onClicked: pageRoot.install(!detailInner.primaryIsAm)
                    }

                    Controls.ProgressBar {
                        visible: amStoreModel.isInstalling
                        Layout.preferredWidth: Kirigami.Units.gridUnit * 8
                        value: amStoreModel.installStage / 5.0
                    }

                    Controls.Button {
                        visible: amStoreModel.isInstalling
                        text: i18n("Cancel")
                        icon.name: "dialog-cancel"
                        onClicked: amStoreModel.cancelInstallation()
                    }

                    Item { Layout.fillWidth: true }

                    Controls.ToolButton {
                        id: moreButton
                        visible: detailInner.hasBoth
                        icon.name: "overflow-menu"
                        text: i18n("More")
                        display: Controls.AbstractButton.IconOnly
                        down: moreMenu.visible
                        onPressed: moreMenu.visible ? moreMenu.close() : moreMenu.popup(moreButton, 0, moreButton.height)
                        Controls.ToolTip.visible: hovered && !moreMenu.visible
                        Controls.ToolTip.text: text
                        Controls.ToolTip.delay: Kirigami.Units.toolTipDelay

                        Controls.Menu {
                            id: moreMenu
                            Controls.MenuItem {
                                text: detailInner.primaryIsAm ? i18n("Download from AppImage Hub") : i18n("Install with AM")
                                icon.name: detailInner.primaryIsAm ? "folder-download" : "document-import"
                                enabled: !amStoreModel.isInstalling
                                onTriggered: pageRoot.install(detailInner.primaryIsAm)
                            }
                        }
                    }
                }

                // Status line + optional log
                RowLayout {
                    Layout.fillWidth: true
                    visible: logModel.count > 0
                    spacing: Kirigami.Units.smallSpacing

                    Controls.Label {
                        Layout.fillWidth: true
                        text: pageRoot.statusLine
                        color: Kirigami.Theme.disabledTextColor
                        elide: Text.ElideRight
                    }

                    Controls.ToolButton {
                        text: pageRoot.showLog ? i18n("Hide Details") : i18n("Show Details")
                        onClicked: pageRoot.showLog = !pageRoot.showLog
                    }
                }

                Controls.ScrollView {
                    id: logScrollView
                    Layout.fillWidth: true
                    Layout.preferredHeight: Kirigami.Units.gridUnit * 9
                    visible: pageRoot.showLog && logModel.count > 0
                    clip: true

                    Column {
                        width: logScrollView.availableWidth

                        Repeater {
                            model: logModel
                            delegate: Controls.Label {
                                width: parent.width
                                text: model.text
                                font.family: "monospace"
                                font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                                wrapMode: Text.WrapAnywhere
                            }
                        }
                    }

                    // Auto-scroll to bottom when new lines arrive
                    onContentHeightChanged: {
                        if (amStoreModel.isInstalling)
                            contentItem.contentY = Math.max(0, contentItem.contentHeight - height)
                    }
                }

                Controls.Label {
                    Layout.fillWidth: true
                    visible: text !== ""
                    text: detailInner.item.description ?? ""
                    wrapMode: Text.WordWrap
                    textFormat: Text.StyledText
                }

                Item { implicitHeight: Kirigami.Units.gridUnit }
            }
        }
    }
}
