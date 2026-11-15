// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import Qt.labs.platform as Platform
import org.kde.kirigami as Kirigami
import appimagemanager

Kirigami.Page {
    id: pageRoot

    property alias searchField: searchField
    // Whether the drop box is on screen (false on other pages, too); DashboardWindow shows
    // its window-wide drop highlight otherwise.
    readonly property bool dropBoxShown: dropBox.visible

    padding: 0

    // ── Selected item snapshots ───────────────────────────────────────────────
    property var currentItem: ({})
    // Trails currentItem with a fade; drives all detail pane rendering
    property var displayedItem: ({})

    function refreshCurrentItemAt(idx) {
        if (idx < 0) { currentItem = {}; return }
        currentItem = proxyModel.itemData(idx)
    }

    // Starts every available update. Uses the source model so rows hidden by the
    // search filter are updated too.
    function updateAll() {
        for (let row = 0; row < listModel.rowCount(); ++row) {
            const item = listModel.itemData(row)
            if (item.updateAvailable && !item.isUpdating)
                listModel.downloadUpdate(row)
        }
    }

    // Non-modal feedback for update checks: a passive notification when there is
    // news, an inline message with Retry when the check failed.
    Connections {
        target: listModel
        function onUpdateCheckFinished(updatesFound, networkFailures) {
            if (updatesFound < 0) {
                updateErrorMessage.text = i18n("The update check timed out. Check your network connection and try again.")
                updateErrorMessage.visible = true
            } else if (networkFailures > 0 && updatesFound === 0) {
                updateErrorMessage.text = i18n("Could not check for updates. Check your network connection and try again.")
                updateErrorMessage.visible = true
            } else {
                showPassiveNotification(updatesFound === 0
                    ? i18n("All AppImages are up to date")
                    : i18np("%1 update available", "%1 updates available", updatesFound))
            }
        }
        function onCheckingUpdatesChanged() {
            if (listModel.checkingUpdates)
                updateErrorMessage.visible = false
        }
    }

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

        Kirigami.PlaceholderMessage {
            Layout.fillWidth: true
            Layout.fillHeight: true
            icon.name: "appimagemanager"
            text: i18n("No AppImages yet")
            explanation: i18n("Drop an AppImage here, or put one in %1.", AppSettings.applicationsPath)
            helpfulAction: Kirigami.Action {
                icon.name: "list-add"
                text: i18n("Install from File…")
                onTriggered: installFileDialog.open()
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
        // List pane: the View color set sets it apart from the details, like a sidebar.
        Rectangle {
            id: leftPane
            Layout.fillHeight: true
            // When nothing selected, fill the entire row; when selected, fixed width.
            Layout.fillWidth: listView.currentIndex < 0
            Layout.preferredWidth: Kirigami.Units.gridUnit * 16
            Kirigami.Theme.colorSet: Kirigami.Theme.View
            Kirigami.Theme.inherit: false
            color: Kirigami.Theme.backgroundColor

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                RowLayout {
                    Layout.fillWidth: true
                    Layout.margins: Kirigami.Units.smallSpacing
                    spacing: Kirigami.Units.smallSpacing

                    Kirigami.SearchField {
                        id: searchField
                        Layout.fillWidth: true
                        placeholderText: i18n("Search")
                        onTextChanged: proxyModel.filterText = text
                    }

                    Controls.ToolButton {
                        id: sortButton
                        icon.name: "view-sort"
                        text: i18n("Sort")
                        display: Controls.AbstractButton.IconOnly
                        down: sortMenu.visible
                        onPressed: sortMenu.visible ? sortMenu.close() : sortMenu.popup(sortButton, 0, sortButton.height)
                        Controls.ToolTip.visible: hovered && !sortMenu.visible
                        Controls.ToolTip.text: text
                        Controls.ToolTip.delay: Kirigami.Units.toolTipDelay

                        Controls.Menu {
                            id: sortMenu
                            Controls.MenuItem {
                                text: i18n("By Name")
                                checkable: true; autoExclusive: true
                                checked: proxyModel.sortField === AppImageSortFilterModel.SortByName
                                onTriggered: proxyModel.sortField = AppImageSortFilterModel.SortByName
                            }
                            Controls.MenuItem {
                                text: i18n("By Size")
                                checkable: true; autoExclusive: true
                                checked: proxyModel.sortField === AppImageSortFilterModel.SortBySize
                                onTriggered: proxyModel.sortField = AppImageSortFilterModel.SortBySize
                            }
                            Controls.MenuItem {
                                text: i18n("By Category")
                                checkable: true; autoExclusive: true
                                checked: proxyModel.sortField === AppImageSortFilterModel.SortByCategory
                                onTriggered: proxyModel.sortField = AppImageSortFilterModel.SortByCategory
                            }
                            Controls.MenuItem {
                                text: i18n("By Date Added")
                                checkable: true; autoExclusive: true
                                checked: proxyModel.sortField === AppImageSortFilterModel.SortByDate
                                onTriggered: proxyModel.sortField = AppImageSortFilterModel.SortByDate
                            }
                        }
                    }
                }

                Controls.ProgressBar {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 2
                    indeterminate: true
                    visible: listModel.scanning || listModel.checkingUpdates
                    padding: 0
                }

                Kirigami.InlineMessage {
                    Layout.fillWidth: true
                    Layout.margins: Kirigami.Units.smallSpacing
                    id: updateErrorMessage
                    visible: false
                    type: Kirigami.MessageType.Error
                    showCloseButton: true
                    actions: Kirigami.Action {
                        icon.name: "view-refresh"
                        text: i18n("Retry")
                        onTriggered: listModel.checkForUpdates()
                    }
                }

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    // With the drop box shown, the list is only as tall as its rows and the box
                    // takes the rest; once rows outgrow the space, the list scrolls above the box.
                    Layout.preferredHeight: listView.contentHeight
                    Layout.maximumHeight: dropBox.visible ? listView.contentHeight : Number.POSITIVE_INFINITY

                    // The desktop ScrollView reserves room for the scrollbar beside the rows (no overlap)
                    // and adds Kirigami.WheelHandler for desktop-speed wheel scrolling.
                    Controls.ScrollView {
                        anchors.fill: parent

                        ListView {
                            id: listView
                            model: proxyModel
                            clip: true
                            currentIndex: -1

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

                            section.property: proxyModel.sortField === AppImageSortFilterModel.SortByCategory ? "sectionCategory"
                                            : proxyModel.sortField === AppImageSortFilterModel.SortByDate ? "sectionAdded" : ""
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
                                    if (!hit)
                                        listView.currentIndex = -1
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

                                contentItem: RowLayout {
                                    spacing: Kirigami.Units.largeSpacing

                                    Kirigami.IconTitleSubtitle {
                                        Layout.fillWidth: true
                                        title: model.displayName
                                        subtitle: model.version
                                        icon.source: model.iconSource
                                        icon.width: Kirigami.Units.iconSizes.medium
                                        icon.height: Kirigami.Units.iconSizes.medium
                                        selected: delegateRoot.highlighted
                                        reserveSpaceForSubtitle: true
                                    }

                                    // Update dot
                                    Rectangle {
                                        implicitWidth: Kirigami.Units.smallSpacing * 2
                                        implicitHeight: implicitWidth
                                        radius: width / 2
                                        color: Kirigami.Theme.positiveTextColor
                                        visible: model.updateAvailable && !model.isUpdating
                                        Layout.alignment: Qt.AlignVCenter
                                        HoverHandler { id: updateDotHover }
                                        Controls.ToolTip.text: i18n("Update available: %1", model.updateVersion)
                                        Controls.ToolTip.visible: updateDotHover.hovered
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
                        }
                    }

                    Kirigami.PlaceholderMessage {
                        anchors.centerIn: parent
                        width: parent.width - Kirigami.Units.gridUnit * 2
                        visible: listView.count === 0 && !listModel.scanning && proxyModel.filterText !== ""
                        icon.name: "edit-find"
                        text: i18n("No results for \"%1\"", proxyModel.filterText)
                    }
                }

                // Drop box: fills the space under the list, never smaller than a few rows.
                Rectangle {
                    id: dropBox
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.minimumHeight: Kirigami.Units.gridUnit * 6
                    Layout.preferredHeight: Kirigami.Units.gridUnit * 6
                    Layout.margins: Kirigami.Units.smallSpacing
                    visible: AppSettings.showInstallBox
                             && !(listView.count === 0 && proxyModel.filterText !== "")
                    radius: Kirigami.Units.cornerRadius
                    color: "transparent"
                    border.width: 1
                    border.color: globalDropArea.containsDrag
                        ? Kirigami.Theme.highlightColor
                        : Kirigami.ColorUtils.linearInterpolation(Kirigami.Theme.backgroundColor,
                                                                  Kirigami.Theme.textColor, 0.2)

                    // Tint while an AppImage is dragged over the window (the window's DropArea
                    // takes the drop).
                    Rectangle {
                        anchors.fill: parent
                        radius: parent.radius
                        color: Kirigami.Theme.highlightColor
                        opacity: globalDropArea.containsDrag ? 0.1 : 0
                        Behavior on opacity { NumberAnimation { duration: Kirigami.Units.shortDuration } }
                    }

                    ColumnLayout {
                        anchors.centerIn: parent
                        spacing: Kirigami.Units.smallSpacing
                        opacity: globalDropArea.containsDrag ? 1.0 : 0.6
                        Behavior on opacity { NumberAnimation { duration: Kirigami.Units.shortDuration } }

                        Kirigami.Icon {
                            source: "list-add"
                            implicitWidth:  Kirigami.Units.iconSizes.medium
                            implicitHeight: Kirigami.Units.iconSizes.medium
                            Layout.alignment: Qt.AlignHCenter
                        }
                        Controls.Label {
                            text: i18n("Drop an AppImage to install it")
                            Layout.alignment: Qt.AlignHCenter
                            font: Kirigami.Theme.smallFont
                        }
                    }
                }

                Kirigami.InlineMessage {
                    Layout.fillWidth: true
                    Layout.margins: Kirigami.Units.smallSpacing
                    visible: AppSettings.showInstallBox && AppSettings.showDisclaimer
                    type: Kirigami.MessageType.Warning
                    text: i18n("AppImages are unverified executables. Only install from sources you trust.")
                    showCloseButton: false
                }
            }
        }

        Kirigami.Separator {
            Layout.fillHeight: true
            visible: rightPane.visible
        }

        // ── Right pane: detail view ───────────────────────────────────
        Controls.ScrollView {
            id: rightPane
            visible: listView.currentIndex >= 0
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            Controls.ScrollBar.horizontal.policy: Controls.ScrollBar.AlwaysOff

            ColumnLayout {
                id: detailInner
                x: Kirigami.Units.gridUnit * 1.5
                // Capped so description lines stay readable on wide windows.
                width: Math.min(rightPane.availableWidth - Kirigami.Units.gridUnit * 3, Kirigami.Units.gridUnit * 38)
                spacing: Kirigami.Units.largeSpacing

                readonly property var item: pageRoot.displayedItem
                readonly property bool loaded: item.metadataLoaded ?? false

                Item { implicitHeight: Kirigami.Units.largeSpacing }

                Controls.BusyIndicator {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.topMargin: Kirigami.Units.gridUnit * 3
                    running: !detailInner.loaded
                    visible: running
                }

                // Icon, name, developer · website
                RowLayout {
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.largeSpacing
                    visible: detailInner.loaded

                    Kirigami.Icon {
                        source: detailInner.item.iconSource ?? "application-x-executable"
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
                            readonly property string developer: detailInner.item.developerName ?? ""
                            readonly property string homepage: detailInner.item.homepage ?? ""
                            readonly property string host: {
                                const m = homepage.match(/^https?:\/\/([^/]+)/)
                                return m ? m[1] : homepage
                            }
                            Layout.fillWidth: true
                            visible: developer !== "" || host !== ""
                            text: {
                                const link = host !== "" ? "<a href=\"" + homepage + "\">" + host + "</a>" : ""
                                return [developer, link].filter(s => s !== "").join(" · ")
                            }
                            textFormat: Text.StyledText
                            color: Kirigami.Theme.disabledTextColor
                            elide: Text.ElideRight
                            onLinkActivated: (link) => Qt.openUrlExternally(link)
                            HoverHandler { cursorShape: parent.hoveredLink ? Qt.PointingHandCursor : Qt.ArrowCursor }
                        }
                    }
                }

                // Actions: Launch, Update to X (or progress), overflow menu
                RowLayout {
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.smallSpacing
                    visible: detailInner.loaded

                    readonly property bool updating: detailInner.item.isUpdating ?? false

                    Controls.Button {
                        text: i18n("Launch")
                        icon.name: "media-playback-start"
                        onClicked: proxyModel.requestLaunch(listView.currentIndex)
                    }

                    Controls.Button {
                        visible: (detailInner.item.updateAvailable ?? false) && !parent.updating
                        text: i18n("Update to %1", detailInner.item.updateVersion ?? "")
                        icon.name: "system-software-update"
                        onClicked: proxyModel.downloadUpdate(listView.currentIndex)
                    }

                    Controls.ProgressBar {
                        visible: parent.updating
                        Layout.preferredWidth: Kirigami.Units.gridUnit * 8
                        from: 0; to: 100
                        value: detailInner.item.updateProgress ?? 0
                        Controls.ToolTip.text: i18n("Updating to %1…", detailInner.item.updateVersion ?? "")
                        Controls.ToolTip.visible: updateProgressHover.hovered
                        HoverHandler { id: updateProgressHover }
                    }

                    Item { Layout.fillWidth: true }

                    Controls.ToolButton {
                        id: moreButton
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
                                text: i18n("Show in App Menu")
                                checkable: true
                                checked: detailInner.item.hasDesktopLink ?? false
                                onTriggered: {
                                    proxyModel.toggleDesktopLink(listView.currentIndex, checked)
                                    showPassiveNotification(checked
                                        ? i18n("Added %1 to the app menu", detailInner.item.displayName ?? "")
                                        : i18n("Removed %1 from the app menu", detailInner.item.displayName ?? ""))
                                }
                            }
                            Controls.MenuItem {
                                icon.name: "configure"
                                text: i18n("Update Source…")
                                onTriggered: updateSourceDialog.openFor(listView.currentIndex,
                                                                        detailInner.item.displayName ?? "")
                            }
                            Controls.MenuItem {
                                icon.name: "code-context"
                                text: i18n("Environment Variables…")
                                onTriggered: environmentDialog.openFor(listView.currentIndex,
                                                                       detailInner.item.displayName ?? "")
                            }
                            Controls.MenuSeparator {}
                            Controls.MenuItem {
                                icon.name: "edit-delete"
                                text: i18n("Remove…")
                                onTriggered: proxyModel.requestRemoveAt(listView.currentIndex)
                            }
                        }
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
                    visible: detailInner.loaded && text !== ""
                    text: detailInner.item.description ?? ""
                    wrapMode: Text.WordWrap
                    textFormat: Text.StyledText
                    lineHeight: 1.15
                }

                // ── Details ───────────────────────────────────────────
                Kirigami.ListSectionHeader {
                    Layout.fillWidth: true
                    Layout.topMargin: Kirigami.Units.largeSpacing
                    visible: detailInner.loaded
                    text: i18n("Details")
                    leftPadding: 0
                }

                GridLayout {
                    id: detailsGrid
                    Layout.fillWidth: true
                    visible: detailInner.loaded
                    columns: 2
                    columnSpacing: Kirigami.Units.largeSpacing
                    rowSpacing: Kirigami.Units.smallSpacing

                    readonly property var rows: {
                        const item = detailInner.item
                        const categories = (item.categories ?? "").split(";").filter(s => s.length > 0)
                        const env = item.environment ?? []
                        const home = Platform.StandardPaths.writableLocation(Platform.StandardPaths.HomeLocation).toString().replace("file://", "")
                        const path = item.filePath ?? ""
                        return [
                            [i18n("Version"), item.version || i18n("Unknown")],
                            [i18n("Size"), item.formattedSize ?? ""],
                            [i18n("Category"), categories.join(", ") || i18n("None")],
                            [i18n("Added"), item.addedDate ? Qt.formatDate(item.addedDate, "d MMM yyyy") : ""],
                            [i18n("Updates"), item.updateSourceLabel || i18n("No update source")],
                            [i18n("Environment"), env.length > 0 ? env.join("\n") : i18n("Default")],
                            [i18n("File"), path.startsWith(home) ? "~" + path.slice(home.length) : path],
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
                            font.family: !isKey && row[0] === i18n("Environment") && (detailInner.item.environment ?? []).length > 0
                                         ? "monospace" : Kirigami.Theme.defaultFont.family
                        }
                    }
                }

                Item { implicitHeight: Kirigami.Units.gridUnit }
            }
        }
    }

    EnvironmentDialog {
        id: environmentDialog
        onSaved: showPassiveNotification(i18n("Environment saved. It applies the next time the app starts."))
    }

    UpdateSourceDialog {
        id: updateSourceDialog
        onSaved: (cleared) => showPassiveNotification(cleared
            ? i18n("Using the built-in update source. Check for updates to look for a new version.")
            : i18n("Update source saved. Check for updates to look for a new version."))
    }

    // ── Keyboard shortcuts (only while this page is shown) ────────────────
    Shortcut {
        sequence: "Return"
        enabled: pageRoot.visible && listView.currentIndex >= 0
        onActivated: proxyModel.requestLaunch(listView.currentIndex)
    }
    Shortcut {
        sequence: "Delete"
        enabled: pageRoot.visible && listView.currentIndex >= 0
        onActivated: proxyModel.requestRemoveAt(listView.currentIndex)
    }
    Shortcut {
        sequence: "Escape"
        enabled: pageRoot.visible && searchField.text !== ""
        onActivated: { searchField.text = ""; proxyModel.filterText = "" }
    }
    Shortcut {
        sequence: "Escape"
        enabled: pageRoot.visible && searchField.text === "" && listView.currentIndex >= 0
        onActivated: listView.currentIndex = -1
    }
}
