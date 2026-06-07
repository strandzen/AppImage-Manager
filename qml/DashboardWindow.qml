// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import appimagemanager

Kirigami.ApplicationWindow {
    id: root
    title: i18n("AppImage Dashboard")
    width: Kirigami.Units.gridUnit * 50
    height: Kirigami.Units.gridUnit * 30
    minimumWidth: Kirigami.Units.gridUnit * 40
    minimumHeight: Kirigami.Units.gridUnit * 22

    Kirigami.Theme.colorSet: Kirigami.Theme.Window
    Kirigami.Theme.inherit: false

    readonly property color cardBorderColor: Theme.cardBorderColor
    property bool isSidebarCollapsed: false
    property int activeNavIndex: 1

    function handleDrop(drop) {
        for (const url of drop.urls) {
            if (url.toString().toLowerCase().endsWith(".appimage"))
                dashboardController.installFromPath(url)
        }
    }

    // ── Update check result ───────────────────────────────────────────────────
    Connections {
        target: listModel
        function onUpdateCheckFinished(updatesFound, networkFailures) {
            if (updatesFound < 0)
                checkTimedOutDialog.open()
            else if (networkFailures > 0 && updatesFound === 0)
                networkErrorDialog.open()
            else if (updatesFound === 0)
                noUpdatesDialog.open()
        }
    }

    Kirigami.PromptDialog {
        id: noUpdatesDialog
        title: i18n("No Updates Found")
        subtitle: i18n("All your AppImages are up to date.")
        standardButtons: Kirigami.Dialog.Ok
        Kirigami.Theme.colorSet: Kirigami.Theme.Window
        Kirigami.Theme.inherit: false
    }

    Kirigami.PromptDialog {
        id: checkTimedOutDialog
        title: i18n("Update Check Failed")
        subtitle: i18n("The update check timed out. Check your network connection and try again.")
        standardButtons: Kirigami.Dialog.Ok
        Kirigami.Theme.colorSet: Kirigami.Theme.Window
        Kirigami.Theme.inherit: false
    }

    Kirigami.PromptDialog {
        id: networkErrorDialog
        title: i18n("Network Error")
        subtitle: i18n("Could not check for updates. Verify your network connection and try again.")
        standardButtons: Kirigami.Dialog.Ok
        Kirigami.Theme.colorSet: Kirigami.Theme.Window
        Kirigami.Theme.inherit: false
    }

    // ── Dialogs ───────────────────────────────────────────────────────────────
    FileDialog {
        id: installFileDialog
        title: i18n("Select AppImage to Install")
        nameFilters: [i18n("AppImage files (*.AppImage *.appimage)"), i18n("All files (*)")]
        onAccepted: dashboardController.installFromPath(selectedFile)
    }

    UninstallDialog { id: uninstallDialog }

    UpdateDialog {
        id: updateDialog
        onDownloadRequested: proxyModel.downloadUpdate(updateDialog.proxyRow)
    }

    OnboardingDialog { id: onboardingDialog; parentWindowWidth: root.width }

    Connections {
        target: listModel
        function onOpenUninstallWindow(filePath) {
            uninstallDialog.backend = dashboardController.createUninstallBackend(filePath)
            uninstallDialog.open()
        }
    }

    Component.onCompleted: {
        if (AppSettings.firstRun) {
            onboardingDialog.open()
        }
    }

    // ── Main UI Layout ────────────────────────────────────────────────────────
    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // 2. Top Header Bar
        Rectangle {
            id: topBar
            Layout.fillWidth: true
            Layout.preferredHeight: Kirigami.Units.gridUnit * 2.5
            color: Kirigami.Theme.alternateBackgroundColor
            
            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: root.cardBorderColor
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Kirigami.Units.smallSpacing
                anchors.rightMargin: Kirigami.Units.largeSpacing
                spacing: Kirigami.Units.largeSpacing

                Controls.Button {
                    id: sidebarToggleBtn
                    display: Controls.AbstractButton.IconOnly
                    icon.name: root.isSidebarCollapsed ? "sidebar-expand" : "sidebar-collapse"
                    text: root.isSidebarCollapsed ? i18n("Expand Sidebar") : i18n("Collapse Sidebar")
                    onClicked: root.isSidebarCollapsed = !root.isSidebarCollapsed
                    
                    Controls.ToolTip.visible: hovered
                    Controls.ToolTip.text: text
                    Controls.ToolTip.delay: Kirigami.Units.toolTipDelay
                    
                    Layout.alignment: Qt.AlignVCenter
                }

                Kirigami.Heading {
                    text: i18n("AppImage Manager")
                    level: 2
                    Layout.alignment: Qt.AlignVCenter
                }

                Item { Layout.fillWidth: true }

                Controls.Button {
                    icon.name: "document-import"
                    text: i18n("Install from File")
                    onClicked: installFileDialog.open()
                }
            }
        }

        // Main workspace: Left Master List + Right Content Stack
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            // 3. Navigation Master List (Left Pane)
            Rectangle {
                id: navPane
                Layout.fillHeight: true
                Layout.preferredWidth: root.isSidebarCollapsed
                                       ? Kirigami.Units.gridUnit * 2.5
                                       : Kirigami.Units.gridUnit * 10
                color: Kirigami.Theme.alternateBackgroundColor

                Behavior on Layout.preferredWidth {
                    NumberAnimation {
                        duration: Kirigami.Units.longDuration
                        easing.type: Easing.InOutQuad
                    }
                }

                Rectangle {
                    anchors.right: parent.right
                    height: parent.height
                    width: 1
                    color: root.cardBorderColor
                }

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0

                    ListView {
                        id: topNavListView
                        Layout.fillWidth: true
                        Layout.preferredHeight: contentHeight
                        Layout.topMargin: Kirigami.Units.largeSpacing
                        clip: true
                        interactive: false
                        model: ListModel {
                            ListElement { name: "Discover"; icon: "get-hot-new-stuff"; pageIndex: 0 }
                            ListElement { name: "Installed"; icon: "view-list-details"; pageIndex: 1 }
                        }
                        currentIndex: (root.activeNavIndex < 2) ? root.activeNavIndex : -1
                        delegate: navDelegateComponent
                    }

                    Item {
                        Layout.fillHeight: true
                    }

                    ListView {
                        id: bottomNavListView
                        Layout.fillWidth: true
                        Layout.preferredHeight: contentHeight
                        Layout.bottomMargin: Kirigami.Units.largeSpacing
                        clip: true
                        interactive: false
                        model: ListModel {
                            ListElement { name: "Settings"; icon: "configure"; pageIndex: 2 }
                            ListElement { name: "About"; icon: "help-about"; pageIndex: 3 }
                        }
                        currentIndex: (root.activeNavIndex >= 2) ? (root.activeNavIndex - 2) : -1
                        delegate: navDelegateComponent
                    }
                }

                Component {
                    id: navDelegateComponent
                    
                    Controls.ItemDelegate {
                        id: navDelegate
                        width: ListView.view.width
                        height: Kirigami.Units.gridUnit * 2
                        highlighted: ListView.isCurrentItem
                        
                        topInset: Kirigami.Units.largeSpacing / 2
                        bottomInset: Kirigami.Units.largeSpacing / 2
                        leftInset: Kirigami.Units.smallSpacing
                        rightInset: Kirigami.Units.smallSpacing
                        verticalPadding: Kirigami.Units.largeSpacing / 2

                        contentItem: RowLayout {
                            spacing: root.isSidebarCollapsed ? 0 : Kirigami.Units.largeSpacing

                            Item {
                                visible: root.isSidebarCollapsed
                                Layout.fillWidth: true
                            }

                            Kirigami.Icon {
                                source: model.icon
                                implicitWidth: Kirigami.Units.iconSizes.smallMedium
                                implicitHeight: Kirigami.Units.iconSizes.smallMedium
                                Layout.alignment: Qt.AlignVCenter
                                color: navDelegate.highlighted
                                       ? Kirigami.Theme.highlightedTextColor
                                       : Kirigami.Theme.textColor
                            }

                            Item {
                                visible: root.isSidebarCollapsed
                                Layout.fillWidth: true
                            }

                            Controls.Label {
                                text: model.name
                                color: navDelegate.highlighted
                                       ? Kirigami.Theme.highlightedTextColor
                                       : Kirigami.Theme.textColor
                                elide: Text.ElideRight
                                font.bold: navDelegate.highlighted
                                Layout.fillWidth: true
                                verticalAlignment: Text.AlignVCenter
                                visible: !root.isSidebarCollapsed
                                opacity: root.isSidebarCollapsed ? 0.0 : 1.0
                                
                                Behavior on opacity {
                                    NumberAnimation { duration: Kirigami.Units.shortDuration }
                                }
                            }
                        }

                        // Collapsed Tooltip support
                        Controls.ToolTip.visible: root.isSidebarCollapsed && hovered
                        Controls.ToolTip.text: model.name
                        Controls.ToolTip.delay: Kirigami.Units.toolTipDelay

                        onClicked: {
                            root.activeNavIndex = model.pageIndex
                        }
                    }
                }
            }

            // 4. Content Area (Right Pane StackLayout)
            StackLayout {
                id: contentStack
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: root.activeNavIndex

                StorePage {
                    id: storePage
                }

                LibraryPage {
                    id: libraryPage
                }

                SettingsPage {
                    id: settingsPage
                }

                AboutPage {
                    id: aboutPage
                }
            }
        }
    }

    // ── Global shortcuts ──────────────────────────────────────────────────
    Shortcut {
        sequence: "Ctrl+F"
        onActivated: {
            if (contentStack.currentIndex === 0 && storePage.searchField) {
                storePage.searchField.forceActiveFocus()
            } else if (contentStack.currentIndex === 1 && libraryPage.searchField) {
                libraryPage.searchField.forceActiveFocus()
            }
        }
    }
    Shortcut {
        sequences: [ StandardKey.Quit ]
        onActivated: Qt.quit()
    }
    Shortcut {
        sequences: [ StandardKey.Close ]
        onActivated: root.close()
    }

    // ── Global drag-and-drop install overlay ──────────────────────────────────
    DropArea {
        id: globalDropArea
        anchors.fill: parent
        keys: ["text/uri-list"]

        onDropped: (drop) => root.handleDrop(drop)

        readonly property bool noInnerBox: (proxyModel.rowCount() > 0 || proxyModel.filterText !== "") && !AppSettings.showInstallBox

        Rectangle {
            anchors.fill: parent
            color: Kirigami.Theme.highlightColor
            opacity: globalDropArea.containsDrag && parent.noInnerBox ? 0.15 : 0.0
            Behavior on opacity { NumberAnimation { duration: Kirigami.Units.shortDuration } }
        }

        Kirigami.Heading {
            anchors.centerIn: parent
            level: 2
            text: i18n("Drop to install")
            visible: globalDropArea.containsDrag && parent.noInnerBox
            color: Kirigami.Theme.highlightedTextColor
        }
    }
}
