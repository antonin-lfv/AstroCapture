import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Rectangle {
    id:root
    property string currentTab:"home"
    property bool recording:false
    property string activityText:""
    property string activityTab:""
    signal navigate(string tab)
    readonly property var tabs:[
        {key:"home",label:qsTr("Home"),icon:"home"},
        {key:"capture",label:qsTr("Capture"),icon:"camera"},
        {key:"processing",label:qsTr("Processing"),icon:"processing"},
        {key:"deepSky",label:qsTr("Deep sky"),icon:"stars"},
        {key:"mosaic",label:qsTr("Mosaic"),icon:"mosaic"},
        {key:"history",label:qsTr("History"),icon:"history"},
        {key:"guide",label:qsTr("Guide"),icon:"book"}
    ]
    implicitHeight:104
    color:"#10141b"
    ColumnLayout {
        anchors.fill:parent;spacing:0
        RowLayout {
            Layout.fillWidth:true;Layout.preferredHeight:52
            Layout.leftMargin:20;Layout.rightMargin:16;spacing:10
            Icon {name:"stars";color:"#91b8ff";Layout.preferredWidth:24;Layout.preferredHeight:24}
            Label {text:"AstroCapture";font.pixelSize:16;font.weight:Font.DemiBold;color:"#eef3fa"}
            Label {text:Qt.application.version;font.pixelSize:12;color:"#9aa7b8"}
            Item {Layout.fillWidth:true}
            Button {
                objectName:"studioActivity";visible:root.activityText.length>0
                Layout.maximumWidth:Math.max(100,root.width*.3)
                onClicked:root.navigate(root.activityTab)
                contentItem:Label {text:root.activityText;color:root.recording?"#f5aaa4":"#91b8ff";elide:Text.ElideRight;font.pixelSize:12}
                background:Item {}
                DarkToolTip {visible:parent.hovered;text:root.activityText}
            }
            ToolButton {
                objectName:"studioSettings";implicitWidth:38;implicitHeight:38;padding:8
                enabled:!root.recording
                Accessible.name:qsTr("Settings")
                onClicked:root.navigate("settings")
                contentItem:Icon {name:"gear";color:root.currentTab==="settings"||parent.hovered?"#91b8ff":"#a2aec0"}
                background:Rectangle {radius:6;color:parent.hovered||root.currentTab==="settings"?"#253348":"transparent"}
                DarkToolTip {visible:parent.hovered;text:qsTr("Settings")}
            }
        }
        Rectangle {Layout.fillWidth:true;Layout.preferredHeight:1;color:"#29313d"}
        RowLayout {
            Layout.fillWidth:true;Layout.fillHeight:true;Layout.leftMargin:12;Layout.rightMargin:12;spacing:3
            Repeater {
                model:root.tabs
                delegate:Button {
                    id:tab;required property var modelData
                    objectName:"studioTab_"+modelData.key
                    readonly property bool selected:root.currentTab===modelData.key
                    enabled:!root.recording || ["home","capture","guide"].indexOf(modelData.key)>=0
                    Accessible.name:modelData.label;Accessible.role:Accessible.PageTab
                    Accessible.description:selected?qsTr("Current workspace"):""
                    leftPadding:root.width<900?9:12;rightPadding:leftPadding
                    topPadding:9;bottomPadding:9
                    onClicked:root.navigate(modelData.key)
                    contentItem:RowLayout {
                        spacing:7
                        Icon {visible:root.width>=900;name:tab.modelData.icon;color:tab.selected?"#eef3fa":"#a2aec0";Layout.preferredWidth:16;Layout.preferredHeight:16}
                        Label {text:tab.modelData.label;color:!tab.enabled?"#687486":tab.selected?"#eef3fa":"#a2aec0";font.pixelSize:13}
                    }
                    background:Rectangle {
                        radius:6;color:tab.selected?"#253348":tab.hovered?"#1a222e":"transparent"
                        Rectangle {anchors.bottom:parent.bottom;anchors.left:parent.left;anchors.right:parent.right;anchors.margins:5;height:2;radius:1;color:"#91b8ff";visible:tab.selected}
                    }
                }
            }
            Item {Layout.fillWidth:true}
        }
        Rectangle {Layout.fillWidth:true;Layout.preferredHeight:1;color:"#29313d"}
    }
}
