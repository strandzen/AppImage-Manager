// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard
import QtQuick.Dialogs
import appimagemanager

Kirigami.Page {
    id: pageRoot
    title: i18n("Settings")

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

            Kirigami.InlineMessage {
                id: errorMsg
                Layout.fillWidth: true
                Layout.margins: Kirigami.Units.largeSpacing
                type: Kirigami.MessageType.Error
                visible: false
                showCloseButton: true
            }

            // ── General ───────────────────────────────────────────────────────
            FormCard.FormHeader {
                title: i18n("General")
            }

            FormCard.FormCard {
                FormCard.AbstractFormDelegate {
                    background: null
                    contentItem: RowLayout {
                        spacing: Kirigami.Units.smallSpacing
                        Controls.Label {
                            text: i18n("Applications folder:")
                        }
                        Controls.TextField {
                            id: pathField
                            Layout.fillWidth: true
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
                }

                FormCard.FormDelegateSeparator {}

                FormCard.FormSwitchDelegate {
                    text: i18n("Show a drop zone below the Installed list")
                    checked: AppSettings.showInstallBox
                    onToggled: AppSettings.showInstallBox = checked
                }

                FormCard.FormDelegateSeparator {}

                FormCard.FormSwitchDelegate {
                    text: i18n("Notify when an AppImage is downloaded")
                    description: i18n("Watches your Downloads folder.")
                    checked: AppSettings.watchDownloads
                    enabled: !listModel.downloadWatcherSandboxed
                    onToggled: AppSettings.watchDownloads = checked
                }

                FormCard.AbstractFormDelegate {
                    background: null
                    visible: listModel.downloadWatcherSandboxed
                    contentItem: Kirigami.InlineMessage {
                        Layout.fillWidth: true
                        type: Kirigami.MessageType.Information
                        text: i18n("Download watching is unavailable in sandboxed environments.")
                        showCloseButton: false
                    }
                }

                FormCard.FormDelegateSeparator {}

                FormCard.FormSwitchDelegate {
                    text: i18n("Notify when installs and removals finish")
                    checked: AppSettings.showNotifications
                    onToggled: AppSettings.showNotifications = checked
                }
            }

            // ── Updates ───────────────────────────────────────────────────────
            FormCard.FormHeader {
                title: i18n("Updates")
            }

            FormCard.FormCard {
                FormCard.FormComboBoxDelegate {
                    text: i18n("Check for updates in the background")
                    model: [i18n("Never"), i18n("Daily"), i18n("Weekly"), i18n("Monthly"), i18n("Custom")]
                    currentIndex: AppSettings.updateFrequency
                    onActivated: AppSettings.updateFrequency = currentIndex
                }

                FormCard.FormDelegateSeparator {
                    visible: AppSettings.updateFrequency === 4
                }

                FormCard.FormSpinBoxDelegate {
                    label: i18n("Every (days)")
                    visible: AppSettings.updateFrequency === 4
                    from: 1
                    to: 365
                    value: AppSettings.customUpdateDays
                    onValueChanged: AppSettings.customUpdateDays = value
                }
            }

            // ── Security ──────────────────────────────────────────────────────
            FormCard.FormHeader {
                title: i18n("Security")
            }

            FormCard.FormCard {
                FormCard.FormSwitchDelegate {
                    text: i18n("Verify signatures before installing")
                    description: i18n("Warns before installing an AppImage that is unsigned or has an invalid signature.")
                    checked: AppSettings.verifySignatures
                    onToggled: AppSettings.verifySignatures = checked
                }

                FormCard.FormDelegateSeparator {}

                FormCard.FormSwitchDelegate {
                    text: i18n("Show a reminder that AppImages are unverified")
                    checked: AppSettings.showDisclaimer
                    onToggled: AppSettings.showDisclaimer = checked
                }
            }

            // ── Discover ──────────────────────────────────────────────────────
            FormCard.FormHeader {
                title: i18n("Discover")
            }

            FormCard.FormCard {
                FormCard.FormSwitchDelegate {
                    text: i18n("Enable Discover")
                    description: i18n("Search and install apps from online catalogs. The catalogs are downloaded from the internet.")
                    checked: AppSettings.discoverEnabled
                    onToggled: AppSettings.discoverEnabled = checked
                }
            }

            FormCard.FormHeader {
                visible: AppSettings.discoverEnabled
                title: i18n("Catalogs")
            }

            FormCard.FormCard {
                visible: AppSettings.discoverEnabled

                ListView {
                    id: sourcesListView
                    Layout.fillWidth: true
                    implicitHeight: contentHeight
                    interactive: false
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
                                AnchorChanges {
                                    target: delegateLayout
                                    anchors.horizontalCenter: undefined
                                    anchors.verticalCenter: undefined
                                }
                            }
                        ]

                        FormCard.AbstractFormDelegate {
                            id: delegateItem
                            Layout.fillWidth: true
                            background: Rectangle {
                                color: dragMouseArea.drag.active ? Kirigami.Theme.alternateBackgroundColor : "transparent"
                                border.color: dragMouseArea.drag.active ? Kirigami.Theme.highlightColor : "transparent"
                                border.width: dragMouseArea.drag.active ? 1 : 0
                                radius: Kirigami.Units.smallSpacing
                            }

                            contentItem: RowLayout {
                                spacing: Kirigami.Units.smallSpacing

                                // Drag handle
                                Controls.ToolButton {
                                    id: dragHandle
                                    icon.name: "handle-sort"
                                    display: Controls.AbstractButton.IconOnly
                                    opacity: 0.6

                                    MouseArea {
                                        id: dragMouseArea
                                        anchors.fill: parent
                                        drag.target: delegateLayout
                                        drag.axis: Drag.YAxis
                                        cursorShape: drag.active ? Qt.ClosedHandCursor : Qt.OpenHandCursor
                                        onPressed: {
                                            pageRoot.isDraggingSources = true
                                        }
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
                                    Controls.ToolTip.text: i18n("Drag to reorder source priority")
                                    Controls.ToolTip.visible: hovered
                                    Controls.ToolTip.delay: Kirigami.Units.toolTipDelay
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: Kirigami.Units.smallSpacing / 2
                                    Controls.Label {
                                        text: model.name
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }
                                    Controls.Label {
                                        text: model.url
                                        font: Kirigami.Theme.smallFont
                                        color: Kirigami.Theme.disabledTextColor
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }
                                }

                                Controls.Switch {
                                    checked: model.enabled
                                    onToggled: {
                                        amStoreModel.updateSource(model.id, model.name, model.url, model.type, checked)
                                    }
                                }

                                Controls.ToolButton {
                                    icon.name: "edit"
                                    display: Controls.AbstractButton.IconOnly
                                    onClicked: sourceDialog.openForEdit({
                                        "id": model.id,
                                        "name": model.name,
                                        "url": model.url,
                                        "type": model.type,
                                        "enabled": model.enabled
                                    })
                                    Controls.ToolTip.text: i18n("Edit Source")
                                    Controls.ToolTip.visible: hovered
                                }

                                Controls.ToolButton {
                                    icon.name: "delete"
                                    display: Controls.AbstractButton.IconOnly
                                    onClicked: amStoreModel.removeSource(model.id)
                                    Controls.ToolTip.text: i18n("Delete Source")
                                    Controls.ToolTip.visible: hovered
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

                        FormCard.FormDelegateSeparator {
                            visible: index < localSourcesModel.count - 1
                        }
                    }
                }

                FormCard.FormDelegateSeparator {
                    visible: amStoreModel.sources.length > 0
                }

                FormCard.AbstractFormDelegate {
                    background: null
                    contentItem: RowLayout {
                        spacing: Kirigami.Units.largeSpacing
                        Layout.fillWidth: true

                        Controls.Button {
                            text: i18n("Add Source…")
                            icon.name: "list-add"
                            onClicked: sourceDialog.openForAdd()
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        Controls.Button {
                            text: i18n("Reset Sources")
                            icon.name: "document-revert"
                            onClicked: amStoreModel.resetSourcesToDefault()
                        }
                    }
                }
            }

            FormCard.FormCard {
                Layout.topMargin: Kirigami.Units.gridUnit
                FormCard.FormButtonDelegate {
                    icon.name: "document-revert"
                    text: i18n("Restore Defaults")
                    onClicked: {
                        AppSettings.resetToDefaults()
                        pathField.text = AppSettings.applicationsPath
                    }
                }
            }

            Item {
                implicitHeight: Kirigami.Units.gridUnit
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
