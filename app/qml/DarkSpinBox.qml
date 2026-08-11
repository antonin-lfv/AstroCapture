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
        color: "#e8eaed"
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
        color: control.up.pressed ? "#2a3038" : "#1c2026"
        border.color: "#3a4048"
        border.width: 1

        Text {
            text: "+"
            anchors.centerIn: parent
            color: "#e8eaed"
            font.pixelSize: 13
        }
    }

    down.indicator: Rectangle {
        x: 0
        width: 24
        height: control.height
        color: control.down.pressed ? "#2a3038" : "#1c2026"
        border.color: "#3a4048"
        border.width: 1

        Text {
            text: "−"
            anchors.centerIn: parent
            color: "#e8eaed"
            font.pixelSize: 13
        }
    }

    background: Rectangle {
        implicitHeight: 28
        color: "#14171c"
        border.color: "#3a4048"
        border.width: 1
        radius: 6
    }
}
