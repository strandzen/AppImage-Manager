// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import appimagemanager

Kirigami.Page {
    id: pageRoot
    title: i18n("Discover AppImages")

    property alias searchField: searchField

    readonly property color cardBorderColor: Theme.cardBorderColor

    // Lazy init — trigger once when this page first becomes visible
    property bool storeInitialized: false
    onVisibleChanged: {
        if (visible && !storeInitialized) {
            storeInitialized = true
            amStoreModel.initialize()
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

    function getLogColor(text) {
        const lower = text.toLowerCase()
        if (lower.includes("error") || lower.includes("failed") || lower.includes("✗"))
            return Kirigami.Theme.negativeTextColor
        if (lower.includes("warning") || lower.includes("warn"))
            return Kirigami.Theme.neutralTextColor
        if (lower.includes("success") || lower.includes("done") || lower.includes("✓")
                || lower.includes("installed") || lower.includes("complete"))
            return Kirigami.Theme.positiveTextColor
        return Kirigami.Theme.textColor
    }

    // Installation log
    ListModel { id: logModel }

    property string activePackage: ""
    property bool showLog: true

    Connections {
        target: amStoreModel
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
                    ? i18n("✓ Installation completed successfully!")
                    : i18n("✗ Installation failed.") })
                if (success)
                    showPassiveNotification(i18n("Installed %1 successfully", pageRoot.displayedItem.displayName ?? ""))
                else
                    showPassiveNotification(i18n("Failed to install %1", pageRoot.displayedItem.displayName ?? ""))
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
        anchors.fill: parent
        spacing: 0

        // ── Left pane ──────────────────────────────────────────────────────
        Item {
            id: leftPane
            Layout.fillHeight: true
            Layout.fillWidth: listView.currentIndex < 0
            Layout.preferredWidth: Kirigami.Units.gridUnit * 18

            Kirigami.Theme.colorSet: Kirigami.Theme.Window
            Kirigami.Theme.inherit: false

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                // Search + sort + source filter
                RowLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: Kirigami.Units.smallSpacing
                    Layout.rightMargin: Kirigami.Units.smallSpacing
                    Layout.topMargin: Kirigami.Units.smallSpacing
                    Layout.bottomMargin: Kirigami.Units.smallSpacing
                    spacing: Kirigami.Units.smallSpacing

                    Kirigami.SearchField {
                        id: searchField
                        Layout.fillWidth: true
                        placeholderText: i18n("Search apps")
                        onTextChanged: amStoreModel.filterText = text
                    }

                    Controls.Button {
                        id: sortBtn
                        display: Controls.AbstractButton.IconOnly
                        icon.name: amStoreModel.sortAscending ? "view-sort-ascending" : "view-sort-descending"
                        text: amStoreModel.sortAscending ? i18n("Sort A–Z") : i18n("Sort Z–A")
                        onClicked: amStoreModel.sortAscending = !amStoreModel.sortAscending
                        Controls.ToolTip.visible: hovered
                        Controls.ToolTip.text: text
                        Controls.ToolTip.delay: Kirigami.Units.toolTipDelay
                    }

                    Controls.Button {
                        id: filterBtn
                        display: Controls.AbstractButton.IconOnly
                        icon.name: "view-filter"
                        text: i18n("Filter by source")
                        // Highlight when a non-default source is active
                        highlighted: amStoreModel.storeSource !== 0
                        onClicked: sourceMenu.popup()
                        Controls.ToolTip.visible: hovered
                        Controls.ToolTip.text: {
                            const sources = amStoreModel.sources
                            if (amStoreModel.storeSource > 0 && amStoreModel.storeSource - 1 < sources.length) {
                                return i18n("Source: %1", sources[amStoreModel.storeSource - 1].name)
                            }
                            return i18n("Source: All")
                        }
                        Controls.ToolTip.delay: Kirigami.Units.toolTipDelay

                        Controls.Menu {
                            id: sourceMenu
                            Controls.MenuItem {
                                text: i18n("All Sources")
                                checkable: true
                                checked: amStoreModel.storeSource === 0
                                onTriggered: amStoreModel.storeSource = 0
                            }

                            Instantiator {
                                model: amStoreModel.sources
                                onObjectAdded: (index, object) => sourceMenu.insertItem(index + 1, object)
                                onObjectRemoved: (index, object) => sourceMenu.removeItem(object)
                                delegate: Controls.MenuItem {
                                    text: modelData.name
                                    visible: modelData.enabled
                                    checkable: true
                                    checked: amStoreModel.storeSource === index + 1
                                    onTriggered: amStoreModel.storeSource = index + 1
                                }
                            }
                        }
                    }
                }

                // Category chips — Flickable avoids the ScrollView scrollbar
                // stealing vertical height and clipping chip text.
                Flickable {
                    Layout.fillWidth: true
                    Layout.preferredHeight: chipRow.implicitHeight
                    Layout.leftMargin: Kirigami.Units.smallSpacing
                    Layout.rightMargin: Kirigami.Units.smallSpacing
                    Layout.bottomMargin: Kirigami.Units.smallSpacing
                    contentWidth: chipRow.implicitWidth
                    contentHeight: chipRow.implicitHeight
                    flickableDirection: Flickable.HorizontalFlick
                    clip: true

                    RowLayout {
                        id: chipRow
                        spacing: Kirigami.Units.smallSpacing
                        Repeater {
                            model: amStoreModel.availableCategories
                            delegate: Controls.Button {
                                text: modelData
                                checkable: true
                                checked: amStoreModel.categoryFilter === modelData
                                onClicked: {
                                    amStoreModel.categoryFilter =
                                        amStoreModel.categoryFilter === modelData ? "" : modelData
                                }
                                scale: 1.0
                                Behavior on scale {
                                    NumberAnimation { duration: 100; easing.type: Easing.OutBack }
                                }
                                onPressed: scale = 0.93
                                onReleased: scale = 1.0
                                onCanceled: scale = 1.0
                            }
                        }
                    }
                }

                // App list
                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    ColumnLayout {
                        anchors.centerIn: parent
                        spacing: Kirigami.Units.smallSpacing
                        visible: amStoreModel.loading
                        z: 1

                        Controls.BusyIndicator {
                            Layout.alignment: Qt.AlignHCenter
                            running: amStoreModel.loading
                        }

                        Controls.Label {
                            Layout.alignment: Qt.AlignHCenter
                            text: i18n("Gathering from sources…")
                            opacity: 0.6
                            font: Kirigami.Theme.smallFont
                        }
                    }

                    ListView {
                        id: listView
                        anchors.fill: parent
                        model: amStoreModel
                        clip: true
                        currentIndex: -1

                        Controls.ScrollBar.vertical: Controls.ScrollBar {
                            id: listScrollBar
                            policy: Controls.ScrollBar.AsNeeded
                        }

                        onCurrentIndexChanged: {
                            pageRoot.refreshCurrentItemAt(currentIndex)
                            // Clear stale log when selection changes
                            if (currentIndex >= 0) {
                                logModel.clear()
                                pageRoot.activePackage = ""
                                detailTransition.restart()
                            } else {
                                detailTransition.stop()
                                detailInner.opacity = 1.0
                                pageRoot.displayedItem = {}
                            }
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
                            width: listView.width - listScrollBar.width
                            height: Kirigami.Units.gridUnit * 2.5
                            highlighted: ListView.isCurrentItem
                            horizontalPadding: Kirigami.Units.smallSpacing
                            topInset: Kirigami.Units.largeSpacing / 2
                            bottomInset: Kirigami.Units.largeSpacing / 2
                            leftInset: 0
                            rightInset: 0
                            verticalPadding: Kirigami.Units.largeSpacing / 2
                            onClicked: listView.currentIndex = index

                            contentItem: RowLayout {
                                spacing: Kirigami.Units.smallSpacing

                                Item {
                                    implicitWidth: Kirigami.Units.iconSizes.smallMedium
                                    implicitHeight: Kirigami.Units.iconSizes.smallMedium
                                    Layout.alignment: Qt.AlignVCenter
                                    readonly property bool isUrl: model.iconSource.startsWith("http")
                                    Image {
                                        id: delegateImg
                                        anchors.fill: parent
                                        source: parent.isUrl ? model.iconSource : ""
                                        fillMode: Image.PreserveAspectFit
                                    }
                                    Kirigami.Icon {
                                        anchors.fill: parent
                                        source: model.iconSource || "package-x-generic"
                                        fallback: "package-x-generic"
                                        visible: !parent.isUrl || delegateImg.status !== Image.Ready
                                    }
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
                            }
                        }
                    }

                    Kirigami.PlaceholderMessage {
                        anchors.centerIn: parent
                        visible: listView.count === 0 && amStoreModel.filterText !== ""
                        icon.name: "edit-find"
                        text: i18n("No results for \"%1\"", amStoreModel.filterText)
                        explanation: i18n("Try a different search term or switch to \"All\" sources")
                    }

                    Kirigami.PlaceholderMessage {
                        anchors.centerIn: parent
                        visible: !amStoreModel.loading && listView.count === 0 && amStoreModel.filterText === ""
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

        // ── Right pane: detail view ────────────────────────────────────────
        Item {
            id: rightPane
            visible: listView.currentIndex >= 0
            Layout.fillWidth: true
            Layout.fillHeight: true

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

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
                        id: detailInner
                        width: detailScrollView.availableWidth
                        spacing: 0

                        // Hero: icon + name + source link
                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.topMargin: Kirigami.Units.largeSpacing
                            spacing: Kirigami.Units.smallSpacing

                            Item {
                                implicitWidth: Kirigami.Units.iconSizes.enormous
                                implicitHeight: Kirigami.Units.iconSizes.enormous
                                Layout.alignment: Qt.AlignHCenter
                                readonly property bool isUrl: (pageRoot.displayedItem.iconSource ?? "").startsWith("http")
                                Image {
                                    id: detailImg
                                    anchors.fill: parent
                                    source: parent.isUrl ? (pageRoot.displayedItem.iconSource ?? "") : ""
                                    fillMode: Image.PreserveAspectFit
                                }
                                Kirigami.Icon {
                                    anchors.fill: parent
                                    source: pageRoot.displayedItem.iconSource ?? "package-x-generic"
                                    fallback: "package-x-generic"
                                    visible: !parent.isUrl || detailImg.status !== Image.Ready
                                }
                            }

                            Kirigami.Heading {
                                text: pageRoot.displayedItem.displayName ?? ""
                                level: 2
                                horizontalAlignment: Text.AlignHCenter
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                            }

                            // Author
                            Controls.Label {
                                text: pageRoot.displayedItem.author ?? ""
                                visible: (pageRoot.displayedItem.author ?? "") !== ""
                                opacity: 0.6
                                font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                                horizontalAlignment: Text.AlignHCenter
                                Layout.fillWidth: true
                            }

                            RowLayout {
                                Layout.alignment: Qt.AlignHCenter
                                spacing: Kirigami.Units.smallSpacing
                                visible: (pageRoot.displayedItem.homepage ?? "") !== ""

                                Controls.Label {
                                    text: (pageRoot.displayedItem.hasAmScript ?? false)
                                          ? i18n("Install Recipe Script:")
                                          : i18n("Source:")
                                    opacity: 0.6
                                    font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                                }

                                Controls.Label {
                                    text: "<a href='" + (pageRoot.displayedItem.homepage ?? "") + "'>"
                                          + i18n("View Source") + "</a>"
                                    textFormat: Text.StyledText
                                    font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                                    onLinkActivated: Qt.openUrlExternally(pageRoot.displayedItem.homepage)
                                    HoverHandler { cursorShape: Qt.PointingHandCursor }
                                }
                            }

                            Flow {
                                Layout.alignment: Qt.AlignHCenter
                                Layout.topMargin: Kirigami.Units.smallSpacing
                                spacing: Kirigami.Units.smallSpacing

                                Repeater {
                                    model: (pageRoot.displayedItem.categories ?? "")
                                           .split(";").filter(s => s.length > 0).slice(0, 3)
                                    delegate: Kirigami.Chip {
                                        text: modelData
                                        closable: false; checkable: false
                                    }
                                }

                                Kirigami.Chip {
                                    text: pageRoot.displayedItem.license ?? ""
                                    visible: (pageRoot.displayedItem.license ?? "") !== ""
                                    closable: false; checkable: false
                                    opacity: 0.75
                                }

                                // Source badge
                                Kirigami.Chip {
                                    text: (pageRoot.displayedItem.hasAmScript ?? false)
                                          && (pageRoot.displayedItem.hasFeedData ?? false)
                                          ? i18n("Hub + AM")
                                          : (pageRoot.displayedItem.hasFeedData ?? false)
                                            ? i18n("AppImage Hub")
                                            : i18n("AM Only")
                                    visible: (pageRoot.displayedItem.displayName ?? "") !== ""
                                    closable: false; checkable: false
                                    opacity: 0.6
                                }
                            }
                        }

                        // Description card
                        Kirigami.Card {
                            Layout.fillWidth: true
                            Layout.topMargin: Kirigami.Units.largeSpacing
                            Layout.maximumHeight: Math.max(
                                Kirigami.Units.gridUnit * 6,
                                detailScrollView.availableHeight - Kirigami.Units.gridUnit * 18)
                            visible: (pageRoot.displayedItem.description ?? "") !== ""

                            contentItem: ColumnLayout {
                                spacing: Kirigami.Units.smallSpacing

                                Kirigami.Heading { text: i18n("Description"); level: 4 }

                                Controls.ScrollView {
                                    id: descriptionScroller
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    clip: true
                                    Controls.ScrollBar.horizontal.policy: Controls.ScrollBar.AlwaysOff

                                    Controls.Label {
                                        width: descriptionScroller.availableWidth
                                        text: pageRoot.displayedItem.description ?? ""
                                        wrapMode: Text.WordWrap
                                        opacity: 0.85
                                        textFormat: Text.StyledText
                                    }
                                }
                            }
                        }

                        // Installation output console
                        Kirigami.Card {
                            id: logCard
                            Layout.fillWidth: true
                            Layout.topMargin: Kirigami.Units.smallSpacing
                            Layout.preferredHeight: Kirigami.Units.gridUnit * 9
                            visible: amStoreModel.isInstalling || logModel.count > 0

                            contentItem: ColumnLayout {
                                spacing: Kirigami.Units.smallSpacing

                                RowLayout {
                                    Layout.fillWidth: true

                                    Kirigami.Heading {
                                        text: i18n("Installation Output")
                                        level: 4
                                        Layout.fillWidth: true
                                    }

                                    Controls.ToolButton {
                                        icon.name: pageRoot.showLog ? "view-hidden" : "view-visible"
                                        text: pageRoot.showLog ? i18n("Hide log") : i18n("Show log")
                                        display: Controls.AbstractButton.IconOnly
                                        onClicked: pageRoot.showLog = !pageRoot.showLog
                                        Controls.ToolTip.text: text
                                        Controls.ToolTip.visible: hovered
                                        Controls.ToolTip.delay: Kirigami.Units.toolTipDelay
                                    }
                                }

                                // Stage progress bar
                                Controls.ProgressBar {
                                    Layout.fillWidth: true
                                    visible: amStoreModel.isInstalling || amStoreModel.installStage === 5
                                    value: amStoreModel.installStage / 5.0
                                    Behavior on value {
                                        NumberAnimation { duration: 300; easing.type: Easing.OutCubic }
                                    }
                                }

                                Controls.ScrollView {
                                    id: logScrollView
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    clip: true
                                    visible: pageRoot.showLog

                                    Column {
                                        id: logConsole
                                        width: logScrollView.availableWidth
                                        spacing: 2

                                        Repeater {
                                            model: logModel
                                            delegate: Controls.Label {
                                                width: parent.width
                                                text: model.text
                                                font.family: "monospace"
                                                font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                                                wrapMode: Text.WrapAnywhere
                                                color: pageRoot.getLogColor(model.text)
                                            }
                                        }
                                    }

                                    // Auto-scroll to bottom when new lines arrive
                                    onContentHeightChanged: {
                                        if (amStoreModel.isInstalling)
                                            contentItem.contentY = Math.max(0, contentItem.contentHeight - height)
                                    }
                                }
                            }
                        }

                        Item { height: Kirigami.Units.gridUnit }
                    }
                }

                // Sticky bottom action bar
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.margins: Kirigami.Units.largeSpacing
                    spacing: Kirigami.Units.smallSpacing

                    // Cancel — replaces action row while installing
                    Controls.Button {
                        text: i18n("Cancel")
                        icon.name: "process-stop"
                        visible: amStoreModel.isInstalling
                        onClicked: amStoreModel.cancelInstallation()
                        Layout.fillWidth: true
                    }

                    // Split button: primary action + optional dropdown for second source
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 0
                        visible: !amStoreModel.isInstalling

                        readonly property bool hasAm: pageRoot.displayedItem.hasAmScript ?? false
                        readonly property bool hasHub: (pageRoot.displayedItem.hasFeedData ?? false)
                                                       && (pageRoot.displayedItem.downloadUrl ?? "") !== ""
                        readonly property bool hasBoth: hasAm && hasHub
                        readonly property bool preferAm: pageRoot.displayedItem.preferAm ?? false
                        readonly property bool primaryIsAm: hasBoth ? preferAm : hasAm

                        Controls.Button {
                            id: primaryBtn
                            text: parent.primaryIsAm ? i18n("Install via AM") : i18n("Download")
                            icon.name: parent.primaryIsAm ? "document-import" : "folder-download"
                            Layout.fillWidth: true
                            onClicked: {
                                logModel.clear()
                                pageRoot.showLog = true
                                pageRoot.activePackage = pageRoot.displayedItem.packageName
                                if (parent.primaryIsAm) {
                                    amStoreModel.installApp(pageRoot.displayedItem.packageName, false)
                                } else {
                                    amStoreModel.installApp(pageRoot.displayedItem.packageName, true)
                                }
                            }
                        }

                        // Dropdown arrow — only when both AM and Hub available
                        Controls.Button {
                            id: dropBtn
                            icon.name: "arrow-down"
                            visible: parent.hasBoth
                            implicitWidth: visible ? implicitHeight : 0
                            onClicked: splitMenu.popup(dropBtn, 0, dropBtn.height)

                            Controls.Menu {
                                id: splitMenu
                                Controls.MenuItem {
                                    text: i18n("Install via AM")
                                    icon.name: "document-import"
                                    onTriggered: {
                                        logModel.clear()
                                        pageRoot.showLog = true
                                        pageRoot.activePackage = pageRoot.displayedItem.packageName
                                        amStoreModel.installApp(pageRoot.displayedItem.packageName, false)
                                    }
                                }
                                Controls.MenuItem {
                                    text: i18n("Download from Hub")
                                    icon.name: "folder-download"
                                    onTriggered: {
                                        logModel.clear()
                                        pageRoot.showLog = true
                                        pageRoot.activePackage = pageRoot.displayedItem.packageName
                                        amStoreModel.installApp(pageRoot.displayedItem.packageName, true)
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
