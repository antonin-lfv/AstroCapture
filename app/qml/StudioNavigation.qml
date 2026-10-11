import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "Atelier.js" as Atelier

Rectangle {
    id: root
    property string currentTab: "home"
    property bool recording: false
    property string activityText: ""
    property string activityTab: ""
    signal navigate(string tab)
    readonly property var tabs: [
        {key:"home",label:qsTr("Home")}, {key:"capture",label:qsTr("Capture")},
        {key:"processing",label:qsTr("Processing")}, {key:"deepSky",label:qsTr("Deep sky")},
        {key:"mosaic",label:qsTr("Mosaic")}, {key:"history",label:qsTr("History")},
        {key:"observation",label:qsTr("Observation")}, {key:"guide",label:qsTr("Guide")}
    ]
    implicitHeight: 44
    color: Atelier.toolbar
    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12;anchors.rightMargin:12
        spacing: 4
        Repeater {
            model: root.tabs
            delegate: Button {
                id: tab
                required property var modelData
                objectName: "studioTab_"+modelData.key
                readonly property bool selected: root.currentTab===modelData.key
                enabled: !root.recording || ["home","capture","observation","guide"].indexOf(modelData.key)>=0
                Accessible.name: modelData.label
                Accessible.role: Accessible.PageTab
                Accessible.description: selected?qsTr("Current workspace"):""
                implicitHeight: Atelier.controlHeight
                leftPadding: root.width<1100?9:12;rightPadding:leftPadding
                onClicked: root.navigate(modelData.key)
                contentItem: Label {text:tab.modelData.label;color:tab.enabled?(tab.selected?Atelier.text:Atelier.secondary):Atelier.muted;font.pixelSize:13;verticalAlignment:Text.AlignVCenter;horizontalAlignment:Text.AlignHCenter}
                background: Rectangle {radius:Atelier.controlRadius;color:tab.selected?Atelier.field:tab.hovered?Atelier.hover:"transparent";border.width:tab.activeFocus?1:0;border.color:Atelier.accent}
            }
        }
        Item {Layout.fillWidth:true}
        Button {
            objectName: "studioActivity"
            visible: root.activityText.length>0
            Layout.maximumWidth: Math.max(100,root.width*.25)
            onClicked: root.navigate(root.activityTab)
            contentItem: Label {text:root.activityText;color:root.recording?Atelier.danger:Atelier.accent;elide:Text.ElideRight;font.pixelSize:11}
            background: Item {}
            DarkToolTip {visible:parent.hovered;text:root.activityText}
        }
        IconAction {objectName:"studioSettings";iconName:"gear";text:qsTr("Settings");enabled:!root.recording;onClicked:root.navigate("settings")}
    }
    Rectangle {anchors.bottom:parent.bottom;width:parent.width;height:1;color:Atelier.border}
}
