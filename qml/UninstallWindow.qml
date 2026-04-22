import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import org.kde.kirigami as Kirigami

ApplicationWindow {
    id: uninstallWindow
    width: 500
    height: 400
    title: qsTr("Uninstall %1").arg(backend.cleanName)
    flags: Qt.Window | Qt.WindowStaysOnTopHint
    modality: Qt.ApplicationModal
    
    Kirigami.Theme.colorSet: Kirigami.Theme.Window
    Kirigami.Theme.inherit: false

    function formatBytes(bytes) {
        if (bytes === 0) return '0 Bytes';
        var k = 1024, dm = 2, sizes = ['Bytes', 'KB', 'MB', 'GB', 'TB', 'PB', 'EB', 'ZB', 'YB'];
        var i = Math.floor(Math.log(bytes) / Math.log(k));
        return parseFloat((bytes / Math.pow(k, i)).toFixed(dm)) + ' ' + sizes[i];
    }

    ListModel {
        id: itemsModel
    }

    Component.onCompleted: {
        // Add the AppImage itself (checked by default)
        itemsModel.append({
            "filePath": backend.originalName + " (AppImage)",
            "actualPath": "APPIMAGE_PLACEHOLDER",
            "fileSize": backend.appSize,
            "isChecked": true
        })

        if (!backend.isFindingCorpses) {
            populateCorpses()
        }
        updateTotalSize()
    }

    Connections {
        target: backend
        function onCorpsesChanged() {
            populateCorpses()
            updateTotalSize()
        }
    }

    function populateCorpses() {
        var corpses = backend.corpses
        for (var i = 0; i < corpses.length; i++) {
            itemsModel.append({
                "filePath": corpses[i].path,
                "actualPath": corpses[i].path,
                "fileSize": corpses[i].size,
                "isChecked": false
            })
        }
    }

    function updateTotalSize() {
        var total = 0
        for (var i = 0; i < itemsModel.count; i++) {
            if (itemsModel.get(i).isChecked) {
                total += itemsModel.get(i).fileSize
            }
        }
        totalSizeLabel.text = qsTr("Selected for removal: %1").arg(formatBytes(total))
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 15
        spacing: 10

        BusyIndicator {
            anchors.centerIn: parent
            running: backend.isFindingCorpses || backend.isRemovingItems
            visible: running
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: !backend.isFindingCorpses && !backend.isRemovingItems

        Label {
            text: qsTr("Select items to remove:")
            font.bold: true
            color: Kirigami.Theme.textColor
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            ListView {
                id: listView
                model: itemsModel
                spacing: 5
                
                delegate: RowLayout {
                    width: listView.width
                    
                    CheckBox {
                        checked: model.isChecked
                        onCheckedChanged: {
                            itemsModel.setProperty(index, "isChecked", checked)
                            updateTotalSize()
                        }
                    }
                    
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        
                        Label {
                            text: model.filePath
                            Layout.fillWidth: true
                            elide: Label.ElideMiddle
                            font.pixelSize: 12
                            color: Kirigami.Theme.textColor
                        }
                        Label {
                            text: formatBytes(model.fileSize)
                            font.pixelSize: 10
                            opacity: 0.7
                            color: Kirigami.Theme.textColor
                        }
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true

            Label {
                id: totalSizeLabel
                Layout.fillWidth: true
                font.bold: true
                color: Kirigami.Theme.textColor
            }

            Button {
                text: qsTr("Cancel")
                onClicked: uninstallWindow.close()
            }

            Button {
                text: qsTr("Remove")
                icon.name: "edit-delete"
                palette.button: Kirigami.Theme.negativeTextColor
                palette.buttonText: Kirigami.Theme.backgroundColor
                onClicked: {
                    var pathsToDelete = []
                    for (var i = 0; i < itemsModel.count; i++) {
                        var item = itemsModel.get(i)
                        if (item.isChecked && item.actualPath !== "APPIMAGE_PLACEHOLDER") {
                            pathsToDelete.push(item.actualPath)
                        }
                    }
                    // We only tell backend about the corpse paths. 
                    // Backend automatically removes the AppImage if we call removeAppImageAndCorpses.
                    // Wait, what if the user UNCHECKS the AppImage? 
                    // Let's modify the backend call to pass a boolean for appImage too.
                    // Actually, if we just pass pathsToDelete, the backend can delete them.
                    // But backend.remove_items explicitly deletes the AppImage.
                    // Let's fix that. We should pass the AppImage path in the list if it's checked.
                    if (itemsModel.get(0).isChecked) {
                        // AppImage is checked
                        pathsToDelete.push("APPIMAGE_ITSELF")
                    }
                    
                    backend.removeAppImageAndCorpses(pathsToDelete)
                    uninstallWindow.close()
                }
            }
        }
    }
    }
}
