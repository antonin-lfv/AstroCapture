import QtQuick
import QtQuick.Controls.Basic

ToolButton {
    id: action
    property string iconName: ""
    property string hint: text
    implicitWidth: 36
    implicitHeight: 36
    padding: 8
    Accessible.name: text
    contentItem: Icon {
        name: action.iconName
        color: !action.enabled ? "#657184" : action.hovered ? "#a9caff" : "#dce6f4"
    }
    background: Rectangle {
        radius: 6
        color: action.pressed ? "#30435b" : action.hovered ? "#253348" : "transparent"
        border.width: action.activeFocus ? 1 : 0
        border.color: "#8dbaff"
    }
    DarkToolTip {
        visible: action.hovered || action.activeFocus
        text: action.hint
        width: Math.min(280, contentItem.implicitWidth + leftPadding + rightPadding)
    }
}
