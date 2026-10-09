import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// Shared button chrome: icon + optional label, consistent hover/pressed/
// disabled states. Replaces the ad hoc contentItem/background pairs that
// used to be duplicated on every Button in Main.qml.
Button {
    id: control

    property string iconName: ""
    property string helpText: ""
    property color borderColor: "#3a4048"
    property color activeBorderColor: borderColor
    property bool active: false

    implicitHeight: Math.max(34, contentItem.implicitHeight + topPadding + bottomPadding)
    hoverEnabled: true
    padding: 0
    leftPadding: 10
    rightPadding: helpText.length>0 ? 36 : 10
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
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
        }
    }

    HelpIcon {anchors.right:parent.right;anchors.verticalCenter:parent.verticalCenter;anchors.rightMargin:8;visible:control.helpText.length>0;text:control.helpText}
    background: Rectangle {
        radius: 6
        color: control.active
               ? "#3a1d1f"
               : (control.pressed ? "#252b32" : (control.hovered ? "#2a3038" : "#1c2026"))
        border.color: control.activeFocus ? "#8dbaff" : control.active ? control.activeBorderColor : control.borderColor
        border.width: 1
    }
}
