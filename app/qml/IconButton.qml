import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// Shared button chrome: icon + optional label, consistent hover/pressed/
// disabled states. Replaces the ad hoc contentItem/background pairs that
// used to be duplicated on every Button in Main.qml.
Button {
    id: control

    property string iconName: ""
    property color borderColor: "#3a4048"
    property color activeBorderColor: borderColor
    property bool active: false

    padding: 0
    leftPadding: 10
    rightPadding: 10
    topPadding: 7
    bottomPadding: 7

    contentItem: RowLayout {
        spacing: 6

        Icon {
            visible: control.iconName.length > 0
            name: control.iconName
            color: control.enabled ? "#e8eaed" : "#5f6368"
            Layout.preferredWidth: 14
            Layout.preferredHeight: 14
            Layout.alignment: Qt.AlignVCenter
        }

        Text {
            visible: control.text.length > 0
            text: control.text
            color: control.enabled ? "#e8eaed" : "#5f6368"
            font.pixelSize: 12
            Layout.alignment: Qt.AlignVCenter
        }
    }

    background: Rectangle {
        radius: 6
        color: control.active
               ? "#3a1d1f"
               : (control.pressed ? "#252b32" : (control.hovered ? "#2a3038" : "#1c2026"))
        border.color: control.active ? control.activeBorderColor : control.borderColor
        border.width: 1
    }
}
