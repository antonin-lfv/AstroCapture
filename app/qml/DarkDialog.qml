import "Atelier.js" as Atelier
import QtQuick
import QtQuick.Controls.Basic

Dialog {
    id: dialog
    padding: 18
    palette.window: Atelier.surface
    palette.windowText: Atelier.text
    palette.base: Atelier.field
    palette.text: Atelier.text
    palette.placeholderText: Atelier.muted
    palette.button: Atelier.field
    palette.buttonText: Atelier.text
    palette.highlight: Atelier.accent
    palette.highlightedText: Atelier.canvas
    palette.mid: Atelier.border
    background: Rectangle { color: Atelier.surface; radius: Atelier.dialogRadius; border.color: Atelier.border }
    header: Label {
        text: dialog.title
        color: Atelier.text
        font.pixelSize: 16
        font.bold: true
        wrapMode: Text.WordWrap
        padding: 18
        visible: text.length > 0
    }
}
