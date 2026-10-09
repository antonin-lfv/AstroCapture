import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "Atelier.js" as Atelier

CheckBox {
    id: control
    property string explanation: ""
    Accessible.description: explanation
    Layout.minimumWidth: 0
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
