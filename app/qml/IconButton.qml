import "Atelier.js" as Atelier
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
    property color borderColor: Atelier.border
    property color activeBorderColor: borderColor
    property bool active: false
    property bool primary: false
    property bool ghost: false
    property bool busy: false

    implicitHeight: Math.max(Atelier.controlHeight, contentItem.implicitHeight + topPadding + bottomPadding)
    hoverEnabled: true
    padding: 0
    leftPadding: 10
    rightPadding: helpText.length>0 ? 36 : 10
    topPadding: 6
    bottomPadding: 6

    contentItem: RowLayout {
        spacing: 6

        Item {
            visible: control.iconName.length > 0 || control.busy
            Layout.preferredWidth: 14
            Layout.preferredHeight: 14
            Layout.alignment: Qt.AlignVCenter
            Icon {
                anchors.fill: parent
                visible: !control.busy
                name: control.iconName
                color: !control.enabled ? Atelier.muted : control.primary ? Atelier.onPrimary : control.active ? Atelier.danger : Atelier.text
            }
            Icon {
                id: spinner
                objectName: "buttonActivitySpinner"
                anchors.fill: parent
                visible: control.busy
                name: "loading"
                color: control.primary ? Atelier.onPrimary : Atelier.accent
                RotationAnimator { target: spinner; from: 0; to: 360; duration: 900; loops: Animation.Infinite; running: control.busy && control.visible }
            }
        }

        Text {
            visible: control.text.length > 0
            text: control.text
            color: !control.enabled ? Atelier.muted : control.primary ? Atelier.onPrimary : control.active ? Atelier.danger : Atelier.text
            font.pixelSize: 13
            Layout.alignment: Qt.AlignVCenter
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
        }
    }

    HelpIcon {anchors.right:parent.right;anchors.verticalCenter:parent.verticalCenter;anchors.rightMargin:8;visible:control.helpText.length>0;text:control.helpText}
    background: Rectangle {
        radius: Atelier.controlRadius
        color: control.primary ? (control.hovered ? "#4d80b7" : Atelier.primary)
               : control.active ? (control.hovered ? Atelier.dangerSurface : "transparent")
               : control.hovered || control.pressed ? Atelier.hover : control.ghost ? "transparent" : Atelier.field
        border.color: control.activeFocus ? Atelier.accent : control.active ? Atelier.dangerSurface : control.primary || control.ghost ? "transparent" : control.borderColor
        border.width: 1
    }
}
