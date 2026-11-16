// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kirigami.primitives as Primitives

// Toast at the bottom of the dashboard while updates are checked: "Checking for updates
// (3/13)", counting up, then the result for a few seconds. Looks like Kirigami's passive
// notifications. Failed checks show nothing here: LibraryPage shows those with Retry.
// Context property: listModel.
Item {
    id: toast

    property string resultText: ""

    readonly property bool checking: listModel.checkingUpdates
    readonly property bool shown: checking || resultTimer.running

    implicitWidth: box.implicitWidth
    implicitHeight: box.implicitHeight
    opacity: shown ? 1 : 0
    visible: opacity > 0
    Behavior on opacity { NumberAnimation { duration: Kirigami.Units.longDuration } }

    Timer {
        id: resultTimer
        interval: 4000
    }

    Connections {
        target: listModel
        function onCheckingUpdatesChanged() {
            if (listModel.checkingUpdates)
                resultTimer.stop()
        }
        function onUpdateCheckFinished(updatesFound, networkFailures) {
            if (updatesFound < 0 || (networkFailures > 0 && updatesFound === 0))
                return // shown above the list, with Retry
            toast.resultText = listModel.checkTotal === 0
                ? i18n("No AppImage has an update source")
                : updatesFound === 0 ? i18n("All AppImages are up to date")
                : i18np("1 update available", "%1 updates available", updatesFound)
            resultTimer.restart()
        }
    }

    Controls.Control {
        id: box
        anchors.centerIn: parent
        leftPadding: Kirigami.Units.largeSpacing
        rightPadding: Kirigami.Units.largeSpacing
        topPadding: Kirigami.Units.largeSpacing
        bottomPadding: Kirigami.Units.largeSpacing

        Kirigami.Theme.inherit: false
        Kirigami.Theme.colorSet: Kirigami.Theme.Complementary

        // A click dismisses the result (like a passive notification).
        TapHandler { onTapped: resultTimer.stop() }

        contentItem: RowLayout {
            spacing: Kirigami.Units.mediumSpacing

            Controls.BusyIndicator {
                visible: toast.checking
                running: visible
                implicitWidth: Kirigami.Units.iconSizes.smallMedium
                implicitHeight: Kirigami.Units.iconSizes.smallMedium
            }
            Kirigami.Icon {
                visible: !toast.checking
                source: "checkmark"
                implicitWidth: Kirigami.Units.iconSizes.smallMedium
                implicitHeight: Kirigami.Units.iconSizes.smallMedium
            }
            Controls.Label {
                text: toast.checking
                    ? i18n("Checking for updates (%1/%2)", listModel.checkDone, listModel.checkTotal)
                    : toast.resultText
            }
        }

        background: Primitives.ShadowedRectangle {
            Kirigami.Theme.inherit: false
            Kirigami.Theme.colorSet: Kirigami.Theme.Complementary
            shadow {
                size: Kirigami.Units.gridUnit / 2
                color: Qt.rgba(0, 0, 0, 0.4)
                yOffset: 2
            }
            radius: Kirigami.Units.cornerRadius
            color: Kirigami.Theme.backgroundColor
            opacity: 0.9
        }
    }
}
