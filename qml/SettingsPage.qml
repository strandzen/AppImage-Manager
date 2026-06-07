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

    actions: [
        Kirigami.Action {
            text: i18n("Restore Defaults")
            icon.name: "document-revert"
            onTriggered: {
                AppSettings.resetToDefaults();
                pathField.text = AppSettings.applicationsPath
            }
        }
    ]

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

            // ── Card 1: Storage & Location ─────────────────────────────────────
            FormCard.FormHeader {
                title: i18n("Storage & Location")
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
                            onClicked: folderDialog.open()
                            Controls.ToolTip.text: i18n("Browse…")
                            Controls.ToolTip.visible: hovered
                        }
                        Controls.Button {
                            icon.name: "system-file-manager"
                            onClicked: Qt.openUrlExternally("file://" + AppSettings.applicationsPath)
                            Controls.ToolTip.text: i18n("Open Folder in Dolphin")
                            Controls.ToolTip.visible: hovered
                        }
                    }
                }
            }

            // ── Card 2: Appearance ─────────────────────────────────────────────
            FormCard.FormHeader {
                title: i18n("Appearance")
            }

            FormCard.FormCard {
                FormCard.FormComboBoxDelegate {
                    text: i18n("Manage window icon size")
                    model: [i18n("Small"), i18n("Medium"), i18n("Large")]
                    currentIndex: AppSettings.manageIconSize
                    onActivated: AppSettings.manageIconSize = currentIndex
                }

                FormCard.FormDelegateSeparator {}

                FormCard.FormSwitchDelegate {
                    id: showInstallBoxCheck
                    text: i18n("Show drag-and-drop install zone at bottom of list")
                    description: i18n("Displays a persistent installation target at the footer of the dashboard application list.")
                    checked: AppSettings.showInstallBox
                    onToggled: AppSettings.showInstallBox = checked
                }

                FormCard.FormDelegateSeparator {}

                FormCard.FormSwitchDelegate {
                    id: accentBordersCheck
                    text: i18n("Show accent borders on cards and drop zones")
                    description: i18n("Outlines visual frames and drop targets with active highlight colors instead of quiet gray outlines.")
                    checked: AppSettings.accentBorders
                    onToggled: AppSettings.accentBorders = checked
                }
            }

            // ── Card 3: Behavior & Security ────────────────────────────────────
            FormCard.FormHeader {
                title: i18n("Behavior & Security")
            }

            FormCard.FormCard {
                FormCard.FormSwitchDelegate {
                    id: verifySignaturesCheck
                    text: i18n("Verify GPG signatures before installing")
                    description: i18n("Checks each AppImage for an embedded GPG signature. Warns on unsigned files; blocks installation of files with invalid signatures unless overridden.")
                    checked: AppSettings.verifySignatures
                    onToggled: AppSettings.verifySignatures = checked
                }

                FormCard.FormDelegateSeparator {}

                FormCard.FormSwitchDelegate {
                    id: showDisclaimerCheck
                    text: i18n("Show security disclaimer")
                    description: i18n("Displays an alert warning about unverified executable files when installing new AppImages.")
                    checked: AppSettings.showDisclaimer
                    onToggled: AppSettings.showDisclaimer = checked
                }

                FormCard.FormDelegateSeparator {}

                FormCard.FormSwitchDelegate {
                    id: showNotificationsCheck
                    text: i18n("Show install/uninstall notifications")
                    description: i18n("Fires native system popups when installation or removal actions complete successfully.")
                    checked: AppSettings.showNotifications
                    onToggled: AppSettings.showNotifications = checked
                }

                FormCard.FormDelegateSeparator {}

                FormCard.FormSwitchDelegate {
                    id: watchDownloadsCheck
                    text: i18n("Notify when an AppImage is downloaded")
                    description: i18n("Monitors your Downloads folder and triggers a native KDE notification with quick installation actions.")
                    checked: AppSettings.watchDownloads
                    onToggled: AppSettings.watchDownloads = checked
                }

                FormCard.AbstractFormDelegate {
                    background: null
                    visible: listModel.downloadWatcherSandboxed
                    contentItem: Kirigami.InlineMessage {
                        Layout.fillWidth: true
                        type: Kirigami.MessageType.Information
                        text: i18n("Download monitoring is unavailable because AppImage Manager is running in a sandbox.")
                        showCloseButton: false
                    }
                }
            }

            // ── Card 4: Background Updates ─────────────────────────────────────
            FormCard.FormHeader {
                title: i18n("Background Updates")
            }

            FormCard.FormCard {
                FormCard.FormComboBoxDelegate {
                    text: i18n("Check for updates")
                    description: i18n("Configures the frequency of background update scans for all registered AppImages on this machine.")
                    model: [i18n("Never"), i18n("Daily"), i18n("Weekly"), i18n("Monthly"), i18n("Custom")]
                    currentIndex: AppSettings.updateFrequency
                    onActivated: AppSettings.updateFrequency = currentIndex
                }

                FormCard.FormDelegateSeparator {
                    opacity: AppSettings.updateFrequency === 4 ? 1.0 : 0.0
                    visible: opacity > 0
                    Behavior on opacity { NumberAnimation { duration: 150; easing.type: Easing.InOutQuad } }
                }

                FormCard.FormSpinBoxDelegate {
                    label: i18n("Custom interval (days)")
                    opacity: AppSettings.updateFrequency === 4 ? 1.0 : 0.0
                    visible: opacity > 0
                    Behavior on opacity { NumberAnimation { duration: 150; easing.type: Easing.InOutQuad } }
                    from: 1
                    to: 365
                    value: AppSettings.customUpdateDays
                    onValueChanged: AppSettings.customUpdateDays = value
                }
            }

            // ── Card 5: AppImage Sources ───────────────────────────────────────
            FormCard.FormHeader {
                title: i18n("AppImage Sources")
            }

            FormCard.FormCard {
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

                        property int index: index

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
                                            delegateLayout.y = index * delegateLayout.height
                                            
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
                                        font.bold: true
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }
                                    Controls.Label {
                                        text: model.url
                                        font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                                        opacity: 0.6
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }
                                    Controls.Label {
                                        text: model.type === "appimagehub"
                                              ? i18n("Format: AppImage Hub (Direct)")
                                              : (model.type === "universal"
                                                 ? i18n("Format: Universal JSON (Direct)")
                                                 : i18n("Format: AM Database (Recipe)"))
                                        font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                                        opacity: 0.6
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

                                Controls.Button {
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

                                Controls.Button {
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
                                    const fromIdx = drag.source.index
                                    const toIdx = index
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

            Item {
                height: Kirigami.Units.largeSpacing
            }
        }
    }

    Kirigami.Dialog {
        id: sourceDialog
        title: isEdit ? i18n("Edit Source") : i18n("Add Source")

        property bool isEdit: false
        property string sourceId: ""

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
                            amStoreModel.updateSource(sourceDialog.sourceId, sourceNameField.text.trim(), sourceUrlField.text.trim(), type, true)
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
