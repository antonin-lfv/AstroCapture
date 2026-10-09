import "Atelier.js" as Atelier
import QtQuick
import QtQuick.Controls.Basic

ToolButton {
    id: action
    property string iconName: ""
    property string hint: text
    implicitWidth: Atelier.controlHeight
    implicitHeight: Atelier.controlHeight
    padding: 7
    Accessible.name: text
    contentItem: Icon {
        name: action.iconName
        color: !action.enabled ? Atelier.muted : action.hovered ? Atelier.accent : Atelier.text
    }
    background: Rectangle {
        radius: Atelier.controlRadius
        color: action.pressed ? Atelier.hover : action.hovered ? Atelier.hover : "transparent"
        border.width: action.activeFocus ? 1 : 0
        border.color: Atelier.accent
    }
    DarkToolTip {
        visible: action.hovered || action.activeFocus
        text: action.hint
        width: Math.min(280, contentItem.implicitWidth + leftPadding + rightPadding)
    }
}
