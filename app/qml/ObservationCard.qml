import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "Atelier.js" as Atelier

Rectangle {
    id: card
    property string title: ""
    property string help: ""
    default property alias content: contentColumn.data
    implicitHeight: body.implicitHeight + 32
    color: Atelier.surface
    radius: Atelier.controlRadius
    border.color: Atelier.border
    ColumnLayout {
        id: body
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 16
        spacing: 12
        RowLayout {
            Layout.fillWidth: true
            Label {
                text: card.title
                color: Atelier.text
                font.pixelSize: 14
                font.weight: Font.Medium
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
            HelpIcon {
                visible: card.help.length > 0
                text: card.help
            }
        }
        ColumnLayout {
            id: contentColumn
            Layout.fillWidth: true
            spacing: 8
        }
    }
}
