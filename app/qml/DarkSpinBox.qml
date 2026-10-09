import "Atelier.js" as Atelier
import QtQuick
import QtQuick.Controls.Basic

// SpinBox styled to match the rest of the app (Basic style's default
// indicators/background are light and clash with the always-dark window,
// same issue IconButton exists to solve for Button).
SpinBox {
    id: control

    editable: true

    contentItem: TextInput {
        text: control.textFromValue(control.value, control.locale)
        color: Atelier.text
        font.pixelSize: 12
        horizontalAlignment: Qt.AlignHCenter
        verticalAlignment: Qt.AlignVCenter
        readOnly: !control.editable
        validator: control.validator
        selectByMouse: true
    }

    up.indicator: Rectangle {
        x: control.width - width
        width: 24
        height: control.height
        color: control.up.pressed ? Atelier.hover : Atelier.field
        border.color: Atelier.border
        border.width: 1

        Text {
            text: "+"
            anchors.centerIn: parent
            color: Atelier.text
            font.pixelSize: 13
        }
    }

    down.indicator: Rectangle {
        x: 0
        width: 24
        height: control.height
        color: control.down.pressed ? Atelier.hover : Atelier.field
        border.color: Atelier.border
        border.width: 1

        Text {
            text: "−"
            anchors.centerIn: parent
            color: Atelier.text
            font.pixelSize: 13
        }
    }

    background: Rectangle {
        implicitHeight: 28
        color: "#14171c"
        border.color: Atelier.border
        border.width: 1
        radius: 6
    }
}
