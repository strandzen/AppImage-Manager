// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
import QtQuick
import QtQuick.Layouts
import org.kde.plasma.plasmoid
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.components as PlasmaComponents3
import org.kde.plasma.extras as PlasmaExtras
import org.kde.kirigami as Kirigami
import org.kde.plasma.workspace.dbus as DBus

// System tray entry for AppImage Manager: an overview of installed AppImages and their
// update state. Only a view: the background checker (`appimagemanager --daemon`) checks,
// notifies and updates; this widget reads its State() over D-Bus and calls its actions.
// Strings come from the app's catalog (i18nd "appimagemanager").
PlasmoidItem {
    id: root

    readonly property string service: "io.github.appimagemanager.Daemon"
    readonly property string objectPath: "/Daemon"
    readonly property string iface: "io.github.appimagemanager.Daemon1"

    // UpdateDaemon / TrayModel::State
    readonly property int stateNotChecked: 0
    readonly property int stateUpToDate: 1
    readonly property int stateUpdateAvailable: 2
    readonly property int stateNoSource: 3
    readonly property int stateFailed: 4
    readonly property int stateUpdating: 5

    property var daemonState: ({ updateCount: 0, updatingCount: 0, checking: false, schedule: "", items: [] })
    // False until the background checker answers (D-Bus starts it on our first call).
    property bool reachable: false

    // The list: AppImages with an update source (updates first, as the daemon sorts them),
    // then a "Not tracked (n)" row that folds away those without one.
    property bool notTrackedExpanded: false
    readonly property var rows: {
        const items = daemonState.items ?? []
        const untracked = items.filter(item => item.state === stateNoSource)
        const rows = items.filter(item => item.state !== stateNoSource)
        if (untracked.length > 0) {
            rows.push({ header: true, count: untracked.length })
            if (notTrackedExpanded)
                rows.push(...untracked)
        }
        return rows
    }
    readonly property int updateCount: daemonState.updateCount ?? 0
    readonly property int updatingCount: daemonState.updatingCount ?? 0
    readonly property bool checking: daemonState.checking ?? false
    readonly property bool busy: checking || updatingCount > 0
    readonly property string statusText: checking ? i18nd("appimagemanager", "Checking for updates…")
        : updatingCount > 0 ? i18ndp("appimagemanager", "Updating 1 AppImage…", "Updating %1 AppImages…", updatingCount)
        : updateCount > 0 ? i18ndp("appimagemanager", "1 update available", "%1 updates available", updateCount)
        : i18nd("appimagemanager", "No updates available")

    function call(member, args, callback) {
        const reply = DBus.SessionBus.asyncCall({
            "service": root.service,
            "path": root.objectPath,
            "iface": root.iface,
            "member": member,
            "arguments": args || []
        })
        reply.finished.connect(() => {
            if (member === "State")
                root.reachable = !reply.isError
            if (!reply.isError && callback)
                callback(reply.value)
            // Destroy once finished; the connection would otherwise keep it alive.
            reply.destroy()
        })
    }

    function refresh() {
        call("State", [], value => {
            try {
                root.daemonState = JSON.parse(value)
            } catch (e) {
                console.warn("AppImage Manager: unreadable state", e)
            }
        })
    }

    // Calling the service starts the daemon through D-Bus activation if it is not running.
    Component.onCompleted: {
        call("RegisterView", [])
        refresh()
    }
    Component.onDestruction: call("UnregisterView", [])

    DBus.DBusServiceWatcher {
        busType: DBus.BusType.Session
        watchedService: root.service
        onRegisteredChanged: {
            if (registered) {
                root.call("RegisterView", [])
                root.refresh()
            } else {
                root.reachable = false
            }
        }
    }

    DBus.SignalWatcher {
        busType: DBus.BusType.Session
        service: root.service
        path: root.objectPath
        iface: root.iface
        // SignalWatcher calls "dbus" + the signal's name.
        function dbusStateChanged() { root.refresh() }
        function dbusUpdatesChanged() {} // for the main window; StateChanged follows anyway
    }

    // In case a signal is missed; faster while the popup is open (update progress).
    Timer {
        interval: root.expanded ? 2000 : 60000
        running: true
        repeat: true
        onTriggered: root.refresh()
    }
    onExpandedChanged: if (root.expanded) refresh()

    Plasmoid.icon: "appimagemanager"
    Plasmoid.title: i18nd("appimagemanager", "AppImage Manager")
    // Hidden among the tray's extra items unless something needs a look.
    Plasmoid.status: updateCount > 0 || busy ? PlasmaCore.Types.ActiveStatus : PlasmaCore.Types.PassiveStatus
    toolTipMainText: i18nd("appimagemanager", "AppImage Manager")
    toolTipSubText: statusText + "\n" + (daemonState.schedule ?? "")

    Plasmoid.contextualActions: [
        PlasmaCore.Action {
            text: i18nd("appimagemanager", "Open AppImage Manager")
            icon.name: "appimagemanager"
            onTriggered: root.call("OpenManager", [""])
        },
        PlasmaCore.Action {
            text: i18nd("appimagemanager", "Check for Updates")
            icon.name: "view-refresh"
            enabled: !root.busy
            onTriggered: root.call("CheckNow", [])
        },
        PlasmaCore.Action {
            text: i18nd("appimagemanager", "Settings…")
            icon.name: "configure"
            onTriggered: root.call("OpenSettings", [])
        }
    ]

    compactRepresentation: MouseArea {
        id: compact

        property bool wasExpanded: false

        acceptedButtons: Qt.LeftButton | Qt.MiddleButton
        hoverEnabled: true
        onPressed: mouse => wasExpanded = root.expanded
        onClicked: mouse => {
            if (mouse.button === Qt.MiddleButton)
                root.call("CheckNow", [])
            else
                root.expanded = !wasExpanded
        }

        Kirigami.Icon {
            anchors.fill: parent
            source: Plasmoid.icon
            active: compact.containsMouse
        }

        // Update count, bottom right
        Rectangle {
            visible: root.updateCount > 0
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: Math.max(Kirigami.Units.iconSizes.small * 0.75, countLabel.implicitHeight)
            width: Math.max(height, countLabel.implicitWidth + height / 2)
            radius: height / 2
            color: Kirigami.Theme.highlightColor

            PlasmaComponents3.Label {
                id: countLabel
                anchors.centerIn: parent
                text: root.updateCount > 99 ? "99+" : root.updateCount
                color: Kirigami.Theme.highlightedTextColor
                font.pixelSize: Math.max(7, compact.height * 0.4)
                font.bold: true
            }
        }
    }

    fullRepresentation: PlasmaExtras.Representation {
        Layout.minimumWidth: Kirigami.Units.gridUnit * 18
        Layout.preferredWidth: Kirigami.Units.gridUnit * 22
        Layout.minimumHeight: Kirigami.Units.gridUnit * 14
        Layout.preferredHeight: Kirigami.Units.gridUnit * 24

        collapseMarginsHint: true

        header: PlasmaExtras.PlasmoidHeading {
            contentItem: RowLayout {
                spacing: Kirigami.Units.largeSpacing

                // "No updates available · Checked 14:02" on one line; the next check is in the tooltip.
                // Same font as "Checked …"; the status is not dimmed.
                PlasmaComponents3.Label {
                    Layout.leftMargin: Kirigami.Units.smallSpacing
                    Layout.alignment: Qt.AlignBaseline
                    font: Kirigami.Theme.smallFont
                    text: root.statusText
                }
                PlasmaComponents3.Label {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignBaseline
                    text: "· " + (root.daemonState.lastChecked ?? "")
                    visible: (root.daemonState.lastChecked ?? "") !== ""
                    opacity: 0.7
                    font: Kirigami.Theme.smallFont
                    elide: Text.ElideRight
                }

                PlasmaComponents3.BusyIndicator {
                    implicitWidth: Kirigami.Units.iconSizes.medium
                    implicitHeight: Kirigami.Units.iconSizes.medium
                    running: root.busy
                    visible: running
                }
            }
        }

        footer: PlasmaExtras.PlasmoidHeading {
            position: PlasmaComponents3.ToolBar.Footer
            contentItem: RowLayout {
                spacing: Kirigami.Units.smallSpacing

                PlasmaComponents3.ToolButton {
                    icon.name: "view-refresh"
                    text: i18nd("appimagemanager", "Check Now")
                    enabled: !root.busy
                    onClicked: root.call("CheckNow", [])
                }

                PlasmaComponents3.ToolButton {
                    icon.name: "system-software-update"
                    text: i18nd("appimagemanager", "Update All")
                    visible: root.updateCount > 0
                    enabled: !root.busy
                    onClicked: root.call("UpdateAll", [])
                }

                Item { Layout.fillWidth: true }

                PlasmaComponents3.ToolButton {
                    icon.name: "appimagemanager"
                    text: i18nd("appimagemanager", "Open AppImage Manager")
                    display: PlasmaComponents3.AbstractButton.IconOnly
                    onClicked: { root.expanded = false; root.call("OpenManager", [""]) }
                    PlasmaComponents3.ToolTip.text: text
                    PlasmaComponents3.ToolTip.visible: hovered
                }

                PlasmaComponents3.ToolButton {
                    icon.name: "configure"
                    text: i18nd("appimagemanager", "Settings")
                    display: PlasmaComponents3.AbstractButton.IconOnly
                    onClicked: { root.expanded = false; root.call("OpenSettings", []) }
                    PlasmaComponents3.ToolTip.text: text
                    PlasmaComponents3.ToolTip.visible: hovered
                }
            }
        }

        contentItem: PlasmaComponents3.ScrollView {
            ListView {
                id: listView
                clip: true
                model: root.rows
                highlight: PlasmaExtras.Highlight {}
                highlightMoveDuration: 0
                currentIndex: -1

                delegate: PlasmaComponents3.ItemDelegate {
                    id: row
                    required property var modelData
                    required property int index

                    width: ListView.view.width
                    hoverEnabled: true
                    onHoveredChanged: if (hovered) listView.currentIndex = index
                    readonly property bool isHeader: modelData.header === true

                    onClicked: {
                        if (isHeader) {
                            root.notTrackedExpanded = !root.notTrackedExpanded
                        } else {
                            root.expanded = false
                            root.call("OpenManager", [modelData.path])
                        }
                    }

                    // Why a check failed, on hover.
                    PlasmaComponents3.ToolTip.text: row.modelData.reason ?? ""
                    PlasmaComponents3.ToolTip.visible: hovered && !isHeader && (row.modelData.reason ?? "") !== ""

                    contentItem: RowLayout {
                        spacing: Kirigami.Units.largeSpacing

                        // "Not tracked (2)": folds AppImages without an update source.
                        Kirigami.Icon {
                            visible: row.isHeader
                            source: root.notTrackedExpanded ? "arrow-down" : "arrow-right"
                            Layout.preferredWidth: Kirigami.Units.iconSizes.small
                            Layout.preferredHeight: Kirigami.Units.iconSizes.small
                        }
                        PlasmaComponents3.Label {
                            visible: row.isHeader
                            Layout.fillWidth: true
                            text: i18ndc("appimagemanager", "AppImages without an update source", "Not tracked (%1)", row.modelData.count ?? 0)
                            opacity: 0.7
                        }

                        Kirigami.Icon {
                            visible: !row.isHeader
                            source: row.modelData.icon || "application-x-executable"
                            fallback: "application-x-executable"
                            Layout.preferredWidth: Kirigami.Units.iconSizes.medium
                            Layout.preferredHeight: Kirigami.Units.iconSizes.medium
                        }

                        ColumnLayout {
                            visible: !row.isHeader
                            Layout.fillWidth: true
                            spacing: 0

                            PlasmaComponents3.Label {
                                Layout.fillWidth: true
                                text: row.modelData.name ?? ""
                                elide: Text.ElideRight
                            }
                            PlasmaComponents3.Label {
                                Layout.fillWidth: true
                                opacity: 0.7
                                font: Kirigami.Theme.smallFont
                                elide: Text.ElideRight
                                text: {
                                    const d = row.modelData
                                    switch (d.state) {
                                    case root.stateUpdateAvailable:
                                        return d.version ? i18ndc("appimagemanager", "old → new version", "%1 → %2", d.version, d.newVersion)
                                                         : d.newVersion
                                    case root.stateUpdating: return i18nd("appimagemanager", "Updating to %1…", d.newVersion)
                                    case root.stateUpToDate: return i18nd("appimagemanager", "Up to date")
                                    case root.stateNoSource: return i18nd("appimagemanager", "No update source")
                                    case root.stateFailed:
                                        return d.reason ? i18nd("appimagemanager", "Check failed: %1", d.reason)
                                                        : i18nd("appimagemanager", "Check failed")
                                    default:                 return i18nd("appimagemanager", "Not checked yet")
                                    }
                                }
                            }
                        }

                        PlasmaComponents3.ProgressBar {
                            visible: row.modelData.state === root.stateUpdating
                            Layout.preferredWidth: Kirigami.Units.gridUnit * 4
                            from: 0; to: 100
                            value: row.modelData.progress ?? 0
                        }

                        Rectangle {
                            visible: row.modelData.state === root.stateUpdateAvailable
                            implicitWidth: Kirigami.Units.smallSpacing * 2
                            implicitHeight: implicitWidth
                            radius: width / 2
                            color: Kirigami.Theme.positiveTextColor
                        }
                    }
                }

                PlasmaExtras.PlaceholderMessage {
                    anchors.centerIn: parent
                    width: parent.width - Kirigami.Units.gridUnit * 2
                    visible: listView.count === 0
                    iconName: root.reachable ? "appimagemanager" : "dialog-warning"
                    text: root.reachable ? i18nd("appimagemanager", "No AppImages installed")
                                         : i18nd("appimagemanager", "The background checker is not running")
                    explanation: root.reachable ? ""
                        : i18nd("appimagemanager", "It starts when you log in. To start it now, run: appimagemanager --daemon")
                }
            }
        }
    }
}
