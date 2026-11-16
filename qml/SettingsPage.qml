// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import QtQuick.Dialogs
import appimagemanager

// Settings, laid out like a KDE settings module: a sidebar of pages next to a centered
// Kirigami.FormLayout ("Label:" + control, section separators). Restore Defaults sits under
// the sidebar and applies to every page.
Kirigami.Page {
    id: pageRoot

    padding: 0

    readonly property real sidebarWidth: Kirigami.Units.gridUnit * 10
    readonly property var pages: [
        { name: i18n("General"),  icon: "configure" },
        { name: i18n("Updates"),  icon: "system-software-update" },
        { name: i18n("Security"), icon: "security-high" },
        { name: i18n("Discover"), icon: "plasma-discover" },
    ]

    // One settings page: a scrollable column, capped to formWidth and centered (as in Sirkel).
    component FormPage: Controls.ScrollView {
        id: formPage

        default property alias content: column.data
        readonly property real formWidth: Kirigami.Units.gridUnit * 30

        contentWidth: availableWidth
        Controls.ScrollBar.horizontal.policy: Controls.ScrollBar.AlwaysOff

        Item {
            width: parent.width
            implicitHeight: column.implicitHeight + Kirigami.Units.largeSpacing * 2

            ColumnLayout {
                id: column
                width: Math.min(parent.width - Kirigami.Units.largeSpacing * 2, formPage.formWidth)
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top
                anchors.topMargin: Kirigami.Units.largeSpacing
                spacing: Kirigami.Units.largeSpacing
            }
        }
    }

    // A dim explanation under a control, aligned with the form's fields.
    component Hint: Controls.Label {
        Layout.fillWidth: true
        Layout.preferredWidth: Kirigami.Units.gridUnit * 16
        wrapMode: Text.WordWrap
        color: Kirigami.Theme.disabledTextColor
        font: Kirigami.Theme.smallFont
    }

    property bool isDraggingSources: false

    ListModel {
        id: localSourcesModel
    }

    function syncLocalModel() {
        localSourcesModel.clear();
        var srcList = amStoreModel.sources;
        for (var i = 0; i < srcList.length; i++) {
            localSourcesModel.append(srcList[i]);
        }
    }

    Component.onCompleted: {
        syncLocalModel();
    }

    Connections {
        target: amStoreModel
        function onSourcesChanged() {
            if (!pageRoot.isDraggingSources) {
                Qt.callLater(pageRoot.syncLocalModel);
            }
        }
    }

    Connections {
        target: AppSettings
        function onApplicationsPathError(badPath) {
            errorMsg.text = i18n("Cannot create folder: %1", badPath)
            errorMsg.visible = true
            pathField.text = AppSettings.applicationsPath
        }
        function onApplicationsPathChanged() {
            errorMsg.visible = false
            pathField.text = AppSettings.applicationsPath
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // ── Sidebar ───────────────────────────────────────────────────────────
        Rectangle {
            Layout.fillHeight: true
            Layout.preferredWidth: pageRoot.sidebarWidth
            Kirigami.Theme.colorSet: Kirigami.Theme.View
            Kirigami.Theme.inherit: false
            color: Kirigami.Theme.backgroundColor

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                Controls.ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    ListView {
                        id: sidebar
                        model: pageRoot.pages
                        currentIndex: 0
                        clip: true
                        topMargin: Kirigami.Units.smallSpacing

                        delegate: Controls.ItemDelegate {
                            required property var modelData
                            required property int index
                            width: ListView.view.width
                            // Same geometry for every row (the style insets the last row differently).
                            topInset: 0
                            bottomInset: 0
                            leftInset: Kirigami.Units.smallSpacing
                            rightInset: Kirigami.Units.smallSpacing
                            text: modelData.name
                            icon.name: modelData.icon
                            highlighted: ListView.isCurrentItem
                            onClicked: sidebar.currentIndex = index
                        }
                    }
                }

                Kirigami.Separator { Layout.fillWidth: true }

                Controls.ItemDelegate {
                    Layout.fillWidth: true
                    leftInset: Kirigami.Units.smallSpacing
                    rightInset: Kirigami.Units.smallSpacing
                    icon.name: "document-revert"
                    text: i18n("Restore Defaults")
                    onClicked: {
                        AppSettings.resetToDefaults()
                        pathField.text = AppSettings.applicationsPath
                    }
                    Controls.ToolTip.text: i18n("Restores every setting on all pages. Catalogs are reset under Discover.")
                    Controls.ToolTip.visible: hovered
                    Controls.ToolTip.delay: Kirigami.Units.toolTipDelay
                }
            }
        }

        Kirigami.Separator { Layout.fillHeight: true }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: sidebar.currentIndex

            // ── General ───────────────────────────────────────────────────────
            FormPage {
                Kirigami.InlineMessage {
                    id: errorMsg
                    Layout.fillWidth: true
                    type: Kirigami.MessageType.Error
                    visible: false
                    showCloseButton: true
                }

                Kirigami.FormLayout {
                    Layout.fillWidth: true

                    Kirigami.Separator {
                        Kirigami.FormData.isSection: true
                        Kirigami.FormData.label: i18n("Applications")
                    }

                    RowLayout {
                        Kirigami.FormData.label: i18n("Folder:")
                        spacing: Kirigami.Units.smallSpacing

                        Controls.TextField {
                            id: pathField
                            Layout.preferredWidth: Kirigami.Units.gridUnit * 14
                            text: AppSettings.applicationsPath
                            onEditingFinished: AppSettings.applicationsPath = text
                        }
                        Controls.Button {
                            icon.name: "folder-open"
                            text: i18n("Browse…")
                            display: Controls.AbstractButton.IconOnly
                            onClicked: folderDialog.open()
                            Controls.ToolTip.text: text
                            Controls.ToolTip.visible: hovered
                            Controls.ToolTip.delay: Kirigami.Units.toolTipDelay
                        }
                        Controls.Button {
                            icon.name: "system-file-manager"
                            text: i18n("Open Folder")
                            display: Controls.AbstractButton.IconOnly
                            onClicked: Qt.openUrlExternally("file://" + AppSettings.applicationsPath)
                            Controls.ToolTip.text: text
                            Controls.ToolTip.visible: hovered
                            Controls.ToolTip.delay: Kirigami.Units.toolTipDelay
                        }
                    }

                    Controls.Switch {
                        Kirigami.FormData.label: i18n("Drop zone:")
                        text: i18n("Show below the Installed list")
                        checked: AppSettings.showInstallBox
                        onToggled: AppSettings.showInstallBox = checked
                    }

                    Controls.Switch {
                        Kirigami.FormData.label: i18n("Overview:")
                        text: i18n("Show a summary when no AppImage is selected")
                        checked: AppSettings.showOverview
                        onToggled: AppSettings.showOverview = checked
                    }
                    Hint {
                        text: i18n("How many AppImages are up to date, have updates or no update source, and when they were last checked.")
                    }

                    Kirigami.Separator {
                        Kirigami.FormData.isSection: true
                        Kirigami.FormData.label: i18n("Notifications")
                    }

                    Controls.Switch {
                        Kirigami.FormData.label: i18n("Downloads:")
                        text: i18n("Notify when an AppImage is downloaded")
                        checked: AppSettings.watchDownloads
                        enabled: !listModel.downloadWatcherSandboxed
                        onToggled: AppSettings.watchDownloads = checked
                    }
                    Hint {
                        text: listModel.downloadWatcherSandboxed
                              ? i18n("Download watching is unavailable in sandboxed environments.")
                              : i18n("Watches your Downloads folder.")
                    }

                    Controls.Switch {
                        Kirigami.FormData.label: i18n("Installs and removals:")
                        text: i18n("Notify when they finish")
                        checked: AppSettings.showNotifications
                        onToggled: AppSettings.showNotifications = checked
                    }
                }
            }

            // ── Updates ───────────────────────────────────────────────────────
            FormPage {
                Kirigami.FormLayout {
                    Layout.fillWidth: true

                    Kirigami.Separator {
                        Kirigami.FormData.isSection: true
                        Kirigami.FormData.label: i18n("Background checks")
                    }

                    Controls.ComboBox {
                        Kirigami.FormData.label: i18n("Check for updates:")
                        Layout.preferredWidth: Kirigami.Units.gridUnit * 14
                        model: [i18n("Never"), i18n("Daily"), i18n("Weekly"), i18n("Monthly"), i18n("Custom")]
                        currentIndex: AppSettings.updateFrequency
                        onActivated: AppSettings.updateFrequency = currentIndex
                    }

                    Controls.SpinBox {
                        Kirigami.FormData.label: i18n("Every:")
                        visible: AppSettings.updateFrequency === 4
                        from: 1
                        to: 365
                        value: AppSettings.customUpdateDays
                        textFromValue: (value, locale) => i18np("%1 day", "%1 days", value)
                        valueFromText: (text, locale) => parseInt(text) || 1
                        onValueModified: AppSettings.customUpdateDays = value
                    }

                    Hint {
                        text: i18n("Checks run in the background, also while this window is closed. The tray shows the result and you are notified once about each new version.")
                    }
                }
            }

            // ── Security ──────────────────────────────────────────────────────
            FormPage {
                Kirigami.FormLayout {
                    Layout.fillWidth: true

                    Kirigami.Separator {
                        Kirigami.FormData.isSection: true
                        Kirigami.FormData.label: i18n("Installing")
                    }

                    Controls.Switch {
                        Kirigami.FormData.label: i18n("Signatures:")
                        text: i18n("Verify before installing")
                        checked: AppSettings.verifySignatures
                        onToggled: AppSettings.verifySignatures = checked
                    }
                    Hint {
                        text: i18n("Warns before installing an AppImage that is unsigned or has an invalid signature. Needs gpg.")
                    }

                    Controls.Switch {
                        Kirigami.FormData.label: i18n("Reminder:")
                        text: i18n("Show that AppImages are unverified")
                        checked: AppSettings.showDisclaimer
                        onToggled: AppSettings.showDisclaimer = checked
                    }
                }
            }

            // ── Discover ──────────────────────────────────────────────────────
            FormPage {
                Kirigami.FormLayout {
                    Layout.fillWidth: true

                    Kirigami.Separator {
                        Kirigami.FormData.isSection: true
                        Kirigami.FormData.label: i18n("Discover")
                    }

                    Controls.Switch {
                        Kirigami.FormData.label: i18n("App catalogs:")
                        text: i18n("Search and install from online catalogs")
                        checked: AppSettings.discoverEnabled
                        onToggled: AppSettings.discoverEnabled = checked
                    }
                    Hint {
                        text: i18n("The catalogs are downloaded from the internet when you open Discover.")
                    }
                }

                // Catalogs: a framed list under the form (drag to reorder).
                ColumnLayout {
                    Layout.fillWidth: true
                    visible: AppSettings.discoverEnabled
                    spacing: Kirigami.Units.smallSpacing

                    Kirigami.ListSectionHeader {
                        Layout.fillWidth: true
                        text: i18n("Catalogs")
                    }

                    Hint {
                        text: i18n("Drag to change the order. An app that is in several catalogs is installed from the first one.")
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: sourcesListView.implicitHeight + 2
                        Kirigami.Theme.colorSet: Kirigami.Theme.View
                        Kirigami.Theme.inherit: false
                        color: Kirigami.Theme.backgroundColor
                        radius: Kirigami.Units.cornerRadius
                        border.width: 1
                        border.color: Kirigami.ColorUtils.linearInterpolation(Kirigami.Theme.backgroundColor,
                                                                              Kirigami.Theme.textColor, 0.15)

                        ListView {
                            id: sourcesListView
                            anchors.fill: parent
                            anchors.margins: 1
                            implicitHeight: contentHeight
                            interactive: false
                            clip: true
                            model: localSourcesModel

                            move: Transition {
                                NumberAnimation { properties: "x,y"; duration: 200; easing.type: Easing.InOutQuad }
                            }
                            displaced: Transition {
                                NumberAnimation { properties: "x,y"; duration: 200; easing.type: Easing.InOutQuad }
                            }

                            delegate: ColumnLayout {
                                id: delegateLayout
                                width: sourcesListView.width
                                spacing: 0

                                // Not named "index": a property can't bind to the delegate's own index of the same name.
                                readonly property int itemIndex: index

                                Drag.active: dragMouseArea.drag.active
                                Drag.source: delegateLayout
                                Drag.hotSpot.x: width / 2
                                Drag.hotSpot.y: height / 2

                                states: [
                                    State {
                                        when: dragMouseArea.drag.active
                                        ParentChange {
                                            target: delegateLayout
                                            parent: sourcesListView
                                        }
                                    }
                                ]

                                Item {
                                    Layout.fillWidth: true
                                    implicitHeight: sourceRow.implicitHeight + Kirigami.Units.smallSpacing * 2

                                    Rectangle {
                                        anchors.fill: parent
                                        visible: dragMouseArea.drag.active
                                        color: Kirigami.Theme.alternateBackgroundColor
                                        border.color: Kirigami.Theme.highlightColor
                                        border.width: 1
                                        radius: Kirigami.Units.cornerRadius
                                    }

                                    RowLayout {
                                        id: sourceRow
                                        anchors.fill: parent
                                        anchors.leftMargin: Kirigami.Units.smallSpacing
                                        anchors.rightMargin: Kirigami.Units.smallSpacing
                                        spacing: Kirigami.Units.smallSpacing

                                        // Drag handle
                                        Controls.ToolButton {
                                            icon.name: "handle-sort"
                                            display: Controls.AbstractButton.IconOnly
                                            opacity: 0.6

                                            MouseArea {
                                                id: dragMouseArea
                                                anchors.fill: parent
                                                drag.target: delegateLayout
                                                drag.axis: Drag.YAxis
                                                cursorShape: drag.active ? Qt.ClosedHandCursor : Qt.OpenHandCursor
                                                onPressed: pageRoot.isDraggingSources = true
                                                onReleased: {
                                                    pageRoot.isDraggingSources = false
                                                    delegateLayout.Drag.drop()
                                                    delegateLayout.y = delegateLayout.itemIndex * delegateLayout.height

                                                    var ids = [];
                                                    for (var i = 0; i < localSourcesModel.count; i++) {
                                                        ids.push(localSourcesModel.get(i).id);
                                                    }
                                                    amStoreModel.setSourcesOrder(ids);
                                                }
                                            }
                                            Controls.ToolTip.text: i18n("Drag to reorder")
                                            Controls.ToolTip.visible: hovered
                                            Controls.ToolTip.delay: Kirigami.Units.toolTipDelay
                                        }

                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: 0
                                            Controls.Label {
                                                text: model.name
                                                elide: Text.ElideRight
                                                Layout.fillWidth: true
                                            }
                                            Controls.Label {
                                                text: model.url
                                                font: Kirigami.Theme.smallFont
                                                color: Kirigami.Theme.disabledTextColor
                                                elide: Text.ElideMiddle
                                                Layout.fillWidth: true
                                            }
                                        }

                                        Controls.Switch {
                                            checked: model.enabled
                                            onToggled: amStoreModel.updateSource(model.id, model.name, model.url, model.type, checked)
                                            Controls.ToolTip.text: checked ? i18n("Searched") : i18n("Not searched")
                                            Controls.ToolTip.visible: hovered
                                            Controls.ToolTip.delay: Kirigami.Units.toolTipDelay
                                        }

                                        Controls.ToolButton {
                                            icon.name: "edit-entry"
                                            text: i18n("Edit Source")
                                            display: Controls.AbstractButton.IconOnly
                                            onClicked: sourceDialog.openForEdit({
                                                "id": model.id,
                                                "name": model.name,
                                                "url": model.url,
                                                "type": model.type,
                                                "enabled": model.enabled
                                            })
                                            Controls.ToolTip.text: text
                                            Controls.ToolTip.visible: hovered
                                            Controls.ToolTip.delay: Kirigami.Units.toolTipDelay
                                        }

                                        Controls.ToolButton {
                                            icon.name: "edit-delete"
                                            text: i18n("Delete Source")
                                            display: Controls.AbstractButton.IconOnly
                                            onClicked: amStoreModel.removeSource(model.id)
                                            Controls.ToolTip.text: text
                                            Controls.ToolTip.visible: hovered
                                            Controls.ToolTip.delay: Kirigami.Units.toolTipDelay
                                        }
                                    }

                                    DropArea {
                                        anchors.fill: parent
                                        onEntered: (drag) => {
                                            const fromIdx = drag.source.itemIndex
                                            const toIdx = delegateLayout.itemIndex
                                            if (fromIdx !== toIdx) {
                                                localSourcesModel.move(fromIdx, toIdx, 1)
                                            }
                                        }
                                    }
                                }

                                Kirigami.Separator {
                                    Layout.fillWidth: true
                                    visible: delegateLayout.itemIndex < localSourcesModel.count - 1
                                }
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Kirigami.Units.smallSpacing

                        Controls.Button {
                            text: i18n("Add Catalog…")
                            icon.name: "list-add"
                            onClicked: sourceDialog.openForAdd()
                        }

                        Item { Layout.fillWidth: true }

                        Controls.Button {
                            text: i18n("Reset Catalogs")
                            icon.name: "document-revert"
                            onClicked: amStoreModel.resetSourcesToDefault()
                        }
                    }
                }
            }
        }
    }

    Kirigami.Dialog {
        id: sourceDialog
        title: isEdit ? i18n("Edit Source") : i18n("Add Source")

        property bool isEdit: false
        property string sourceId: ""
        property bool sourceEnabled: true

        padding: Kirigami.Units.largeSpacing
        preferredWidth: Kirigami.Units.gridUnit * 24

        ColumnLayout {
            spacing: Kirigami.Units.largeSpacing
            width: parent.width

            ColumnLayout {
                spacing: Kirigami.Units.smallSpacing
                Layout.fillWidth: true

                Controls.Label {
                    text: i18n("Name:")
                    font.bold: true
                }
                Controls.TextField {
                    id: sourceNameField
                    Layout.fillWidth: true
                    placeholderText: i18n("e.g. My Repo")
                }
            }

            ColumnLayout {
                spacing: Kirigami.Units.smallSpacing
                Layout.fillWidth: true

                Controls.Label {
                    text: i18n("Feed URL:")
                    font.bold: true
                }
                Controls.TextField {
                    id: sourceUrlField
                    Layout.fillWidth: true
                    placeholderText: i18n("e.g. https://example.com/feed.json")
                }
                Controls.Label {
                    text: i18n("Must be a valid HTTP/HTTPS URL")
                    color: Kirigami.Theme.negativeTextColor
                    font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                    visible: sourceUrlField.text.trim().length > 0 && !/^https?:\/\/\S+$/.test(sourceUrlField.text.trim())
                }
            }

            ColumnLayout {
                spacing: Kirigami.Units.smallSpacing
                Layout.fillWidth: true

                Controls.Label {
                    text: i18n("Format Type:")
                    font.bold: true
                }
                Controls.ComboBox {
                    id: sourceTypeCombo
                    Layout.fillWidth: true
                    textRole: "text"
                    model: [
                        { "text": i18n("AppImage Hub JSON (Direct Download)"), "value": "appimagehub" },
                        { "text": i18n("AM Database Text (Recipe Script)"), "value": "am-database" },
                        { "text": i18n("Universal JSON Feed (Direct Download)"), "value": "universal" }
                    ]
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: Kirigami.Units.largeSpacing
                spacing: Kirigami.Units.smallSpacing

                Item {
                    Layout.fillWidth: true
                }

                Controls.Button {
                    text: i18n("Cancel")
                    icon.name: "dialog-cancel"
                    onClicked: sourceDialog.close()
                }

                Controls.Button {
                    text: sourceDialog.isEdit ? i18n("Save") : i18n("Add")
                    icon.name: sourceDialog.isEdit ? "document-save" : "dialog-ok"
                    highlighted: true
                    enabled: sourceNameField.text.trim().length > 0 && /^https?:\/\/\S+$/.test(sourceUrlField.text.trim())
                    onClicked: {
                        const type = sourceTypeCombo.model[sourceTypeCombo.currentIndex].value
                        if (sourceDialog.isEdit) {
                            amStoreModel.updateSource(sourceDialog.sourceId, sourceNameField.text.trim(), sourceUrlField.text.trim(), type, sourceDialog.sourceEnabled)
                        } else {
                            amStoreModel.addSource(sourceNameField.text.trim(), sourceUrlField.text.trim(), type, true)
                        }
                        sourceDialog.close()
                    }
                }
            }
        }

        function openForAdd() {
            isEdit = false
            sourceId = ""
            sourceNameField.text = ""
            sourceUrlField.text = ""
            sourceTypeCombo.currentIndex = 0
            open()
        }

        function openForEdit(sourceData) {
            isEdit = true
            sourceId = sourceData.id
            sourceEnabled = sourceData.enabled
            sourceNameField.text = sourceData.name
            sourceUrlField.text = sourceData.url
            if (sourceData.type === "am-database") {
                sourceTypeCombo.currentIndex = 1
            } else if (sourceData.type === "universal") {
                sourceTypeCombo.currentIndex = 2
            } else {
                sourceTypeCombo.currentIndex = 0
            }
            open()
        }
    }

    FolderDialog {
        id: folderDialog
        title: i18n("Select Applications Folder")
        currentFolder: "file://" + AppSettings.applicationsPath
        onAccepted: AppSettings.setApplicationsPathFromUrl(folderDialog.selectedFolder)
    }
}
