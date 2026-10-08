import QtQuick
import QtQuick.Controls.Basic

Dialog {
    id: dialog
    padding: 18
    palette.window: "#0b0d10"
    palette.windowText: "#eef3fa"
    palette.base: "#1c2026"
    palette.text: "#eef3fa"
    palette.placeholderText: "#9aa7b8"
    palette.button: "#1c2026"
    palette.buttonText: "#eef3fa"
    palette.highlight: "#6ca5f8"
    palette.highlightedText: "#0b0d10"
    palette.mid: "#39465a"
    background: Rectangle { color: "#0b0d10"; radius: 10; border.color: "#39465a" }
    header: Label {
        text: dialog.title
        color: "#eef3fa"
        font.pixelSize: 18
        font.bold: true
        wrapMode: Text.WordWrap
        padding: 18
        visible: text.length > 0
    }
}
