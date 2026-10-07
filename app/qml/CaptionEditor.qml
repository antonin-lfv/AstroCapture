import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
Popup {
        id: editor
        required property var controller
        signal previewRequested()
        property real hostWidth: 510
        property real hostHeight: 640
        function setAnnotation(key, value) {
            let options = Object.assign({}, controller.exportAnnotation);
            options[key] = value; controller.exportAnnotation = options;
        }
        objectName: "annotationPopup"
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(510, editor.hostWidth - 32)
        height: Math.min(640, editor.hostHeight - 32)
        padding: 18; modal: true; focus: true
        background: Rectangle { color: "#151b24"; radius: 10; border.color: "#39465a" }
        ColumnLayout {
            anchors.fill: parent
            Label { text: qsTr("Image caption"); color: "#eef3fa"; font.pixelSize: 18 }
            ScrollView {
                Layout.fillWidth: true; Layout.fillHeight: true
                contentWidth: availableWidth; clip: true
                ScrollBar.vertical.policy: ScrollBar.AlwaysOff
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                ColumnLayout {
                    width: parent.width; spacing: 12
                    Label {text:editor.controller.stackSummary || "";visible:text.length>0;color:"#a8b5c6";wrapMode:Text.Wrap;Layout.fillWidth:true}
                    Caption { text: qsTr("Optional text printed on the TIFF. Enter your observation details; dates and planetary coordinates are not calculated. Each area accepts 400 characters. Check the preview for clipping.") }
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
            IconButton { text: qsTr("Done"); Layout.alignment: Qt.AlignRight; onClicked: editor.close() }
        }
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
