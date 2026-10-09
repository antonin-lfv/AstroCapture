import "Atelier.js" as Atelier
import QtQuick
import QtQuick.Controls.Basic

Item {
    id: root
    objectName: "helpIcon"
    property string text: ""
    property int maxWidth: 260
    implicitWidth: 20
    implicitHeight: 20
    Accessible.name: text
    Rectangle {anchors.fill:parent;radius:10;color:"transparent";border.color:hover.hovered?Atelier.accent:"#56657a"}
    Text {anchors.centerIn:parent;text:"?";font.bold:true;font.pixelSize:14;color:hover.hovered?Atelier.text:Atelier.muted}
    HoverHandler {id:hover}
    DarkToolTip {parent:root;width:Math.min(root.maxWidth,380);text:root.text;visible:hover.hovered && text.length>0}
}
