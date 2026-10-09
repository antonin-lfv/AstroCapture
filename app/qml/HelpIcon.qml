import "Atelier.js" as Atelier
import QtQuick
import QtQuick.Controls.Basic

Item {
    id: root
    objectName: "helpIcon"
    property string text: ""
    property int maxWidth: 340
    implicitWidth: 20
    implicitHeight: 20
    activeFocusOnTab: true
    Accessible.role: Accessible.Button
    Accessible.name: qsTr("Help")
    Accessible.description: text
    Accessible.onPressAction: forceActiveFocus()
    Rectangle {anchors.fill:parent;radius:10;color:"transparent";border.color:hover.hovered||root.activeFocus?Atelier.accent:Atelier.border}
    Text {anchors.centerIn:parent;text:"?";font.bold:true;font.pixelSize:14;color:hover.hovered||root.activeFocus?Atelier.text:Atelier.muted}
    HoverHandler {id:hover}
    DarkToolTip {parent:root;width:Math.min(root.maxWidth,Overlay.overlay?Overlay.overlay.width-24:root.maxWidth);text:root.text;visible:root.visible && (hover.hovered||root.activeFocus) && text.length>0}
}
