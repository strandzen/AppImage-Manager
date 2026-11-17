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
    title: i18n("AppImage Manager")
    width: Kirigami.Units.gridUnit * 50
    height: Kirigami.Units.gridUnit * 30
    minimumWidth: Kirigami.Units.gridUnit * 40
    minimumHeight: Kirigami.Units.gridUnit * 22

    Kirigami.Theme.colorSet: Kirigami.Theme.Window
    Kirigami.Theme.inherit: false


    function handleDrop(drop) {
        for (const url of drop.urls) {
            if (url.toString().toLowerCase().endsWith(".appimage"))
                dashboardController.installFromPath(url)
        }
    }

    // ── Dialogs ───────────────────────────────────────────────────────────────
    FileDialog {
        id: installFileDialog
        title: i18n("Select AppImage to Install")
        nameFilters: [i18n("AppImage files (*.AppImage *.appimage)"), i18n("All files (*)")]
        onAccepted: dashboardController.installFromPath(selectedFile)
    }

    UninstallDialog { id: uninstallDialog }

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

    // ── Navigation ────────────────────────────────────────────────────────────
    // Two tabs (Installed, Discover) plus Settings and About, which open like pushed
    // pages: the tabs give way to a Back button and the page title.
    readonly property int pageInstalled: 0
    readonly property int pageDiscover: 1
    readonly property int pageSettings: 2
    readonly property int pageAbout: 3
    property int currentPage: pageInstalled
    property int lastTab: pageInstalled
    readonly property bool onSubPage: currentPage >= pageSettings

    function showTab(page) { currentPage = page; lastTab = page }

    // From DashboardWindow::open(): `appimagemanager --settings` / `--select <path>` (the tray uses
    // these) / `--welcome` (the welcome page again; it leaves Browse as it is after the first run).
    function openRequest(page, path) {
        if (page === "welcome") {
            if (!onboardingDialog.visible)
                onboardingDialog.open()
        } else if (page === "settings") {
            showSubPage(pageSettings)
        } else if (path) {
            showTab(pageInstalled)
            libraryPage.selectPath(path)
        }
    }
    function showSubPage(page) { currentPage = page }
    function goBack() { currentPage = lastTab }

    // ── Main UI Layout ────────────────────────────────────────────────────────
    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Controls.ToolBar {
            Layout.fillWidth: true

            contentItem: RowLayout {
                spacing: Kirigami.Units.smallSpacing

                Controls.ToolButton {
                    visible: root.onSubPage
                    icon.name: "go-previous"
                    text: i18n("Back")
                    display: Controls.AbstractButton.IconOnly
                    onClicked: root.goBack()
                    Controls.ToolTip.visible: hovered
                    Controls.ToolTip.text: text
                    Controls.ToolTip.delay: Kirigami.Units.toolTipDelay
                }

                Kirigami.Heading {
                    visible: root.onSubPage
                    level: 2
                    text: root.currentPage === root.pageSettings ? i18n("Settings") : i18n("About")
                }

                Controls.ToolButton {
                    visible: !root.onSubPage
                    icon.name: "view-list-icons"
                    text: listModel.updateCount > 0
                          ? i18n("Installed (%1)", listModel.updateCount)
                          : i18n("Installed")
                    checkable: true
                    checked: root.currentPage === root.pageInstalled
                    onClicked: root.showTab(root.pageInstalled)
                    Controls.ToolTip.visible: hovered && listModel.updateCount > 0
                    Controls.ToolTip.text: i18np("%1 update available", "%1 updates available", listModel.updateCount)
                    Controls.ToolTip.delay: Kirigami.Units.toolTipDelay
                }

                Controls.ToolButton {
                    visible: !root.onSubPage
                    icon.name: "globe"
                    text: i18nc("tab: browse online app catalogs", "Browse")
                    checkable: true
                    checked: root.currentPage === root.pageDiscover
                    onClicked: root.showTab(root.pageDiscover)
                }

                Item { Layout.fillWidth: true }

                Controls.ToolButton {
                    visible: root.currentPage === root.pageInstalled && listModel.updateCount > 1
                    icon.name: "system-software-update"
                    text: i18n("Update All")
                    onClicked: libraryPage.updateAll()
                }

                Controls.ToolButton {
                    visible: root.currentPage === root.pageInstalled
                    icon.name: "view-refresh"
                    text: i18n("Check for Updates")
                    display: Controls.AbstractButton.IconOnly
                    enabled: !listModel.checkingUpdates
                    onClicked: listModel.checkForUpdates()
                    Controls.ToolTip.visible: hovered
                    Controls.ToolTip.text: listModel.lastCheckTime !== ""
                        ? i18n("Check for Updates (last checked %1)", listModel.lastCheckTime) : text
                    Controls.ToolTip.delay: Kirigami.Units.toolTipDelay
                }

                Controls.ToolButton {
                    visible: !root.onSubPage
                    icon.name: "list-add"
                    text: i18n("Install from File…")
                    display: Controls.AbstractButton.IconOnly
                    onClicked: installFileDialog.open()
                    Controls.ToolTip.visible: hovered
                    Controls.ToolTip.text: text
                    Controls.ToolTip.delay: Kirigami.Units.toolTipDelay
                }

                Controls.ToolButton {
                    id: menuButton
                    icon.name: "application-menu"
                    text: i18n("Main Menu")
                    display: Controls.AbstractButton.IconOnly
                    down: mainMenu.visible
                    onPressed: mainMenu.visible ? mainMenu.close() : mainMenu.popup(menuButton, 0, menuButton.height)
                    Controls.ToolTip.visible: hovered && !mainMenu.visible
                    Controls.ToolTip.text: text
                    Controls.ToolTip.delay: Kirigami.Units.toolTipDelay

                    Controls.Menu {
                        id: mainMenu

                        Controls.MenuItem {
                            icon.name: "view-refresh"
                            text: listModel.checkingUpdates ? i18n("Checking for Updates…") : i18n("Check for Updates")
                            enabled: !listModel.checkingUpdates
                            onTriggered: {
                                root.showTab(root.pageInstalled)
                                listModel.checkForUpdates()
                            }
                        }
                        Controls.MenuSeparator {}
                        Controls.MenuItem {
                            icon.name: "configure"
                            text: i18n("Settings")
                            onTriggered: root.showSubPage(root.pageSettings)
                        }
                        Controls.MenuItem {
                            icon.name: "help-about"
                            text: i18n("About AppImage Manager")
                            onTriggered: root.showSubPage(root.pageAbout)
                        }
                    }
                }
            }
        }

        StackLayout {
            id: contentStack
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: root.currentPage

            LibraryPage {
                id: libraryPage
            }

            StorePage {
                id: storePage
            }

            SettingsPage {
                id: settingsPage
            }

            AboutPage {
                id: aboutPage
            }
        }
    }

    // "Checking for updates (3/13)", then the result; above everything, at the bottom.
    UpdateCheckToast {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Kirigami.Units.gridUnit * 2
        z: 100
    }

    // ── Global shortcuts ──────────────────────────────────────────────────
    Shortcut {
        sequence: "Ctrl+F"
        onActivated: {
            if (root.currentPage === root.pageDiscover && storePage.searchField) {
                storePage.searchField.forceActiveFocus()
            } else if (root.currentPage === root.pageInstalled && libraryPage.searchField) {
                libraryPage.searchField.forceActiveFocus()
            }
        }
    }
    Shortcut {
        sequences: [ StandardKey.Back ]
        enabled: root.onSubPage
        onActivated: root.goBack()
    }
    Shortcut {
        sequence: "Ctrl+,"
        onActivated: root.showSubPage(root.pageSettings)
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

        // The drop box under the Installed list highlights itself; everywhere else
        // (other pages, empty list, box turned off) the whole window shows the drop.
        readonly property bool noInnerBox: !libraryPage.dropBoxShown

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
        }
    }
}
