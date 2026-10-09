import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "Atelier.js" as Atelier

MenuItem {
    id: entry
    property string description: ""
    property bool destructive: false
    implicitHeight: Math.max(32,contentItem.implicitHeight+12)
    leftPadding: 8;rightPadding:8;topPadding:6;bottomPadding:6
    contentItem: ColumnLayout {
        spacing: 2
        Label {text:entry.text;color:entry.destructive?Atelier.danger:entry.enabled?Atelier.text:Atelier.muted;Layout.fillWidth:true;wrapMode:Text.Wrap;font.pixelSize:13}
        Label {text:entry.description;visible:text.length>0;color:Atelier.secondary;Layout.fillWidth:true;wrapMode:Text.Wrap;font.pixelSize:11}
    }
    background: Rectangle {radius:Atelier.controlRadius;color:entry.highlighted?Atelier.hover:"transparent";border.width:entry.activeFocus?1:0;border.color:Atelier.accent}
}
