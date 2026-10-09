import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "Atelier.js" as Atelier

ColumnLayout {
    id: family
    property string title: ""
    property bool expanded: false
    property bool headerVisible: true
    property alias body: slot
    signal activated()
    Layout.fillWidth: true
    Layout.fillHeight: expanded
    spacing: 0
    Button {
        id: heading
        visible: family.headerVisible
        Layout.fillWidth: true
        implicitHeight: 34
        padding: 0
        Accessible.name: family.title
        Accessible.role: Accessible.PageTab
        Accessible.description: family.expanded?qsTr("Current adjustment family"):""
        onClicked: family.activated()
        contentItem: RowLayout {
            spacing: 8
            Label {text:family.expanded?"⌄":"›";color:Atelier.muted;font.pixelSize:13;Layout.preferredWidth:12}
            Label {text:family.title;color:Atelier.text;font.pixelSize:13;font.weight:Font.Medium;Layout.fillWidth:true;elide:Text.ElideRight}
        }
        background: Rectangle {color:heading.hovered?Atelier.toolbar:"transparent";border.width:heading.activeFocus?1:0;border.color:Atelier.accent;Rectangle {anchors.bottom:parent.bottom;width:parent.width;height:1;color:Atelier.border}}
    }
    Item {id:slot;visible:family.expanded;Layout.fillWidth:true;Layout.fillHeight:true;Layout.minimumHeight:0}
}
