import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "Atelier.js" as Atelier

RowLayout {
    id: root
    property alias text: label.text
    property alias color: label.color
    property alias font: label.font
    property alias wrapMode: label.wrapMode
    property string explanation: ""
    Layout.fillWidth: true
    Layout.minimumWidth: 0
    spacing: 6
    Label {
        id: label
        color: Atelier.secondary
        font.pixelSize: 13
        wrapMode: Text.Wrap
        Layout.fillWidth: true
        Layout.minimumWidth: 0
    }
    HelpIcon { text: root.explanation; visible: text.length > 0 }
}
