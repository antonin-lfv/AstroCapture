import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "Atelier.js" as Atelier

CheckBox {
    id: control
    property string explanation: ""
    Accessible.description: explanation
    Layout.minimumWidth: 0
    indicator: Rectangle {
        implicitWidth:20;implicitHeight:20
        x:control.leftPadding;y:(control.height-height)/2
        color:control.down?Atelier.hover:Atelier.field
        border.color:control.activeFocus?Atelier.accent:control.hovered?Atelier.secondary:Atelier.border
        radius:2;opacity:control.enabled?1:0.6
        Text {anchors.centerIn:parent;text:control.checkState===Qt.PartiallyChecked?"−":"✓";visible:control.checkState!==Qt.Unchecked;color:Atelier.text;font.pixelSize:16}
    }
    contentItem: RowLayout {
        spacing: 6
        Text {
            text: control.text
            font: control.font
            color: control.enabled ? Atelier.text : Atelier.muted
            leftPadding: control.indicator.width + control.spacing
            wrapMode: Text.Wrap
            verticalAlignment: Text.AlignVCenter
            Layout.fillWidth: true
            Layout.minimumWidth: 0
        }
        HelpIcon { text: control.explanation; visible: text.length > 0 }
    }
}
