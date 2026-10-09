import "Atelier.js" as Atelier
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
        property bool previewOpen:false
        property bool compressedFormats: true
        readonly property bool dialogOpen:opened || saveCaption.visible
        signal previewRequested()
        property real hostWidth: 1100
        property real hostHeight: 740
        function setAnnotation(key, value) {
            let options = Object.assign({}, controller.exportAnnotation);
            options[key] = value; controller.exportAnnotation = options;
        }
        function refreshTechnicalText() {
            const value = controller.captionTechnicalText;
            if (technicalText.text === value) return;
            technicalText.synchronizing = true;
            technicalText.text = value;
            technicalText.synchronizing = false;
        }
        function restoreTechnicalText() {
            const options = Object.assign({}, controller.exportAnnotation);
            delete options.technicalText;
            controller.exportAnnotation = options;
        }
        Connections {
            target: editor.controller
            function onAnnotationChanged() { editor.refreshTechnicalText(); }
        }
        Component.onCompleted: refreshTechnicalText()
        objectName: "annotationPopup"
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(previewOpen?1100:460, editor.hostWidth - 32)
        height: previewOpen?Math.min(740, editor.hostHeight-32):Math.min(editor.hostHeight-32,editor.controller.exportAnnotation.enabled?620:360)
        padding: 20; modal: true; focus: true
        closePolicy:controller.busy?Popup.NoAutoClose:Popup.CloseOnEscape|Popup.CloseOnPressOutside
        background: Rectangle { color: Atelier.surface; radius: 10; border.color: Atelier.border }
        ColumnLayout {
            anchors.fill: parent
            RowLayout {Layout.fillWidth:true
                Label {text:qsTr("Export final image");color:Atelier.text;font.pixelSize:16;font.weight:Font.DemiBold;Layout.fillWidth:true}
                IconAction {iconName:"close";text:qsTr("Close");enabled:!editor.controller.busy;onClicked:editor.close()}
            }
            Label {text:qsTr("Format");color:Atelier.secondary}
            DarkComboBox {id:exportFormat;objectName:"captionExportFormat";Layout.fillWidth:true;model:editor.compressedFormats?[qsTr("PNG · 16-bit, lossless"),qsTr("TIFF · 16-bit, lossless"),qsTr("JPEG · compressed")]:[qsTr("TIFF · 16-bit, lossless")];property string suffix:editor.compressedFormats?["png","tif","jpg"][currentIndex]:"tif"}
            Label {text:exportFormat.suffix==="jpg"?qsTr("Lossy compression · 8-bit · for sharing"):qsTr("Lossless · preserves detail and precision");color:Atelier.secondary;font.pixelSize:12;Layout.fillWidth:true;wrapMode:Text.Wrap}
            Label {text:editor.controller.imageWidth+" × "+editor.controller.imageHeight+" px";color:Atelier.text;font.pixelSize:13}
            RowLayout {Layout.fillWidth:true
                CheckBox {objectName:"annotationEnabled";text:qsTr("Add a caption");checked:editor.controller.exportAnnotation.enabled;enabled:!editor.controller.busy;onClicked:{editor.setAnnotation("enabled",checked);editor.previewRequested()}}
                Item {Layout.fillWidth:true}
                IconButton {text:editor.previewOpen?qsTr("Hide preview"):qsTr("Preview");ghost:true;onClicked:editor.previewOpen=!editor.previewOpen}
            }
            GridLayout {
                columns:editor.previewOpen&&editor.width>=800?2:1
                visible:editor.previewOpen||editor.controller.exportAnnotation.enabled
                Layout.fillWidth:true;Layout.fillHeight:true;columnSpacing:18;rowSpacing:12
                ImageViewport {
                    id:captionViewport;objectName:"captionPreview";visible:editor.previewOpen
                    Layout.fillWidth:true;Layout.fillHeight:editor.width>=800
                    Layout.preferredWidth:600;Layout.minimumWidth:120
                    Layout.preferredHeight:editor.width>=800?400:180;Layout.minimumHeight:140
                    sourceWidth:editor.controller.imageWidth;sourceHeight:editor.controller.imageHeight
                    Loader {active:editor.visible;parent:captionViewport.imageContainer;anchors.fill:parent;sourceComponent:editor.previewContent}
                }
            ScrollView {
                id:captionSettings
                visible:editor.controller.exportAnnotation.enabled
                Layout.fillWidth: true; Layout.fillHeight: true;Layout.preferredWidth:360;Layout.minimumWidth:280
                contentWidth: availableWidth; clip: true
                ScrollBar.vertical.policy: ScrollBar.AlwaysOff
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                ColumnLayout {
                    width: captionSettings.availableWidth; spacing: 12
                    enabled:!editor.controller.busy
                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: qsTr("Image and processing information"); color: Atelier.secondary; wrapMode: Text.Wrap; Layout.fillWidth: true }
                        CaptionSwitch {
                            objectName: "annotationTechnicalEnabled"
                            checked: editor.controller.exportAnnotation.technical
                            Accessible.name: qsTr("Include image and processing information")
                            onToggled: editor.setAnnotation("technical", checked)
                        }
                    }
                    TextArea {
                        id: technicalText
                        objectName: "annotationTechnicalText"
                        property bool synchronizing: false
                        visible: editor.controller.exportAnnotation.technical
                        Layout.fillWidth: true; Layout.minimumHeight: 96
                        Accessible.name: qsTr("Image and processing information")
                        wrapMode: TextEdit.Wrap; selectByMouse: true
                        color: Atelier.text; selectionColor: Atelier.accent; selectedTextColor: Atelier.canvas
                        background: Rectangle { color: Atelier.field; radius: Atelier.controlRadius; border.color: parent.activeFocus ? Atelier.accent : Atelier.border }
                        onTextChanged: if (!synchronizing) editor.setAnnotation("technicalText", text)
                    }
                    RowLayout {
                        visible: editor.controller.exportAnnotation.technical
                        Layout.fillWidth: true
                        Label { text: qsTr("You can correct this text before export."); color: Atelier.muted; font.pixelSize: 12; wrapMode: Text.Wrap; Layout.fillWidth: true }
                        IconAction {
                            objectName: "annotationTechnicalReset"
                            iconName: "refresh"; text: qsTr("Restore suggested information")
                            enabled: editor.controller.exportAnnotation.technicalText !== undefined
                            onClicked: editor.restoreTechnicalText()
                        }
                    }
                    Label { text: qsTr("Title · above the caption"); color: Atelier.secondary }
                    TextField {
                        objectName: "annotationTitle"
                        Layout.fillWidth: true
                        text: editor.controller.exportAnnotation.title
                        maximumLength: 120; selectByMouse: true
                        placeholderText: qsTr("Moon · Sea of Tranquillity")
                        color: Atelier.text; placeholderTextColor: Atelier.muted
                        background: Rectangle { color: Atelier.field; radius: 6; border.color: parent.activeFocus ? Atelier.accent : Atelier.border }
                        onTextChanged: editor.setAnnotation("title", text)
                    }
                    Label { text: qsTr("Top left · object, date, notes"); color: Atelier.secondary }
                    TextArea {
                        objectName: "annotationTop"
                        Layout.fillWidth: true; Layout.preferredHeight: 95
                        text: editor.controller.exportAnnotation.top
                        placeholderText: qsTr("Object · observation date (UTC)\nFilter, conditions…")
                        wrapMode: TextEdit.Wrap; selectByMouse: true
                        color: Atelier.text; placeholderTextColor: Atelier.muted
                        background: Rectangle { color: Atelier.field; radius: 6; border.color: parent.activeFocus ? Atelier.accent : Atelier.border }
                        onTextChanged: editor.setAnnotation("top", text)
                    }
                    Label { text: qsTr("Bottom left · observer and equipment"); color: Atelier.secondary }
                    TextArea {
                        objectName: "annotationBottom"
                        Layout.fillWidth: true; Layout.preferredHeight: 75
                        text: editor.controller.exportAnnotation.bottom
                        placeholderText: qsTr("Observer · location\nTelescope · camera · accessories")
                        wrapMode: TextEdit.Wrap; selectByMouse: true
                        color: Atelier.text; placeholderTextColor: Atelier.muted
                        background: Rectangle { color: Atelier.field; radius: 6; border.color: parent.activeFocus ? Atelier.accent : Atelier.border }
                        onTextChanged: editor.setAnnotation("bottom", text)
                    }
                    RowLayout {
                        Caption { text: qsTr("Text color") }
                        IconButton { text: qsTr("White"); borderColor: !editor.controller.exportAnnotation.gold ? Atelier.accent : Atelier.border; onClicked: editor.setAnnotation("gold", false) }
                        IconButton { text: qsTr("Gold"); borderColor: editor.controller.exportAnnotation.gold ? Atelier.accent : Atelier.border; onClicked: editor.setAnnotation("gold", true) }
                    }
                    RowLayout {
                        Caption { text: qsTr("Text size") }
                        DarkSlider { Layout.fillWidth: true; from: 0.8; to: 3; stepSize: 0.1; value: editor.controller.exportAnnotation.size; onMoved: editor.setAnnotation("size", value) }
                    }

                }
            }
            }
            ExportSizeHint {controller:editor.controller;format:exportFormat.suffix;estimating:editor.visible;ready:!editor.controller.busy;Layout.fillWidth:true}
            RowLayout {Layout.fillWidth:true
                Label {text:editor.exportStarted?editor.controller.status:"";color:Atelier.secondary;wrapMode:Text.Wrap;Layout.fillWidth:true;Layout.maximumHeight:50}
                IconButton {text:qsTr("Cancel export");visible:editor.controller.busy;onClicked:editor.controller.cancel()}

                IconButton {objectName:"captionExport";text:qsTr("Export");iconName:"export";primary:true;enabled:!editor.controller.busy && editor.controller.hasResult;onClicked:{saveCaption.currentFolder=editor.library.exportFolderFor(editor.controller.exportUrl);saveCaption.selectedFile=editor.library.suggestedExport(editor.controller.exportUrl).toString().replace(/\.[^/.]+$/, "."+exportFormat.suffix);saveCaption.open()}}
                IconButton {text:qsTr("Close");enabled:!editor.controller.busy;onClicked:editor.close()}
            }
        }
        FileDialog {id:saveCaption;title:qsTr("Export image");fileMode:FileDialog.SaveFile;nameFilters:exportFormat.suffix==="png"?[qsTr("16-bit PNG image (*.png)")]:exportFormat.suffix==="tif"?[qsTr("16-bit TIFF image (*.tif *.tiff)")]:[qsTr("JPEG image (*.jpg *.jpeg)")];defaultSuffix:exportFormat.suffix;onAccepted:{editor.exportStarted=true;let path=selectedFile.toString();if(!/\.(png|tiff?|jpe?g)$/i.test(path))path+="."+exportFormat.suffix;editor.controller.exportImage(path)}}
        onOpened:{exportStarted=false;if(controller.showResult)controller.showResult(false)}
        component CaptionSwitch: Switch {
        id: control
        indicator: Rectangle {
            implicitWidth: 38; implicitHeight: 22
            x: control.leftPadding; y: (control.height - height) / 2
            radius: 11; color: control.checked ? Atelier.accent : Atelier.border
            Rectangle { width: 16; height: 16; radius: 8; y: 3; x: control.checked ? 19 : 3; color: Atelier.text }
        }
        background: Rectangle { color: "transparent"; radius: 6; border.color: control.visualFocus ? Atelier.text : "transparent" }
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
            color: Atelier.border
            Rectangle { width: control.visualPosition * parent.width; height: parent.height; radius: 2; color: control.enabled ? Atelier.accent : "#445164" }
        }
        handle: Rectangle {
            x: control.leftPadding + control.visualPosition * (control.availableWidth - width)
            y: control.topPadding + control.availableHeight / 2 - height / 2
            implicitWidth: 14; implicitHeight: 14
            radius: 7
            color: control.enabled ? Atelier.accent : "#566170"
        }
    }
    component Caption: Label { color: Atelier.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true; font.pixelSize: 12 }
}
