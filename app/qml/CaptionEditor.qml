import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Dialogs
Popup {
        id: editor
        required property var controller
        required property var library
        required property Component previewContent
        property bool exportStarted:false
        property bool compressedFormats: true
        readonly property bool dialogOpen:opened || saveCaption.visible
        signal previewRequested()
        property real hostWidth: 1100
        property real hostHeight: 740
        function setAnnotation(key, value) {
            let options = Object.assign({}, controller.exportAnnotation);
            options[key] = value; controller.exportAnnotation = options;
        }
        objectName: "annotationPopup"
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(1100, editor.hostWidth - 32)
        height: Math.min(740, editor.hostHeight - 32)
        padding: 18; modal: true; focus: true
        closePolicy:controller.busy?Popup.NoAutoClose:Popup.CloseOnEscape|Popup.CloseOnPressOutside
        background: Rectangle { color: "#151b24"; radius: 10; border.color: "#39465a" }
        ColumnLayout {
            anchors.fill: parent
            Label { text: qsTr("Classic export"); color: "#eef3fa"; font.pixelSize: 18 }
            GridLayout {
                columns:editor.width>=800?2:1
                Layout.fillWidth:true;Layout.fillHeight:true;columnSpacing:18;rowSpacing:12
                ImageViewport {
                    id:captionViewport;objectName:"captionPreview"
                    Layout.fillWidth:true;Layout.fillHeight:editor.width>=800
                    Layout.preferredWidth:600;Layout.minimumWidth:120
                    Layout.preferredHeight:editor.width>=800?400:180;Layout.minimumHeight:140
                    sourceWidth:editor.controller.imageWidth;sourceHeight:editor.controller.imageHeight
                    Loader {active:editor.visible;parent:captionViewport.imageContainer;anchors.fill:parent;sourceComponent:editor.previewContent}
                }
            ScrollView {
                id:captionSettings
                Layout.fillWidth: true; Layout.fillHeight: true;Layout.preferredWidth:360;Layout.minimumWidth:280
                contentWidth: availableWidth; clip: true
                ScrollBar.vertical.policy: ScrollBar.AlwaysOff
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                ColumnLayout {
                    width: captionSettings.availableWidth; spacing: 12
                    enabled:!editor.controller.busy
                    Label {text:editor.controller.stackSummary || "";visible:text.length>0;color:"#a8b5c6";wrapMode:Text.Wrap;Layout.fillWidth:true}
                    Caption { text: qsTr("Add text to a copy of the finished image. The original pixels stay unchanged. Enter your observation details; dates and planetary coordinates are not calculated. Each area accepts 400 characters. Check the preview for clipping.") }
                    RowLayout {
                        Caption { text: qsTr("Include on export") }
                        CaptionSwitch {
                            objectName: "annotationEnabled"
                            checked: editor.controller.exportAnnotation.enabled
                            Accessible.name: qsTr("Add a caption to the exported image")
                            onCheckedChanged: { editor.setAnnotation("enabled", checked); editor.previewRequested(); }
                        }
                    }
                    Label { text: qsTr("Title · above the caption"); color: "#c5cfdd" }
                    TextField {
                        objectName: "annotationTitle"
                        Layout.fillWidth: true
                        text: editor.controller.exportAnnotation.title
                        maximumLength: 120; selectByMouse: true
                        placeholderText: qsTr("Moon · Sea of Tranquillity")
                        color: "#eef3fa"; placeholderTextColor: "#78879a"
                        background: Rectangle { color: "#0f151e"; radius: 6; border.color: parent.activeFocus ? "#6ca5f8" : "#39465a" }
                        onTextChanged: editor.setAnnotation("title", text)
                    }
                    Label { text: qsTr("Top left · object, date, notes"); color: "#c5cfdd" }
                    TextArea {
                        objectName: "annotationTop"
                        Layout.fillWidth: true; Layout.preferredHeight: 95
                        text: editor.controller.exportAnnotation.top
                        placeholderText: qsTr("Object · observation date (UTC)\nFilter, conditions…")
                        wrapMode: TextEdit.Wrap; selectByMouse: true
                        color: "#eef3fa"; placeholderTextColor: "#78879a"
                        background: Rectangle { color: "#0f151e"; radius: 6; border.color: parent.activeFocus ? "#6ca5f8" : "#39465a" }
                        onTextChanged: editor.setAnnotation("top", text)
                    }
                    Label { text: qsTr("Bottom left · observer and equipment"); color: "#c5cfdd" }
                    TextArea {
                        objectName: "annotationBottom"
                        Layout.fillWidth: true; Layout.preferredHeight: 75
                        text: editor.controller.exportAnnotation.bottom
                        placeholderText: qsTr("Observer · location\nTelescope · camera · accessories")
                        wrapMode: TextEdit.Wrap; selectByMouse: true
                        color: "#eef3fa"; placeholderTextColor: "#78879a"
                        background: Rectangle { color: "#0f151e"; radius: 6; border.color: parent.activeFocus ? "#6ca5f8" : "#39465a" }
                        onTextChanged: editor.setAnnotation("bottom", text)
                    }
                    RowLayout {
                        Caption { text: qsTr("Add dimensions and processing summary") }
                        CaptionSwitch {
                            checked: editor.controller.exportAnnotation.technical
                            Accessible.name: qsTr("Add dimensions and processing summary")
                            onCheckedChanged: editor.setAnnotation("technical", checked)
                        }
                    }
                    RowLayout {
                        Caption { text: qsTr("Text color") }
                        IconButton { text: qsTr("White"); borderColor: !editor.controller.exportAnnotation.gold ? "#6ca5f8" : "#39465a"; onClicked: editor.setAnnotation("gold", false) }
                        IconButton { text: qsTr("Gold"); borderColor: editor.controller.exportAnnotation.gold ? "#6ca5f8" : "#39465a"; onClicked: editor.setAnnotation("gold", true) }
                    }
                    RowLayout {
                        Caption { text: qsTr("Text size") }
                        DarkSlider { Layout.fillWidth: true; from: 0.8; to: 3; stepSize: 0.1; value: editor.controller.exportAnnotation.size; onMoved: editor.setAnnotation("size", value) }
                    }
                    Caption { text: qsTr("The caption appears in the result preview. It is cleared when you load a new source. Export with the switch off to keep an image without text.") }
                }
            }
            }
            ExportSizeHint {controller:editor.controller;format:exportFormat.suffix;estimating:editor.visible;ready:!editor.controller.busy;Layout.fillWidth:true}
            RowLayout {Layout.fillWidth:true
                Label {text:editor.exportStarted?editor.controller.status:qsTr("Caption preview · original image unchanged");color:"#a8b5c6";wrapMode:Text.Wrap;Layout.fillWidth:true;Layout.maximumHeight:50}
                IconButton {text:qsTr("Cancel export");visible:editor.controller.busy;onClicked:editor.controller.cancel()}
                DarkComboBox {id:exportFormat;objectName:"captionExportFormat";Layout.fillWidth:true;model:editor.compressedFormats?[qsTr("PNG · 16-bit, lossless"),qsTr("TIFF · 16-bit, lossless"),qsTr("JPEG · compressed")]:[qsTr("TIFF · 16-bit, lossless")];property string suffix:editor.compressedFormats?["png","tif","jpg"][currentIndex]:"tif"}
                IconButton {objectName:"captionExport";text:qsTr("Export");enabled:!editor.controller.busy && editor.controller.hasResult;onClicked:{saveCaption.currentFolder=editor.library.exportFolderFor(editor.controller.exportUrl);saveCaption.selectedFile=editor.library.suggestedExport(editor.controller.exportUrl).toString().replace(/\.[^/.]+$/, "."+exportFormat.suffix);saveCaption.open()}}
                IconButton {text:qsTr("Done");enabled:!editor.controller.busy;onClicked:editor.close()}
            }
        }
        FileDialog {id:saveCaption;title:qsTr("Export image");fileMode:FileDialog.SaveFile;nameFilters:exportFormat.suffix==="png"?[qsTr("16-bit PNG image (*.png)")]:exportFormat.suffix==="tif"?[qsTr("16-bit TIFF image (*.tif *.tiff)")]:[qsTr("JPEG image (*.jpg *.jpeg)")];defaultSuffix:exportFormat.suffix;onAccepted:{editor.exportStarted=true;let path=selectedFile.toString();if(!/\.(png|tiff?|jpe?g)$/i.test(path))path+="."+exportFormat.suffix;editor.controller.exportImage(path)}}
        onOpened:{exportStarted=false;if(controller.showResult)controller.showResult(false)}
        component CaptionSwitch: Switch {
        id: control
        indicator: Rectangle {
            implicitWidth: 38; implicitHeight: 22
            x: control.leftPadding; y: (control.height - height) / 2
            radius: 11; color: control.checked ? "#6ca5f8" : "#394351"
            Rectangle { width: 16; height: 16; radius: 8; y: 3; x: control.checked ? 19 : 3; color: "#eef3fa" }
        }
        background: Rectangle { color: "transparent"; radius: 6; border.color: control.visualFocus ? "#eef3fa" : "transparent" }
    }
    component DarkSlider: Slider {
        id: control
        implicitHeight: 28
        background: Rectangle {
            x: control.leftPadding
            y: control.topPadding + control.availableHeight / 2 - height / 2
            width: control.availableWidth
            height: 4
            radius: 2
            color: "#2e3948"
            Rectangle { width: control.visualPosition * parent.width; height: parent.height; radius: 2; color: control.enabled ? "#6ca5f8" : "#445164" }
        }
        handle: Rectangle {
            x: control.leftPadding + control.visualPosition * (control.availableWidth - width)
            y: control.topPadding + control.availableHeight / 2 - height / 2
            implicitWidth: 14; implicitHeight: 14
            radius: 7
            color: control.enabled ? "#bdd7fa" : "#566170"
        }
    }
    component Caption: Label { color: "#9aa7b8"; wrapMode: Text.WordWrap; Layout.fillWidth: true; font.pixelSize: 12 }
}
