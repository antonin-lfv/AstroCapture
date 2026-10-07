import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Dialogs

Rectangle {
    id: root
    required property AppLibrary library
    required property PreviewController captureController
    color: "#0b0d10"
    FolderDialog {
        id: exportFolderDialog
        title: qsTr("Choose the processing export folder")
        currentFolder: root.library.exportDirectoryUrl
        onAccepted: root.library.chooseExportDirectory(selectedFolder)
    }
    FolderDialog {
        id: captureFolderDialog
        title: qsTr("Choose the capture folder")
        currentFolder: root.captureController.saveDirectoryUrl
        onAccepted: root.captureController.chooseSaveDirectory(selectedFolder)
    }
    component Card: Rectangle {
        color: "#141a22"
        radius: 10
        border.color: "#26303c"
        Layout.fillWidth: true
        implicitHeight: content.implicitHeight + 32
        default property alias contents: content.data
        ColumnLayout { id: content; anchors.fill: parent; anchors.margins: 16; spacing: 12 }
    }
    component Heading: Label { color: "#eef3fa"; font.pixelSize: 17; Layout.fillWidth: true }
    component Description: Label { color: "#9aa7b8"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
    component FolderPath: Label {
        color: "#c5cfdd"; elide: Text.ElideMiddle; Layout.fillWidth: true
        HoverHandler { id: pathHover }
        ToolTip.visible: pathHover.hovered && truncated
        ToolTip.text: text
    }
    ScrollView {
        anchors.fill: parent
        anchors.margins: 24
        contentWidth: availableWidth
        clip: true
        ScrollBar.vertical.policy: ScrollBar.AlwaysOff
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        ColumnLayout {
            width: parent.width
            spacing: 16
            Label { text: qsTr("Application settings"); color: "#eef3fa"; font.pixelSize: 24 }
            Description { text: qsTr("Saved locally and kept when you update the application.") }
            Card {
                RowLayout {
                    Layout.fillWidth: true
                    Heading { text: qsTr("Language") }
                    IconButton { text: "English"; borderColor: root.library.language === "en" ? "#6ca5f8" : "#3a4048"; onClicked: root.library.language = "en" }
                    IconButton { text: "Français"; borderColor: root.library.language === "fr" ? "#6ca5f8" : "#3a4048"; onClicked: root.library.language = "fr" }
                }
                Description { text: qsTr("Applies immediately throughout the interface.") }
            }
            Card {
                Heading { text: qsTr("Processing layout") }
                RowLayout {
                    IconButton { text: qsTr("Guided"); borderColor: !root.library.compactProcessing ? "#6ca5f8" : "#3a4048"; onClicked: root.library.compactProcessing = false }
                    IconButton { text: qsTr("Compact"); borderColor: root.library.compactProcessing ? "#6ca5f8" : "#3a4048"; onClicked: root.library.compactProcessing = true }
                }
                Description { text: qsTr("Compact mode hides explanations. Help buttons remain available. Drag the divider in Processing to resize its controls; your layout is remembered.") }
            }
            Card {
                Heading { text: qsTr("Capture folder") }
                Description { text: qsTr("Camera images and recorded videos. Camera controls and recording formats remain in Capture.") }
                RowLayout {
                    Layout.fillWidth: true
                    FolderPath { text: root.captureController.saveDirectory || qsTr("Default: Pictures and Movies → AstroCapture") }
                    IconButton { text: qsTr("Choose folder…"); onClicked: captureFolderDialog.open() }
                }
                Description { text: qsTr("A custom folder contains two subfolders: Images and Videos.") }
                RowLayout {
                    IconButton { text: qsTr("Open folder"); onClicked: root.captureController.openSaveDirectory() }
                    IconButton { text: qsTr("Use default folders"); visible: root.captureController.saveDirectory.length > 0; onClicked: root.captureController.saveDirectory = "" }
                }
            }
            Card {
                Heading { text: qsTr("Processing export folder") }
                Description { text: qsTr("Processed TIFF images, after stacking and adjustments.") }
                RowLayout {
                    Layout.fillWidth: true
                    FolderPath { text: root.library.exportDirectory || qsTr("Source video's folder until you choose a location") }
                    IconButton { text: qsTr("Choose folder…"); onClicked: exportFolderDialog.open() }
                }
                Description { text: qsTr("Also remembers the folder used for your last successful export.") }
            }
            Card {
                RowLayout {
                    Layout.fillWidth: true
                    Heading { text: qsTr("Remember exported images") }
                    Switch {
                        id: historySwitch
                        checked: root.library.historyEnabled
                        onToggled: root.library.historyEnabled = checked
                        Accessible.name: qsTr("Remember exported images")
                        indicator: Rectangle {
                            implicitWidth: 44; implicitHeight: 24
                            x: historySwitch.leftPadding
                            y: (historySwitch.height - height) / 2
                            radius: 12
                            color: historySwitch.checked ? "#6ca5f8" : "#394351"
                            Rectangle {
                                width: 18; height: 18; radius: 9; y: 3
                                x: historySwitch.checked ? 23 : 3
                                color: "#eef3fa"
                                Behavior on x { NumberAnimation { duration: 120 } }
                            }
                        }
                        background: Rectangle { color: "transparent"; radius: 6; border.color: historySwitch.visualFocus ? "#eef3fa" : "transparent" }
                    }
                }
                Description { text: qsTr("Keeps up to 500 image paths locally. Images stay in their original folders. Turning this off keeps existing entries and still allows manual imports.") }
            }
            Description { text: root.library.status; visible: text.length > 0 }
        }
    }
}
