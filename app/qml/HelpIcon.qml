import QtQuick
import QtQuick.Controls.Basic

Item {
    id: root
    property string text: ""
    property int maxWidth: 260
    implicitWidth: 16
    implicitHeight: 16
    Accessible.name: text
    Text {anchors.centerIn:parent;text:"i";font.italic:true;font.pixelSize:14;color:hover.hovered?"#eef3fa":"#9aa7b8"}
    HoverHandler {id:hover}
    DarkToolTip {parent:root;width:Math.min(root.maxWidth,380);text:root.text;visible:hover.hovered && text.length>0}
}
