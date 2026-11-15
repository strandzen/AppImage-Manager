// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
import QtQuick
import QtQuick.Controls as Controls
import org.kde.kirigami as Kirigami
import appimagemanager

Kirigami.Page {
    id: pageRoot
    title: i18n("About")

    // Fetched when first shown, so starting the dashboard makes no network request.
    property bool contributorsFetched: false
    onVisibleChanged: {
        if (visible && !contributorsFetched) {
            contributorsFetched = true
            aboutContent.fetchContributors()
        }
    }

    Controls.ScrollView {
        id: scrollView
        anchors.fill: parent
        clip: true
        Controls.ScrollBar.horizontal.policy: Controls.ScrollBar.AlwaysOff
        Controls.ScrollBar.vertical.policy: Controls.ScrollBar.AsNeeded

        AboutContent {
            id: aboutContent
            width: scrollView.availableWidth - Kirigami.Units.largeSpacing * 2
            x: Kirigami.Units.largeSpacing
        }
    }
}
