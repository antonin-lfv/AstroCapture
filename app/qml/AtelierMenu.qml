import QtQuick
import QtQuick.Controls.Basic
import "Atelier.js" as Atelier

Menu {
    width: 280
    padding: 6
    popupType: Popup.Item
    font.pixelSize: 13
    palette.window: Atelier.surface
    palette.base: Atelier.surface
    palette.text: Atelier.text
    palette.windowText: Atelier.text
    palette.buttonText: Atelier.text
    palette.highlight: Atelier.hover
    palette.highlightedText: Atelier.text
    background: Rectangle {color:Atelier.surface;radius:6;border.color:Atelier.border}
}
