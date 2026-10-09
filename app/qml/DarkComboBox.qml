import "Atelier.js" as Atelier
import QtQuick
import QtQuick.Controls.Basic

ComboBox {
    id:control
    implicitHeight:36
    implicitWidth:Math.max(100,contentItem.implicitWidth+48)
    leftPadding:12
    rightPadding:32
    font.pixelSize:12
    palette.text:Atelier.text
    palette.buttonText:Atelier.text
    palette.highlight:"#314964"
    palette.highlightedText:"#ffffff"
    contentItem:Text {
        text:control.displayText
        color:control.enabled?Atelier.text:Atelier.muted
        font:control.font
        verticalAlignment:Text.AlignVCenter
        elide:Text.ElideRight
    }
    indicator:Text {
        x:control.width-width-12;y:(control.height-height)/2
        text:"▾";font.pixelSize:16;color:control.enabled?"#a9c4e6":Atelier.muted
    }
    background:Rectangle {
        radius:6
        color:control.down?"#27384d":control.hovered?Atelier.hover:Atelier.field
        border.color:control.activeFocus?Atelier.accent:control.hovered?"#6789b1":Atelier.border
    }
    delegate:ItemDelegate {
        id:choice
        width:control.popup.width-2
        text:control.textAt(index)
        highlighted:control.highlightedIndex===index
        contentItem:Text {text:choice.text;color:Atelier.text;font:control.font;elide:Text.ElideRight;verticalAlignment:Text.AlignVCenter}
        background:Rectangle {radius:4;color:choice.highlighted||choice.hovered?"#314964":Atelier.surface}
    }
    popup:Popup {
        y:control.height+4;width:control.width
        padding:4
        implicitHeight:Math.min(contentItem.implicitHeight+8,320)
        contentItem:ListView {
            clip:true;implicitHeight:contentHeight
            model:control.popup.visible?control.delegateModel:null
            currentIndex:control.highlightedIndex
            ScrollIndicator.vertical:ScrollIndicator {}
        }
        background:Rectangle {color:Atelier.surface;radius:6;border.color:"#506681"}
    }
}
