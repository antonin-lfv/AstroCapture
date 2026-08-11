import QtQuick
import QtQuick.Controls.Basic

// Small clickable "?" affordance next to a label, showing `text` in a
// popup on click. Clickable rather than hover-only: a plain ToolTip
// requires already knowing something is hoverable to discover it exists,
// which defeats the point for text meant to explain something unfamiliar.
Item {
    id: root

    property string text: ""
    property int maxWidth: 260

    implicitWidth: 14
    implicitHeight: 14

    Rectangle {
        anchors.fill: parent
        radius: width / 2
        color: mouseArea.containsMouse ? "#2a3038" : "#1c2026"
        border.color: "#3a4048"
        border.width: 1

        Text {
            anchors.centerIn: parent
            text: "?"
            color: "#9aa0a6"
            font.pixelSize: 9
            font.weight: Font.DemiBold
        }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: popup.open()
    }

    Popup {
        id: popup
        parent: Overlay.overlay
        x: root.mapToItem(Overlay.overlay, 0, root.height + 6).x
        y: root.mapToItem(Overlay.overlay, 0, root.height + 6).y
        width: root.maxWidth
        padding: 10
        modal: false
        focus: true
        closePolicy: Popup.CloseOnPressOutside | Popup.CloseOnEscape

        background: Rectangle {
            color: "#15181d"
            radius: 8
            border.color: "#3a4048"
            border.width: 1
        }

        contentItem: Text {
            text: root.text
            color: "#c8ccd1"
            font.pixelSize: 11
            wrapMode: Text.Wrap
        }
    }
}
