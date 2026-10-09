import "Atelier.js" as Atelier
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Dialogs

Rectangle {
    id: root
    required property AppLibrary library
    property var session: null
    onVisibleChanged:if(visible&&session)session.refreshCache()
    function sizeText(bytes) {return bytes===undefined?qsTr("Not measured"):bytes>=1000000000?qsTr("%1 GB").arg(Number(bytes/1000000000).toLocaleString(Qt.locale(),"f",1)):qsTr("%1 MB").arg(Number((bytes||0)/1000000).toLocaleString(Qt.locale(),"f",1));}
    required property PreviewController captureController
    color: Atelier.canvas
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
        color: Atelier.surface
        radius: 4
        border.color: Atelier.border
        Layout.fillWidth: true
        implicitHeight: content.implicitHeight + 32
        default property alias contents: content.data
        ColumnLayout { id: content; anchors.fill: parent; anchors.margins: 16; spacing: 12 }
    }
    component Heading: SettingLabel { color: Atelier.text; font.pixelSize: 16; Layout.fillWidth: true }
    component Description: Label { color: Atelier.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true }
    component FolderPath: Label {
        color: Atelier.secondary; elide: Text.ElideMiddle; Layout.fillWidth: true
        HoverHandler { id: pathHover }
        ToolTip.visible: pathHover.hovered && truncated
        ToolTip.text: text
    }
    ScrollView {
        id:settingsScroll;objectName:"globalSettingsScroll"
        anchors.top:parent.top;anchors.bottom:parent.bottom;anchors.horizontalCenter:parent.horizontalCenter
        anchors.topMargin:20;anchors.bottomMargin:20;width:Math.min(1040,parent.width-40)
        contentWidth: availableWidth
        clip: true
        ScrollBar.vertical.policy: ScrollBar.AsNeeded
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        ColumnLayout {
            width: settingsScroll.availableWidth
            spacing: 16
            Label { text: qsTr("Application settings"); color: Atelier.text; font.pixelSize: 20; font.weight:Font.Medium }
            Description { text: qsTr("Saved locally and kept when you update the application.") }
            Card {
                RowLayout {
                    Layout.fillWidth: true
                    Heading {explanation: qsTr("Changes interface labels and help immediately. Saved locally; processing parameters and image pixels are unaffected.");  text: qsTr("Language") }
                    IconButton { text: "English"; borderColor: root.library.language === "en" ? Atelier.accent : Atelier.border; onClicked: root.library.language = "en" }
                    IconButton { text: "Français"; borderColor: root.library.language === "fr" ? Atelier.accent : Atelier.border; onClicked: root.library.language = "fr" }
                }
                Description { text: qsTr("Applies immediately throughout the interface.") }
            }
            Card {
                Heading {explanation: qsTr("Guided shows explanations between controls; Compact hides them to save space. Question-mark help remains available in both modes. This changes the interface, not processing.");  text: qsTr("Processing layout") }
                RowLayout {
                    IconButton { text: qsTr("Guided"); borderColor: !root.library.compactProcessing ? Atelier.accent : Atelier.border; onClicked: root.library.compactProcessing = false }
                    IconButton { text: qsTr("Compact"); borderColor: root.library.compactProcessing ? Atelier.accent : Atelier.border; onClicked: root.library.compactProcessing = true }
                }
                Description { text: qsTr("Compact mode hides explanations. Help buttons remain available. Drag the divider in Processing to resize its controls; your layout is remembered.") }
            }
            Card {
                Heading {explanation: qsTr("Destination for new camera images and recordings. A custom folder contains Images and Videos. Changing it does not move existing files.");  text: qsTr("Capture folder") }
                Description { text: qsTr("Camera images and recorded videos. Camera controls and recording formats remain in Capture.") }
                RowLayout {
                    Layout.fillWidth: true
                    FolderPath { text: root.captureController.saveDirectory || qsTr("Default: Pictures and Movies → AstroCapture") }
                    IconButton { text: qsTr("Choose folder"); onClicked: captureFolderDialog.open() }
                }
                Description { text: qsTr("A custom folder contains two subfolders: Images and Videos.") }
                RowLayout {
                    IconButton { text: qsTr("Open folder"); onClicked: root.captureController.openSaveDirectory() }
                    IconButton { text: qsTr("Use default folders"); visible: root.captureController.saveDirectory.length > 0; onClicked: root.captureController.saveDirectory = "" }
                }
            }
            Card {
                Heading {explanation: qsTr("Default destination for finished images. The last successful export folder is remembered. Originals and already exported files are not moved.");  text: qsTr("Export folder") }
                Description { text: qsTr("Finished PNG and TIFF images, with your current adjustments.") }
                RowLayout {
                    Layout.fillWidth: true
                    FolderPath { text: root.library.exportDirectory || qsTr("Source video's folder until you choose a location") }
                    IconButton { text: qsTr("Choose folder"); onClicked: exportFolderDialog.open() }
                }
                Description { text: qsTr("Also remembers the folder used for your last successful export.") }
            }
            Card {
                RowLayout {
                    Layout.fillWidth: true
                    Heading {explanation: qsTr("Adds successful exports to local History, up to 500 paths. Turning it off stops new entries but preserves existing ones and manual imports; image files stay where you saved them.");  text: qsTr("Remember exported images") }
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
                            color: historySwitch.checked ? Atelier.accent : Atelier.border
                            Rectangle {
                                width: 18; height: 18; radius: 9; y: 3
                                x: historySwitch.checked ? 23 : 3
                                color: Atelier.text
                                Behavior on x { NumberAnimation { duration: 120 } }
                            }
                        }
                        background: Rectangle { color: "transparent"; radius: 6; border.color: historySwitch.visualFocus ? Atelier.text : "transparent" }
                    }
                }
                Description { text: qsTr("Keeps up to 500 image paths locally. Images stay in their original folders. Turning this off keeps existing entries and still allows manual imports.") }
            }
            Card {
                id:cacheCard;objectName:"projectCacheCard";visible:root.session!==null
                Heading {text:qsTr("Storage and recovery");explanation:qsTr("Portable projects are opened in a local cache. Cleanup removes only unused project caches; it preserves every cache referenced by open work or the saved recovery session. The portable project files and original images are unchanged.")}
                Description {text:root.session?qsTr("Project caches: %1 · unused: %2").arg(root.sizeText(root.session.cacheInfo.bytes)).arg(root.sizeText(root.session.cacheInfo.unusedBytes)):""}
                Description {text:root.session?qsTr("Automatic recovery: %1").arg(root.sizeText(root.session.cacheInfo.sessionBytes)):""}
                Description {text:qsTr("Open projects and automatic recovery are kept. After closing a project, refresh to see the space that can be freed.")}
                RowLayout {
                    IconButton {text:qsTr("Refresh");enabled:root.session&&!root.session.busy;onClicked:root.session.refreshCache()}
                    IconButton {objectName:"clearUnusedProjectCache";text:qsTr("Free unused space");enabled:root.session&&!root.session.busy&&root.session.cacheInfo.unusedBytes>0;onClicked:root.session.clearUnusedProjectCache()}
                }
                Description {text:root.session?root.session.status:"";visible:text.length>0}
            }
            Card {
                Heading { text: "AstroCapture · " + Qt.application.version }
                Description { text: qsTr("Report a problem by email. You can review the message before sending it.") }
                IconButton { text: qsTr("Report a bug"); onClicked: root.library.reportBug() }
            }
            Description { text: root.library.status; visible: text.length > 0 }
        }
    }
}
