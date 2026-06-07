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

    title: {
        const total = listModel.rowCount()
        if (proxyModel.filterText !== "" && listView.count !== total)
            return i18n("Installed Applications (%1 of %2)", listView.count, total)
        return total > 0 ? i18n("Installed Applications (%1)", total) : i18n("Installed Applications")
    }

    readonly property color cardBorderColor: Theme.cardBorderColor

    // ── Selected item snapshots ───────────────────────────────────────────────
    property var currentItem: ({})
    // Trails currentItem with a fade; drives all detail pane rendering
    property var displayedItem: ({})

    function refreshCurrentItemAt(idx) {
        if (idx < 0) { currentItem = {}; return }
        currentItem = proxyModel.itemData(idx)
    }

    actions: [
        Kirigami.Action {
            icon.name: "view-sort-ascending"
            text: i18n("Sort")
            displayHint: Kirigami.DisplayHint.IconOnly
            visible: listView.count > 0

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
            visible: listView.count > 0
            enabled: !listModel.checkingUpdates
            onTriggered: listModel.checkForUpdates()
        }
    ]

    Connections {
        target: proxyModel
        function onDataChanged(topLeft, bottomRight) {
            if (listView.currentIndex >= topLeft.row && listView.currentIndex <= bottomRight.row) {
                pageRoot.refreshCurrentItemAt(listView.currentIndex)
                // Data update (metadata loaded, update check etc.) — sync immediately, no animation
                pageRoot.displayedItem = pageRoot.currentItem
            }
        }
    }

    // Detail pane fade: fade out → swap content → fade in
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

        // Info block
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Kirigami.Units.largeSpacing

            Item { Layout.fillHeight: true }

            Kirigami.Icon {
                source: "appimagemanager"
                implicitWidth:  Kirigami.Units.iconSizes.enormous
                implicitHeight: Kirigami.Units.iconSizes.enormous
                Layout.alignment: Qt.AlignHCenter
            }

            Kirigami.Heading {
                text: i18n("No AppImages Installed")
                level: 2
                horizontalAlignment: Text.AlignHCenter
                Layout.fillWidth: true
            }

            Controls.Label {
                text: i18n("AppImages are self-contained applications that run without installation. Drop an AppImage below, or place one in <b>%1</b> to get started.", AppSettings.applicationsPath)
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
                textFormat: Text.StyledText
                opacity: 0.7
                Layout.fillWidth: true
            }

            Item { Layout.fillHeight: true }
        }

        // Drop zone
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: Kirigami.Units.gridUnit * 16

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
                border.color: pageRoot.cardBorderColor
                border.width: globalDropArea.containsDrag ? 2 : 1
                Behavior on border.width { NumberAnimation { duration: Kirigami.Units.shortDuration } }

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

                        Controls.ScrollBar.vertical: Controls.ScrollBar {
                            policy: Controls.ScrollBar.AsNeeded
                        }

                        onCurrentIndexChanged: {
                            pageRoot.refreshCurrentItemAt(currentIndex)
                            if (currentIndex >= 0) {
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
                                if (!hit)
                                    listView.currentIndex = -1
                            }
                        }

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
                            topInset: Kirigami.Units.largeSpacing / 2
                            bottomInset: Kirigami.Units.largeSpacing / 2
                            leftInset: 0
                            rightInset: 0
                            verticalPadding: Kirigami.Units.largeSpacing / 2

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
                                    border.color: pageRoot.cardBorderColor
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

                        Controls.BusyIndicator {
                            Layout.alignment: Qt.AlignHCenter
                            Layout.topMargin: Kirigami.Units.gridUnit * 3
                            running: !(pageRoot.displayedItem.metadataLoaded ?? false)
                            visible: running
                        }

                        // Hero: icon + name + developer + chips
                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.topMargin: Kirigami.Units.largeSpacing
                            spacing: Kirigami.Units.smallSpacing
                            visible: pageRoot.displayedItem.metadataLoaded ?? false

                            Kirigami.Icon {
                                source: pageRoot.displayedItem.iconSource ?? "application-x-executable"
                                fallback: "application-x-executable"
                                implicitWidth: Kirigami.Units.iconSizes.enormous
                                implicitHeight: Kirigami.Units.iconSizes.enormous
                                Layout.alignment: Qt.AlignHCenter
                            }

                            Kirigami.Heading {
                                text: pageRoot.displayedItem.displayName ?? ""
                                level: 2
                                horizontalAlignment: Text.AlignHCenter
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                            }

                            RowLayout {
                                Layout.alignment: Qt.AlignHCenter
                                spacing: Kirigami.Units.smallSpacing
                                visible: (pageRoot.displayedItem.developerName ?? "") !== ""

                                Controls.Label {
                                    text: pageRoot.displayedItem.developerName ?? ""
                                    opacity: 0.6
                                    font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                                }

                                Controls.Label {
                                    readonly property string host: {
                                        const url = pageRoot.displayedItem.homepage ?? ""
                                        if (!url) return ""
                                        const m = url.match(/^https?:\/\/([^/]+)/)
                                        return m ? m[1] : url
                                    }
                                    text: "<a href='" + (pageRoot.displayedItem.homepage ?? "") + "'>" + host + "</a>"
                                    visible: host !== ""
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

                                Kirigami.Chip {
                                    text: pageRoot.displayedItem.version ?? ""
                                    visible: (pageRoot.displayedItem.version ?? "") !== ""
                                    closable: false; checkable: false
                                }
                                Kirigami.Chip {
                                    text: pageRoot.displayedItem.formattedSize ?? ""
                                    visible: (pageRoot.displayedItem.formattedSize ?? "") !== ""
                                    closable: false; checkable: false
                                }
                                Kirigami.Chip {
                                    readonly property string cat: (pageRoot.displayedItem.categories ?? "")
                                        .split(";").filter(s => s.length > 0)[0] ?? ""
                                    text: cat
                                    visible: cat !== ""
                                    closable: false; checkable: false
                                }
                                Kirigami.Chip {
                                    readonly property var d: pageRoot.displayedItem.addedDate
                                    text: d ? Qt.formatDate(d, "d MMM yyyy") : ""
                                    visible: !!d
                                    closable: false; checkable: false
                                }
                            }
                        }

                        Kirigami.Card {
                            Layout.fillWidth: true
                            Layout.topMargin: Kirigami.Units.largeSpacing
                            Layout.maximumHeight: Math.max(
                                Kirigami.Units.gridUnit * 8,
                                detailScrollView.availableHeight - Kirigami.Units.gridUnit * 15
                            )
                            visible: (pageRoot.displayedItem.metadataLoaded ?? false)
                                     && (pageRoot.displayedItem.description ?? "") !== ""

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
                                        text: pageRoot.displayedItem.description ?? ""
                                        wrapMode: Text.WordWrap
                                        opacity: 0.85
                                        textFormat: Text.StyledText
                                    }
                                }
                            }
                        }

                        Controls.CheckBox {
                            text: i18n("Show in app menu")
                            checked: pageRoot.displayedItem.hasDesktopLink ?? false
                            enabled: pageRoot.displayedItem.metadataLoaded ?? false
                            visible: pageRoot.displayedItem.metadataLoaded ?? false
                            Layout.alignment: Qt.AlignHCenter
                            Layout.topMargin: Kirigami.Units.smallSpacing
                            Layout.bottomMargin: Kirigami.Units.largeSpacing
                            onToggled: {
                                proxyModel.toggleDesktopLink(listView.currentIndex, checked)
                                showPassiveNotification(checked
                                    ? i18n("Added %1 to app menu", pageRoot.displayedItem.displayName ?? "")
                                    : i18n("Removed %1 from app menu", pageRoot.displayedItem.displayName ?? ""))
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
                            text: (pageRoot.displayedItem.isUpdating ?? false)
                                  ? i18n("Updating…")
                                  : i18n("Update")
                            icon.name: "system-software-update"
                            enabled: (pageRoot.displayedItem.updateAvailable ?? false)
                                     && !(pageRoot.displayedItem.isUpdating ?? false)
                            Layout.fillWidth: true
                            Controls.ToolTip.text: (pageRoot.displayedItem.updateAvailable ?? false)
                                ? i18n("Update to version %1", pageRoot.displayedItem.updateVersion ?? "")
                                : i18n("No update available")
                            Controls.ToolTip.visible: hovered
                            Controls.ToolTip.delay: Kirigami.Units.toolTipDelay
                            onClicked: {
                                updateDialog.proxyRow       = listView.currentIndex
                                updateDialog.appName        = pageRoot.displayedItem.displayName ?? ""
                                updateDialog.currentVersion = pageRoot.displayedItem.version ?? ""
                                updateDialog.newVersion     = pageRoot.displayedItem.updateVersion ?? ""
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
                        visible: pageRoot.displayedItem.isUpdating ?? false
                        value: (pageRoot.displayedItem.updateProgress ?? 0) / 100.0
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
