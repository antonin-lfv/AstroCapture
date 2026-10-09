import "Atelier.js" as Atelier
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Dialogs

Rectangle {
    id: root
    required property MosaicController controller
    required property AppLibrary library
    readonly property bool dialogOpen: openDialog.visible || saveDialog.visible || annotationPopup.dialogOpen || optionsPopup.opened || excludedPopup.opened
    signal editResult()
    property bool canEditResult: true
    property bool balanceExposure: true
    property bool skipUnmatched: true
    property bool softenEdges: true
    readonly property var sessionState: ({balanceExposure:balanceExposure, skipUnmatched:skipUnmatched, softenEdges:softenEdges,fit:viewport.fitMode,zoom:viewport.zoom,panX:viewport.panX,panY:viewport.panY})
    function restoreSessionState(state) {
        balanceExposure=state.balanceExposure!==false;skipUnmatched=state.skipUnmatched!==false;softenEdges=state.softenEdges!==false;viewport.fitMode=state.fit!==false;viewport.zoom=state.zoom||1;viewport.panX=state.panX||0;viewport.panY=state.panY||0;viewport.constrainPan();
    }
    color: Atelier.canvas
    FileDialog {
        id: openDialog
        title: qsTr("Add overlapping Moon or Sun images")
        fileMode: FileDialog.OpenFiles
        nameFilters: [qsTr("Images (*.tif *.tiff *.png *.jpg *.jpeg *.TIF *.TIFF *.PNG *.JPG *.JPEG)")]
        onAccepted: root.controller.addImages(selectedFiles)
    }
    FileDialog {
        id: saveDialog
        title: qsTr("Export mosaic")
        fileMode: FileDialog.SaveFile
        nameFilters: [qsTr("16-bit TIFF image (*.tif *.tiff)")]
        defaultSuffix: "tif"
        onAccepted: root.controller.exportImage(selectedFile)
    }
    CaptionEditor {
        id: annotationPopup
        compressedFormats: false
        controller: root.controller
        library:root.library
        previewContent:Component {MosaicImageItem {controller:root.controller}}
        hostWidth: root.width; hostHeight: root.height
    }
    Popup {
        id: optionsPopup
        parent: Overlay.overlay; anchors.centerIn: parent; width: Math.min(480, root.width - 32)
        padding: 20; modal: true; focus: true
        background: Rectangle { color: Atelier.surface; radius: 10; border.color: Atelier.border }
        contentItem: ColumnLayout {
            spacing: 14
            Label { text: qsTr("Assembly options"); color: Atelier.text; font.pixelSize: 18 }
            OptionSwitch { text: qsTr("Match brightness"); checked: root.balanceExposure; onCheckedChanged: root.balanceExposure = checked }
            OptionSwitch { text: qsTr("Automatically exclude unmatched images"); checked: root.skipUnmatched; onCheckedChanged: root.skipUnmatched = checked }
            Label { text: qsTr("Keeps the largest connected group. Excluded images can be restored; files are never deleted. At least two matching views are needed."); color: Atelier.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            OptionSwitch { objectName: "mosaicSoftenEdges"; text: qsTr("Soften mosaic edges"); checked: root.softenEdges; onCheckedChanged: root.softenEdges = checked }
            Label { text: qsTr("Fades the outer coverage boundary into black and protects it from sharpening in Processing. A narrow strip at the edge is darkened; missing areas are not reconstructed."); color: Atelier.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            Label { text: qsTr("Changes apply to the next assembly."); color: Atelier.muted; Layout.fillWidth: true }
            IconButton { text: qsTr("Done"); Layout.alignment: Qt.AlignRight; onClicked: optionsPopup.close() }
        }
    }
    Popup {
        id: excludedPopup
        parent: Overlay.overlay; anchors.centerIn: parent; width: Math.min(500, root.width - 32); height: Math.min(460, root.height - 32)
        padding: 20; modal: true; focus: true
        background: Rectangle { color: Atelier.surface; radius: 10; border.color: Atelier.border }
        contentItem: ColumnLayout {
            Label { text: qsTr("Excluded images"); color: Atelier.text; font.pixelSize: 18 }
            Label { text: qsTr("These views could not be connected reliably to the retained group. They remain in the grid, unchecked. Original files are unchanged."); color: Atelier.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            ListView {
                Layout.fillWidth: true; Layout.fillHeight: true; clip: true
                model: root.controller.excludedFiles
                delegate: Label { required property string modelData; width: ListView.view.width; text: modelData; color: "#dbe5f2"; wrapMode: Text.WrapAnywhere; padding: 6 }
            }
            RowLayout {
                IconButton { text: qsTr("Restore to selection"); enabled: !root.controller.busy; onClicked: { root.controller.restoreExcluded(); excludedPopup.close(); } }
                Item { Layout.fillWidth: true }
                IconButton { text: qsTr("Close"); onClicked: excludedPopup.close() }
            }
        }
    }
    component OptionSwitch: Switch {
        id: option
        Layout.fillWidth: true
        Accessible.name: text
        contentItem: Label { text: option.text; color: "#dbe5f2"; wrapMode: Text.WordWrap; rightPadding: 50; verticalAlignment: Text.AlignVCenter }
        indicator: Rectangle {
            implicitWidth: 38; implicitHeight: 22; x: option.width-width; y: (option.height-height)/2; radius: 11
            color: option.checked ? Atelier.accent : Atelier.border
            Rectangle { width: 16; height: 16; radius: 8; y: 3; x: option.checked ? 19 : 3; color: Atelier.text }
        }
        background: Rectangle { color: "transparent"; border.color: option.visualFocus ? Atelier.text : "transparent"; radius: 6 }
    }
    Connections { target: root.controller; function onImageChanged() { viewport.fit(); } }
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 20; spacing: 14
        RowLayout {
            Layout.fillWidth: true
            Label { text: qsTr("Mosaic"); color: Atelier.text; font.pixelSize: 22; font.weight:Font.Medium; Layout.fillWidth: true }
            IconButton { text: qsTr("Add images"); enabled: !root.controller.busy; onClicked: openDialog.open() }
            IconButton { text: qsTr("Clear"); enabled: !root.controller.busy && root.controller.files.length>0; onClicked: root.controller.clear() }
        }
        Label {
            text: qsTr("Reassemble the Moon or Sun from overlapping views. Use images from the same session and setup, preferably already stacked, with 25–40% overlap and no captions.")
            color: Atelier.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true
        }
        RowLayout {
            Layout.fillWidth: true; Layout.fillHeight: true; spacing: 16
            ColumnLayout {
                visible:root.controller.files.length>0
                Layout.preferredWidth: Math.min(340, root.width * .38)
                Layout.maximumWidth: Math.min(340, root.width * .38)
                Layout.minimumWidth: 220; Layout.fillHeight: true
                Label { text: qsTr("%1 / %2 images selected").arg(root.controller.selectedCount).arg(root.controller.files.length); color: Atelier.secondary }
                GridView {
                    id: files
                    objectName: "mosaicFiles"
                    Layout.fillWidth: true; Layout.fillHeight: true
                    clip: true; cellWidth: width / 2; cellHeight: 184
                    model: root.controller.files
                    cacheBuffer: 0; reuseItems: true
                    boundsBehavior: Flickable.StopAtBounds
                    delegate: Rectangle {
                        id: tile
                        required property var modelData
                        required property int index
                        readonly property bool included: !!root.controller.selection[index]
                        width: files.cellWidth - 8; height: files.cellHeight - 8
                        radius: 8; color: "#141c26"; border.color: tile.included ? "#466584" : "#2c394b"
                        ColumnLayout {
                            anchors.fill: parent; anchors.margins: 8; spacing: 5
                            Loader {
                                objectName: "mosaicThumbnailLoader"
                                Layout.fillWidth: true; Layout.preferredHeight: 96
                                active: root.visible && tile.y + tile.height > files.contentY && tile.y < files.contentY + files.height
                                sourceComponent: Image {
                                    source: tile.modelData.url
                                    opacity: tile.included ? 1 : 0.4
                                    asynchronous: true; cache: false
                                    sourceSize.width: 256; sourceSize.height: 192
                                    fillMode: Image.PreserveAspectFit
                                    Label { anchors.centerIn: parent; visible: parent.status === Image.Error; text: qsTr("Preview unavailable"); color: Atelier.muted; font.pixelSize: 10 }
                                }
                            }
                            Label { text: modelData.name; color: "#dbe5f2"; elide: Text.ElideMiddle; Layout.fillWidth: true; font.pixelSize: 11 }
                            Label { text: modelData.width + " × " + modelData.height; color: "#92a2b6"; font.pixelSize: 10 }
                            CheckBox {
                                id: selection
                                objectName: "mosaicTileSelection"
                                text: qsTr("Use this image"); checked: tile.included
                                Layout.fillWidth: true; enabled: !root.controller.busy
                                Accessible.name: qsTr("Use %1").arg(tile.modelData.name)
                                property bool initialized: false
                                Component.onCompleted: initialized = true
                                onCheckedChanged: if (initialized) root.controller.setImageSelected(tile.index, checked)
                                indicator: Rectangle {
                                    implicitWidth: 18; implicitHeight: 18; x: selection.leftPadding; y: (selection.height-height)/2
                                    radius: 4; color: selection.checked ? Atelier.accent : Atelier.surface; border.color: selection.visualFocus ? Atelier.text : "#526579"
                                    Label { anchors.centerIn: parent; text: selection.checked ? "✓" : ""; color: "#0d1726"; font.pixelSize: 14 }
                                }
                                contentItem: Label { text: selection.text; leftPadding: 25; verticalAlignment: Text.AlignVCenter; color: Atelier.secondary; font.pixelSize: 11 }
                            }
                        }
                    }
                }
                IconButton {
                    text: qsTr("Assembly options"); Layout.fillWidth: true; enabled: !root.controller.busy
                    onClicked: optionsPopup.open()
                }
                IconButton {
                    text: qsTr("%1 excluded images").arg(root.controller.excludedFiles.length)
                    Layout.fillWidth: true; visible: root.controller.excludedFiles.length > 0
                    onClicked: excludedPopup.open()
                }
                IconButton {
                    text: root.controller.busy ? qsTr("Working…") : qsTr("Assemble automatically")
                    Layout.fillWidth: true
                    enabled: !root.controller.busy && root.controller.selectedCount>=2
                    onClicked: root.controller.assemble(root.balanceExposure, root.skipUnmatched, root.softenEdges)
                }
                RowLayout {
                    Layout.fillWidth: true; enabled: root.controller.hasResult && !root.controller.busy
                    Label { text: qsTr("Image caption"); color: Atelier.secondary; Layout.fillWidth: true }
                    IconButton { text: qsTr("Edit caption");enabled:root.controller.hasResult && !root.controller.busy; onClicked: annotationPopup.open() }
                }
                IconButton {
                    text: qsTr("Edit in Processing"); Layout.fillWidth: true
                    enabled: root.controller.hasResult && !root.controller.busy && root.canEditResult
                    onClicked: root.editResult()
                }
                ExportSizeHint {controller:root.controller;format:"tif";ready:!root.controller.busy;visible:root.controller.hasResult;Layout.fillWidth:true}
                IconButton {
                    text: qsTr("Export TIFF"); Layout.fillWidth: true
                    enabled: root.controller.hasResult && !root.controller.busy
                    onClicked: {
                        saveDialog.currentFolder=root.library.exportFolderFor(root.controller.exportUrl);
                        saveDialog.selectedFile=root.library.suggestedExport(root.controller.exportUrl);
                        saveDialog.open();
                    }
                }
            }
            ImageViewport {
                id: viewport
                objectName: "mosaicViewport"
                Layout.minimumWidth: 200
                Layout.fillWidth: true; Layout.fillHeight: true
                sourceWidth: root.controller.imageWidth; sourceHeight: root.controller.imageHeight
                MosaicImageItem { parent: viewport.imageContainer; anchors.fill: parent; controller: root.controller }
                ColumnLayout {
                    anchors.centerIn:parent;width:Math.min(420,parent.width-50);spacing:12
                    visible:!root.controller.hasResult
                    Icon {name:"mosaic";color:Atelier.accent;Layout.preferredWidth:32;Layout.preferredHeight:32;Layout.alignment:Qt.AlignHCenter}
                    Label {text:root.controller.busy?qsTr("Building your mosaic…"):qsTr("Build your mosaic");font.pixelSize:16;color:Atelier.text;wrapMode:Text.Wrap;horizontalAlignment:Text.AlignHCenter;Layout.fillWidth:true}
                    Label {text:qsTr("Add overlapping Moon or Sun images, then assemble them.");color:Atelier.muted;wrapMode:Text.Wrap;horizontalAlignment:Text.AlignHCenter;Layout.fillWidth:true}
                    IconButton {text:qsTr("Add images");iconName:"folder";enabled:!root.controller.busy;visible:root.controller.files.length===0;Layout.alignment:Qt.AlignHCenter;onClicked:openDialog.open()}
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Layout.minimumHeight: 48; Layout.preferredHeight: 48; Layout.maximumHeight: 48
            Label {
                text: root.controller.status || qsTr("TIFF, PNG or JPEG · up to 100 images · 32 MP output · originals stay unchanged")
                color: "#b8c7da"; wrapMode: Text.WordWrap; maximumLineCount: 3; elide: Text.ElideRight; Layout.fillWidth: true; font.pixelSize: 12
                HoverHandler { id: statusHover }
                ToolTip.visible: statusHover.hovered && truncated
                ToolTip.text: text
            }
            ProgressBar { from: 0; to: 100; value: root.controller.progress; opacity: root.controller.busy ? 1 : 0; Layout.preferredWidth: 100 }
            IconButton { text: qsTr("Cancel"); opacity: root.controller.busy ? 1 : 0; enabled: root.controller.busy; onClicked: root.controller.cancel() }
        }
    }
    DropArea {
        anchors.fill: parent
        enabled: !root.controller.busy && !root.dialogOpen
        function accepts(event) {
            return event.hasUrls && event.urls.some(url => /^file:/i.test(url.toString()) && /\.(tif|tiff|png|jpe?g)$/i.test(url.toString()));
        }
        onEntered: drag => { drag.accepted=accepts(drag); }
        onDropped: drop => { if(!accepts(drop))return; root.controller.addImages(drop.urls); drop.acceptProposedAction(); }
        Rectangle {
            anchors.fill: parent; anchors.margins: 8; radius: 10
            visible: parent.containsDrag; color: "#e6111c2c"; border.color: Atelier.accent; border.width: 2
            Label { anchors.centerIn: parent; text: qsTr("Drop to add mosaic images"); color: Atelier.text; font.pixelSize: 22 }
        }
    }
}
