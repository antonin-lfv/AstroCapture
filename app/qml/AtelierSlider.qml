import QtQuick
import QtQuick.Controls.Basic
import "Atelier.js" as Atelier

Slider {
    id: control
    implicitHeight: 24
    background: Rectangle {
        x:control.leftPadding;y:control.topPadding+control.availableHeight/2-height/2
        width:control.availableWidth;height:3;radius:1.5;color:Atelier.border
        Rectangle {width:control.visualPosition*parent.width;height:parent.height;radius:1.5;color:control.enabled?Atelier.accent:Atelier.muted}
    }
    handle: Rectangle {
        x:control.leftPadding+control.visualPosition*(control.availableWidth-width)
        y:control.topPadding+control.availableHeight/2-height/2
        implicitWidth:12;implicitHeight:12;radius:6;color:control.enabled?Atelier.accent:Atelier.muted
        border.width:control.activeFocus?1:0;border.color:Atelier.text
    }
}
