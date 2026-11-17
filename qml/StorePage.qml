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

    // Drag handle for the list's width, shared with Installed (AppSettings.libraryListWidth).
    MouseArea {
        id: resizeHandle
        // Over the separator, not the list's scrollbar.
        x: leftPane.width - 1
        y: masterDetailRow.y
        height: masterDetailRow.height
        width: Kirigami.Units.smallSpacing + 2
        z: 10
        visible: masterDetailRow.visible && !leftPane.Layout.fillWidth
        cursorShape: Qt.SplitHCursor
        hoverEnabled: true
        property real startX: 0
        property real startWidth: 0
        onPressed: (mouse) => {
            startX = mapToItem(masterDetailRow, mouse.x, 0).x
            startWidth = leftPane.width
            leftPane.dragWidth = startWidth
        }
        onPositionChanged: (mouse) => {
            if (pressed)
                leftPane.dragWidth = startWidth + mapToItem(masterDetailRow, mouse.x, 0).x - startX
        }
        onReleased: {
            AppSettings.libraryListWidth = Math.round(leftPane.listWidth)
            leftPane.dragWidth = -1
        }
        onDoubleClicked: AppSettings.libraryListWidth = 0
        Controls.ToolTip.text: i18n("Drag to resize; double-click for the default width")
        Controls.ToolTip.visible: containsMouse && !pressed
        Controls.ToolTip.delay: Kirigami.Units.toolTipDelay * 3
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
            Layout.preferredWidth: listWidth
            Kirigami.Theme.colorSet: Kirigami.Theme.View
            Kirigami.Theme.inherit: false
            color: Kirigami.Theme.backgroundColor

            // Draggable width, the same as Installed's; 0 in the settings = the default.
            readonly property real defaultWidth: Kirigami.Units.gridUnit * 16
            readonly property real minWidth: Kirigami.Units.gridUnit * 11
            readonly property real maxWidth: Math.max(minWidth, masterDetailRow.width - Kirigami.Units.gridUnit * 20)
            property real dragWidth: -1
            readonly property real listWidth: Math.min(maxWidth, Math.max(minWidth,
                dragWidth >= 0 ? dragWidth
                               : AppSettings.libraryListWidth > 0 ? AppSettings.libraryListWidth : defaultWidth))

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
                        placeholderText: i18n("Search")
                        onTextChanged: amStoreModel.filterText = text
                    }

                    Controls.ToolButton {
                        id: filterButton
                        // Looks like Installed's Sort button; the menu also holds the filters.
                        icon.name: "view-sort"
                        text: i18n("Sort")
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

                Controls.ProgressBar {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 2
                    indeterminate: true
                    visible: amStoreModel.loading
                    padding: 0
                }

                // App list
                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

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
                                visible: section !== ""
                                height: visible ? implicitHeight : 0
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

                                    // Name and author, drawn like Installed's name and status line.
                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 0

                                        Controls.Label {
                                            Layout.fillWidth: true
                                            text: model.displayName
                                            elide: Text.ElideRight
                                            color: delegateRoot.highlighted ? Kirigami.Theme.highlightedTextColor
                                                                            : Kirigami.Theme.textColor
                                        }
                                        Controls.Label {
                                            Layout.fillWidth: true
                                            text: model.author ?? ""
                                            elide: Text.ElideRight
                                            font: Kirigami.Theme.smallFont
                                            color: delegateRoot.highlighted ? Kirigami.Theme.highlightedTextColor
                                                                            : Kirigami.Theme.disabledTextColor
                                        }
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
                // Centered in the pane; the margin stays when the pane is narrow (as on Installed).
                x: Math.max(Kirigami.Units.gridUnit * 1.5, (rightPane.availableWidth - width) / 2)
                // Capped so description lines stay readable on wide windows.
                width: Math.min(rightPane.availableWidth - Kirigami.Units.gridUnit * 3, Kirigami.Units.gridUnit * 38)
                spacing: Kirigami.Units.largeSpacing

                readonly property var item: pageRoot.displayedItem
                readonly property bool hasAm: item.hasAmScript ?? false
                readonly property bool hasHub: (item.hasFeedData ?? false) && (item.downloadUrl ?? "") !== ""
                readonly property bool hasBoth: hasAm && hasHub
                readonly property bool primaryIsAm: hasBoth ? (item.preferAm ?? false) : hasAm
                // AM apps carry their install script's page as homepage unless a catalog gave a website.
                readonly property string scriptUrl: hasAm && (item.packageName ?? "") !== ""
                    ? "https://github.com/ivan-hc/AM/blob/main/programs/x86_64/" + item.packageName : ""
                readonly property string website: {
                    const page = item.homepage ?? ""
                    return page.startsWith("https://github.com/ivan-hc/AM/") ? "" : page
                }

                Item { implicitHeight: Kirigami.Units.largeSpacing }

                // Icon, name, author · website
                RowLayout {
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.largeSpacing

                    Kirigami.Icon {
                        source: pageRoot.iconFor(detailInner.item.iconSource)
                        fallback: "application-x-executable"
                        implicitWidth: Kirigami.Units.iconSizes.huge
                        implicitHeight: Kirigami.Units.iconSizes.huge
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Kirigami.Units.smallSpacing

                        Kirigami.Heading {
                            Layout.fillWidth: true
                            text: detailInner.item.displayName ?? ""
                            level: 1
                            elide: Text.ElideRight
                        }

                        Controls.Label {
                            readonly property string author: detailInner.item.author ?? ""
                            readonly property string homepage: detailInner.website
                            readonly property string host: {
                                const m = homepage.match(/^https?:\/\/([^/]+)/)
                                return m ? m[1] : homepage
                            }
                            Layout.fillWidth: true
                            visible: author !== "" || host !== ""
                            text: {
                                const link = host !== "" ? "<a href=\"" + homepage + "\">" + host + "</a>" : ""
                                return [author, link].filter(s => s !== "").join(" · ")
                            }
                            textFormat: Text.StyledText
                            color: Kirigami.Theme.disabledTextColor
                            elide: Text.ElideRight
                            onLinkActivated: (link) => Qt.openUrlExternally(link)
                            HoverHandler { cursorShape: parent.hoveredLink ? Qt.PointingHandCursor : Qt.ArrowCursor }
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
                        visible: detailInner.hasBoth || detailInner.website !== "" || detailInner.scriptUrl !== ""
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
                                visible: detailInner.hasBoth
                                height: visible ? implicitHeight : 0
                                text: detailInner.primaryIsAm ? i18n("Download from AppImage Hub") : i18n("Install with AM")
                                icon.name: detailInner.primaryIsAm ? "folder-download" : "document-import"
                                enabled: !amStoreModel.isInstalling
                                onTriggered: pageRoot.install(detailInner.primaryIsAm)
                            }
                            Controls.MenuItem {
                                visible: detailInner.website !== ""
                                height: visible ? implicitHeight : 0
                                icon.name: "internet-services"
                                text: i18n("Open Website")
                                onTriggered: Qt.openUrlExternally(detailInner.website)
                            }
                            Controls.MenuItem {
                                visible: detailInner.scriptUrl !== ""
                                height: visible ? implicitHeight : 0
                                icon.name: "text-x-script"
                                text: i18n("View Install Script")
                                onTriggered: Qt.openUrlExternally(detailInner.scriptUrl)
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

                // ── About ─────────────────────────────────────────────
                Kirigami.ListSectionHeader {
                    Layout.fillWidth: true
                    Layout.topMargin: Kirigami.Units.largeSpacing
                    visible: description.visible
                    text: i18n("About")
                    leftPadding: 0
                }

                Controls.Label {
                    id: description
                    Layout.fillWidth: true
                    visible: text !== ""
                    text: detailInner.item.description ?? ""
                    wrapMode: Text.WordWrap
                    textFormat: Text.StyledText
                    lineHeight: 1.15
                }

                // ── Details ───────────────────────────────────────────
                Kirigami.ListSectionHeader {
                    Layout.fillWidth: true
                    Layout.topMargin: Kirigami.Units.largeSpacing
                    text: i18n("Details")
                    leftPadding: 0
                }

                GridLayout {
                    id: detailsGrid
                    Layout.fillWidth: true
                    columns: 2
                    columnSpacing: Kirigami.Units.largeSpacing
                    rowSpacing: Kirigami.Units.smallSpacing

                    readonly property var item: detailInner.item
                    readonly property var rows: {
                        const categories = (item.categories ?? "").split(";").filter(s => s.length > 0)
                        // Each catalog that lists the app, and how it installs from there.
                        const sources = amStoreModel.sources
                        const from = (item.sourceIds ?? []).map(id => {
                            const source = sources.find(s => s.id === id)
                            if (!source)
                                return ""
                            return source.type === "am-database" ? i18n("%1 (install script)", source.name)
                                                                 : i18n("%1 (download)", source.name)
                        }).filter(s => s !== "")
                        return [
                            [i18n("Category"), categories.length > 0 ? categories.join(", ") : i18n("Unknown")],
                            [i18n("License"), item.license || i18n("Unknown")],
                            [i18n("Available from"), from.join("\n")],
                        ]
                    }

                    Repeater {
                        model: detailsGrid.rows.length * 2
                        Controls.Label {
                            required property int index
                            readonly property var row: detailsGrid.rows[Math.floor(index / 2)]
                            readonly property bool isKey: index % 2 === 0
                            Layout.alignment: Qt.AlignTop | (isKey ? Qt.AlignRight : Qt.AlignLeft)
                            Layout.fillWidth: !isKey
                            text: isKey ? row[0] : row[1]
                            color: isKey ? Kirigami.Theme.disabledTextColor : Kirigami.Theme.textColor
                            wrapMode: isKey ? Text.NoWrap : Text.Wrap
                        }
                    }

                    Controls.Label {
                        Layout.alignment: Qt.AlignTop | Qt.AlignRight
                        visible: linksColumn.visible
                        text: i18n("Links")
                        color: Kirigami.Theme.disabledTextColor
                    }
                    ColumnLayout {
                        id: linksColumn
                        Layout.fillWidth: true
                        visible: detailInner.website !== "" || detailInner.scriptUrl !== ""
                        spacing: 0
                        Kirigami.UrlButton {
                            visible: detailInner.website !== ""
                            url: detailInner.website
                            text: i18n("Website")
                            horizontalAlignment: Text.AlignLeft
                        }
                        Kirigami.UrlButton {
                            visible: detailInner.scriptUrl !== ""
                            url: detailInner.scriptUrl
                            text: i18n("AM install script")
                            horizontalAlignment: Text.AlignLeft
                        }
                    }
                }

                Item { implicitHeight: Kirigami.Units.gridUnit }
            }
        }
    }

    // ── Keyboard shortcuts (only while this page is shown), as on Installed ──
    Shortcut {
        sequence: "Return"
        enabled: pageRoot.visible && listView.currentIndex >= 0 && !amStoreModel.isInstalling
                 && (detailInner.hasAm || detailInner.hasHub)
        onActivated: pageRoot.install(!detailInner.primaryIsAm)
    }
    Shortcut {
        sequence: "Escape"
        enabled: pageRoot.visible && searchField.text !== ""
        onActivated: { searchField.text = ""; amStoreModel.filterText = "" }
    }
    Shortcut {
        sequence: "Escape"
        enabled: pageRoot.visible && searchField.text === "" && listView.currentIndex >= 0
        onActivated: listView.currentIndex = -1
    }
}
