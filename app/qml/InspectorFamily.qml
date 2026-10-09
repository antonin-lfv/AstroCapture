import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "Atelier.js" as Atelier

ColumnLayout {
    id: family
    property string title: ""
    property bool expanded: false
    property bool current: false
    default property alias content: controls.children
    signal activated()
    signal interacted()
    Layout.fillWidth: true
    spacing: 0
    Button {
        id: heading
        objectName: family.objectName + "Header"
        Layout.fillWidth: true
        implicitHeight: 34
        padding: 0
        Accessible.name: family.title
        Accessible.role: Accessible.Button
        Accessible.description: family.expanded?qsTr("Collapse this adjustment family"):qsTr("Expand this adjustment family")
        onClicked: family.activated()
        contentItem: RowLayout {
            spacing: 8
            Label {text:family.expanded?"⌄":"›";color:Atelier.muted;font.pixelSize:13;Layout.preferredWidth:12}
            Label {text:family.title;color:family.current&&family.expanded?Atelier.accent:Atelier.text;font.pixelSize:13;font.weight:Font.Medium;Layout.fillWidth:true;elide:Text.ElideRight}
        }
        background: Rectangle {color:heading.hovered?Atelier.toolbar:"transparent";border.width:heading.activeFocus?1:0;border.color:Atelier.accent;Rectangle {anchors.bottom:parent.bottom;width:parent.width;height:1;color:Atelier.border}}
    }
    FocusScope {
        id: slot
        visible: family.expanded
        Layout.fillWidth: true
        implicitHeight: controls.implicitHeight + 16
        onActiveFocusChanged: if(activeFocus) family.interacted()
        PointHandler { acceptedButtons: Qt.LeftButton; onActiveChanged: if(active) family.interacted() }
        ColumnLayout {
            id: controls
            anchors { left: parent.left; right: parent.right; top: parent.top; topMargin: 8; bottomMargin: 8 }
            spacing: 8
        }
    }
}
