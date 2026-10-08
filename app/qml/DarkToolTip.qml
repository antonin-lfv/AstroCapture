import QtQuick
import QtQuick.Controls.Basic

ToolTip {
    id: tip
    objectName: "darkHelpTip"
    width: Math.min(380, Overlay.overlay ? Overlay.overlay.width - 32 : 380)
    x: 0
    y: parent ? parent.height + 6 : 0
    delay: 350
    timeout: -1
    padding: 12
    contentItem: Text {
        text: tip.text
        color: "#eef3fa"
        font.pixelSize: 13
        wrapMode: Text.WordWrap
    }
    background: Rectangle {
        color: "#151b24"
        radius: 8
        border.color: "#39465a"
    }
}
