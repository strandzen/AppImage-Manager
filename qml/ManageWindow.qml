import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import org.kde.kirigami as Kirigami

ApplicationWindow {
    id: root
    width: 500
    height: 420
    minimumWidth: 500
    maximumWidth: 500
    minimumHeight: 420
    maximumHeight: 420
    visible: true
    title: qsTr("Manage AppImage")
    
    Kirigami.Theme.colorSet: Kirigami.Theme.Window
    Kirigami.Theme.inherit: false
    
    // Format bytes to human readable
    function formatBytes(bytes) {
        if (bytes === 0) return '0 Bytes';
        var k = 1024, dm = 2, sizes = ['Bytes', 'KB', 'MB', 'GB', 'TB', 'PB', 'EB', 'ZB', 'YB'];
        var i = Math.floor(Math.log(bytes) / Math.log(k));
        return parseFloat((bytes / Math.pow(k, i)).toFixed(dm)) + ' ' + sizes[i];
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 10

        // App Info Header
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2
            
            Label {
                text: backend.cleanName
                font.pixelSize: 22
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                Layout.fillWidth: true
                elide: Label.ElideRight

                ToolTip.visible: infoMouseArea.containsMouse
                ToolTip.text: backend.originalName
                
                MouseArea {
                    id: infoMouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                }
            }

            Label {
                text: qsTr("Version: %1  •  Size: %2").arg(backend.appVersion).arg(formatBytes(backend.appSize))
                color: Kirigami.Theme.textColor
                opacity: 0.7
                horizontalAlignment: Text.AlignHCenter
                Layout.fillWidth: true
            }
        }

        Item { Layout.fillHeight: true } // Top spacer

        // Center: Drag & Drop (macOS style)
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: innerRow.implicitHeight + 40
            implicitHeight: innerRow.implicitHeight + 60
            color: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.04)
            radius: Kirigami.Units.smallSpacing * 2
            border.color: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.1)
            border.width: 1

            RowLayout {
                id: innerRow
                anchors.centerIn: parent
                spacing: backend.isInstalled ? 0 : 80

            // Source: App Icon
            ColumnLayout {
                spacing: 8
                Item {
                    id: iconContainer
                    width: 96
                    height: 96
                    Layout.alignment: Qt.AlignHCenter

                    Image {
                        id: appIcon
                        width: 96
                        height: 96
                        source: backend.appIconSource != "" ? backend.appIconSource : "image://theme/application-x-executable"
                        fillMode: Image.PreserveAspectFit
                        
                        Drag.active: dragArea.drag.active
                        Drag.hotSpot.x: width / 2
                        Drag.hotSpot.y: height / 2
                        Drag.keys: ["appimage"]
                        
                        states: [
                            State {
                                when: appIcon.Drag.active
                                ParentChange {
                                    target: appIcon
                                    parent: root.contentItem
                                }
                            }
                        ]
                    }

                    MouseArea {
                        id: dragArea
                        anchors.fill: parent
                        drag.target: appIcon
                        enabled: !backend.isInstalled
                        
                        onReleased: {
                            // Sjekk om senter av ikonet er sluppet over mappen
                            var dropPos = appIcon.mapToItem(folderContainer, appIcon.width / 2, appIcon.height / 2)
                            if (dropPos.x >= 0 && dropPos.x <= folderContainer.width && dropPos.y >= 0 && dropPos.y <= folderContainer.height) {
                                backend.installAppImage()
                            }
                            
                            // Reset icon position
                            appIcon.parent = iconContainer
                            appIcon.x = 0
                            appIcon.y = 0
                        }
                    }
                }
                
                Label {
                    text: backend.cleanName.replace('.AppImage', '')
                    horizontalAlignment: Text.AlignHCenter
                    Layout.alignment: Qt.AlignHCenter
                    elide: Label.ElideRight
                    Layout.maximumWidth: 140
                }
            }

            // Target: Applications Folder
            ColumnLayout {
                spacing: 8
                visible: !backend.isInstalled

                Item {
                    id: folderContainer
                    width: 96
                    height: 96
                    Layout.alignment: Qt.AlignHCenter

                    DropArea {
                        id: folderDropArea
                        anchors.fill: parent
                        keys: ["appimage"]
                    }

                    Image {
                        source: "image://theme/system-file-manager"
                        anchors.fill: parent
                        opacity: folderDropArea.containsDrag ? 0.8 : 1.0
                        scale: folderDropArea.containsDrag ? 1.10 : 1.0
                        
                        Behavior on scale { NumberAnimation { duration: 150 } }
                        Behavior on opacity { NumberAnimation { duration: 150 } }
                    }
                }
                
                Label {
                    text: qsTr("Applications")
                    horizontalAlignment: Text.AlignHCenter
                    Layout.alignment: Qt.AlignHCenter
                    font.bold: dragArea.drag.active
                }
            }
            }
        }

        Item { Layout.fillHeight: true } // Bottom spacer

        // Footer: Checkbox & Uninstall
        ColumnLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignHCenter
            spacing: Kirigami.Units.largeSpacing
            
            CheckBox {
                text: qsTr("Create Shortcut")
                Layout.alignment: Qt.AlignHCenter
                checked: backend.hasDesktopLink
                onCheckedChanged: {
                    if (checked !== backend.hasDesktopLink) {
                        backend.toggleDesktopLink(checked)
                    }
                }
            }
            
            Button {
                text: qsTr("Remove")
                icon.name: "edit-delete"
                Layout.alignment: Qt.AlignHCenter
                onClicked: {
                    backend.findCorpses()
                    var component = Qt.createComponent("UninstallWindow.qml")
                    if (component.status === Component.Ready) {
                        var win = component.createObject(root)
                        win.show()
                    } else {
                        console.error(component.errorString())
                    }
                }
            }
        }
    }
    
    Connections {
        target: backend
        function onUninstallFinished() {
            root.close()
        }
    }
}
