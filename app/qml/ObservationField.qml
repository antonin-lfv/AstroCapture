import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "Atelier.js" as Atelier
ColumnLayout {
    id: root
    property string label: ""
    property string help: ""
    property alias text: input.text
    property alias validator: input.validator
    readonly property bool editing: input.activeFocus
    signal accepted()
    signal textEdited()
    function focusInput(selectText) { input.forceActiveFocus(); if(selectText) input.selectAll(); }
    spacing: 5
    SettingLabel {text: root.label; explanation: root.help; Layout.fillWidth: true}
    TextField {
        id: input
        Layout.fillWidth: true
        implicitHeight: Atelier.controlHeight
        color: Atelier.text
        selectByMouse: true
        font.pixelSize: 13
        Accessible.name: root.label
        Accessible.description: root.help
        background: Rectangle {radius: Atelier.controlRadius; color: Atelier.field; border.color: input.activeFocus ? Atelier.accent : Atelier.border}
        onEditingFinished: root.accepted()
        onTextEdited: root.textEdited()
    }
}
