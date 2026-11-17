// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// The tray icon's window (left click): an overview of installed AppImages and their
// update state. Managing happens in the main window; a row opens it with that app selected.
// Context properties: trayModel (TrayModel, for change signals), daemon (UpdateDaemon, State()).
Window {
    id: root

    // TrayModel::State
    readonly property int stateNotChecked: 0
    readonly property int stateUpToDate: 1
    readonly property int stateUpdateAvailable: 2
    readonly property int stateNoSource: 3
    readonly property int stateFailed: 4
    readonly property int stateUpdating: 5
    readonly property int statePinned: 6

    readonly property bool busy: trayModel.checking || trayModel.updatingCount > 0

    // The daemon's State() JSON (as the Plasma tray widget gets it), re-read when the model changes.
    property var daemonState: JSON.parse(daemon.State())
    function refresh() { daemonState = JSON.parse(daemon.State()) }
    Connections {
        target: trayModel
        function onChanged() { root.refresh() }
        function onSummaryChanged() { root.refresh() }
    }

    // AppImages with an update source (updates first), then a "Not tracked (n)" row that
    // folds away those without one. Same as the tray widget.
    property bool notTrackedExpanded: false
    readonly property var rows: {
        const items = daemonState.items ?? []
        // Pinned apps ("Don't Update") fold away with them.
        const untracked = items.filter(item => item.state === stateNoSource || item.state === statePinned)
        const rows = items.filter(item => item.state !== stateNoSource && item.state !== statePinned)
        if (untracked.length > 0) {
            rows.push({ header: true, count: untracked.length })
            if (notTrackedExpanded)
                rows.push(...untracked)
        }
        return rows
    }

    title: i18n("AppImage Manager")
    flags: Qt.FramelessWindowHint | Qt.Tool | Qt.WindowStaysOnTopHint
    visible: false
    width: Kirigami.Units.gridUnit * 22
    height: Math.min(Kirigami.Units.gridUnit * 28,
                     header.implicitHeight + footer.implicitHeight
                     + Math.max(listView.contentHeight, Kirigami.Units.gridUnit * 6) + 2)
    color: Kirigami.Theme.backgroundColor

    Kirigami.Theme.colorSet: Kirigami.Theme.Window
    Kirigami.Theme.inherit: false

    // Like a popup: closes when something else is clicked.
    onActiveChanged: if (!active) daemon.hideTrayWindow()

    Shortcut {
        sequence: "Escape"
        onActivated: daemon.hideTrayWindow()
    }

    Rectangle {
        anchors.fill: parent
        color: "transparent"
        border.width: 1
        border.color: Kirigami.ColorUtils.linearInterpolation(Kirigami.Theme.backgroundColor,
                                                              Kirigami.Theme.textColor, 0.2)
        z: 1
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 1
        spacing: 0

        // ── Header: what is going on, when it was checked ─────────────────
        RowLayout {
            id: header
            Layout.fillWidth: true
            Layout.margins: Kirigami.Units.largeSpacing
            spacing: Kirigami.Units.largeSpacing

            // "No updates available · Checked 14:02" on one line; the next check is in the tooltip.
            // Same font as "Checked …"; the status is not dimmed.
            Controls.Label {
                Layout.alignment: Qt.AlignBaseline
                font: Kirigami.Theme.smallFont
                text: trayModel.checking ? i18n("Checking for updates…")
                    : trayModel.updatingCount > 0 ? i18np("Updating 1 AppImage…", "Updating %1 AppImages…", trayModel.updatingCount)
                    : trayModel.updateCount > 0 ? i18np("1 update available", "%1 updates available", trayModel.updateCount)
                    : i18n("No updates available")
            }
            Controls.Label {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignBaseline
                text: "· " + trayModel.lastCheckText
                color: Kirigami.Theme.disabledTextColor
                font: Kirigami.Theme.smallFont
                elide: Text.ElideRight
            }

            Controls.BusyIndicator {
                implicitWidth: Kirigami.Units.iconSizes.medium
                implicitHeight: Kirigami.Units.iconSizes.medium
                running: root.busy
                visible: running
            }
        }

        Kirigami.Separator { Layout.fillWidth: true }

        // ── Installed AppImages ───────────────────────────────────────────
        Controls.ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ListView {
                id: listView
                clip: true
                model: root.rows

                delegate: Controls.ItemDelegate {
                    id: row
                    required property var modelData
                    readonly property bool isHeader: modelData.header === true

                    width: ListView.view.width
                    topInset: 0
                    bottomInset: 0
                    verticalPadding: Kirigami.Units.smallSpacing
                    horizontalPadding: Kirigami.Units.largeSpacing
                    onClicked: isHeader ? root.notTrackedExpanded = !root.notTrackedExpanded
                                        : daemon.openManager(modelData.path)

                    // Why a check failed, else where a click leads.
                    Controls.ToolTip.text: (modelData.reason ?? "") !== "" ? modelData.reason : i18n("Show in AppImage Manager")
                    Controls.ToolTip.visible: hovered && !isHeader
                    Controls.ToolTip.delay: Kirigami.Units.toolTipDelay

                    contentItem: RowLayout {
                        spacing: Kirigami.Units.largeSpacing

                        // "Not tracked (2)": folds AppImages without an update source.
                        Kirigami.Icon {
                            visible: row.isHeader
                            source: root.notTrackedExpanded ? "arrow-down" : "arrow-right"
                            implicitWidth: Kirigami.Units.iconSizes.small
                            implicitHeight: Kirigami.Units.iconSizes.small
                        }
                        Controls.Label {
                            visible: row.isHeader
                            Layout.fillWidth: true
                            text: i18nc("AppImages without an update source", "Not tracked (%1)", row.modelData.count ?? 0)
                            color: Kirigami.Theme.disabledTextColor
                        }

                        Kirigami.IconTitleSubtitle {
                            visible: !row.isHeader
                            Layout.fillWidth: true
                            icon.source: row.modelData.icon || "application-x-executable"
                            icon.width: Kirigami.Units.iconSizes.medium
                            icon.height: Kirigami.Units.iconSizes.medium
                            title: row.modelData.name ?? ""
                            subtitle: {
                                const d = row.modelData
                                switch (d.state) {
                                case root.stateUpdateAvailable:
                                    return d.version ? i18nc("old → new version", "%1 → %2", d.version, d.newVersion)
                                                     : d.newVersion
                                case root.stateUpdating:     return i18n("Updating to %1…", d.newVersion)
                                case root.stateUpToDate:     return i18n("Up to date")
                                case root.stateNoSource:     return i18n("No update source")
                                case root.statePinned:       return i18nc("app is never updated", "Pinned")
                                case root.stateFailed:
                                    return d.reason ? i18n("Check failed: %1", d.reason) : i18n("Check failed")
                                default:                     return i18n("Not checked yet")
                                }
                            }
                            reserveSpaceForSubtitle: true
                        }

                        Controls.ProgressBar {
                            visible: !row.isHeader && row.modelData.state === root.stateUpdating
                            Layout.preferredWidth: Kirigami.Units.gridUnit * 4
                            from: 0; to: 100
                            value: row.modelData.progress ?? 0
                        }

                        // Update dot, as in the tray widget
                        Rectangle {
                            visible: !row.isHeader && row.modelData.state === root.stateUpdateAvailable
                            implicitWidth: Kirigami.Units.smallSpacing * 2
                            implicitHeight: implicitWidth
                            radius: width / 2
                            color: Kirigami.Theme.positiveTextColor
                        }
                    }
                }

                Kirigami.PlaceholderMessage {
                    anchors.centerIn: parent
                    width: parent.width - Kirigami.Units.gridUnit * 2
                    visible: listView.count === 0
                    icon.name: "appimagemanager"
                    text: i18n("No AppImages installed")
                }
            }
        }

        Kirigami.Separator { Layout.fillWidth: true }

        // ── Actions ───────────────────────────────────────────────────────
        RowLayout {
            id: footer
            Layout.fillWidth: true
            Layout.margins: Kirigami.Units.smallSpacing
            spacing: Kirigami.Units.smallSpacing

            Controls.ToolButton {
                icon.name: "view-refresh"
                text: i18n("Check Now")
                enabled: !root.busy
                onClicked: daemon.checkNow()
            }

            Controls.ToolButton {
                icon.name: "system-software-update"
                text: i18n("Update All")
                visible: trayModel.updateCount > 0
                enabled: !root.busy
                onClicked: daemon.updateAll()
            }

            Item { Layout.fillWidth: true }

            Controls.ToolButton {
                icon.name: "appimagemanager"
                text: i18n("Open AppImage Manager")
                display: Controls.AbstractButton.IconOnly
                onClicked: daemon.openManager("")
                Controls.ToolTip.text: text
                Controls.ToolTip.visible: hovered
                Controls.ToolTip.delay: Kirigami.Units.toolTipDelay
            }

            Controls.ToolButton {
                icon.name: "configure"
                text: i18n("Settings")
                display: Controls.AbstractButton.IconOnly
                onClicked: daemon.openSettings()
                Controls.ToolTip.text: text
                Controls.ToolTip.visible: hovered
                Controls.ToolTip.delay: Kirigami.Units.toolTipDelay
            }
        }
    }
}
