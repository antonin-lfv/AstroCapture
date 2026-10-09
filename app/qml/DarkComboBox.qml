import QtQuick
import QtQuick.Controls.Basic

ComboBox {
    id:control
    implicitHeight:36
    implicitWidth:Math.max(100,contentItem.implicitWidth+48)
    leftPadding:12
    rightPadding:32
    font.pixelSize:12
    palette.text:"#eef3fa"
    palette.buttonText:"#eef3fa"
    palette.highlight:"#314964"
    palette.highlightedText:"#ffffff"
    contentItem:Text {
        text:control.displayText
        color:control.enabled?"#eef3fa":"#788496"
        font:control.font
        verticalAlignment:Text.AlignVCenter
        elide:Text.ElideRight
    }
    indicator:Text {
        x:control.width-width-12;y:(control.height-height)/2
        text:"▾";font.pixelSize:16;color:control.enabled?"#a9c4e6":"#657184"
    }
    background:Rectangle {
        radius:6
        color:control.down?"#27384d":control.hovered?"#253348":"#1c2026"
        border.color:control.activeFocus?"#8dbaff":control.hovered?"#6789b1":"#3a4048"
    }
    delegate:ItemDelegate {
        id:choice
        width:control.popup.width-2
        text:control.textAt(index)
        highlighted:control.highlightedIndex===index
        contentItem:Text {text:choice.text;color:"#eef3fa";font:control.font;elide:Text.ElideRight;verticalAlignment:Text.AlignVCenter}
        background:Rectangle {radius:4;color:choice.highlighted||choice.hovered?"#314964":"#151b24"}
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
        background:Rectangle {color:"#151b24";radius:6;border.color:"#506681"}
    }
}
