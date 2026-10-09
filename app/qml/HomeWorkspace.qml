import "Atelier.js" as Atelier
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Rectangle {
    id:root
    required property AppLibrary library
    property bool recording:false
    property bool deepSkyOpen:false
    property int deepSkyCount:0
    property bool processingOpen:false
    property string processingName:""
    property bool projectBusy:false
    property bool processingBusy:false
    signal navigate(string tab)
    signal chooseProject()
    signal openProject(url source)
    signal openVideo(url source)
    property int recentRefresh:0
    readonly property var projects:{recentRefresh;return library.recentProjects;}
    readonly property var videos:{recentRefresh;return library.recentVideos;}
    function refreshRecents() {library.pruneUnavailableRecents();recentRefresh++;}
    Component.onCompleted:refreshRecents()
    onVisibleChanged:if(visible)refreshRecents()
    Connections {target:Qt.application;function onStateChanged(){if(root.visible && Qt.application.state===Qt.ApplicationActive)root.refreshRecents();}}
    color:Atelier.canvas
    objectName:"studioHome"
    component StartCard:Button {
        id:card
        property string heading
        property string detail
        property string iconName
        Layout.fillWidth:true;Layout.minimumWidth:0
        Accessible.name:heading;Accessible.description:detail
        padding:22
        contentItem:ColumnLayout {
            spacing:12
            Icon {name:card.iconName;color:Atelier.accent;Layout.preferredWidth:26;Layout.preferredHeight:26}
            Label {text:card.heading;color:card.enabled?Atelier.text:Atelier.muted;font.pixelSize:17;wrapMode:Text.Wrap;Layout.fillWidth:true}
            Label {text:card.detail;color:Atelier.secondary;font.pixelSize:13;wrapMode:Text.Wrap;Layout.fillWidth:true}
        }
        background:Rectangle {radius:Atelier.controlRadius;color:card.hovered?Atelier.hover:Atelier.surface;border.color:card.hovered?Atelier.border:Atelier.border}
    }
    ScrollView {
        id:scroll;anchors.fill:parent;anchors.margins:root.width<900?24:36
        contentWidth:availableWidth;clip:true
        ColumnLayout {
            width:scroll.availableWidth;spacing:24
            RowLayout {
                Layout.fillWidth:true;spacing:16
                ColumnLayout {
                    Layout.fillWidth:true;spacing:8
                    Label {text:qsTr("Your next image starts here.");color:Atelier.text;font.pixelSize:root.width<900?24:28;font.weight:Font.Medium;wrapMode:Text.Wrap;Layout.fillWidth:true}
                    Label {text:qsTr("Capture, process or return to a project.");color:Atelier.secondary;wrapMode:Text.Wrap;Layout.fillWidth:true}
                }
                IconButton {objectName:"homeOpenProject";text:qsTr("Open a project");iconName:"folder";enabled:!root.recording && !root.projectBusy;onClicked:root.chooseProject()}
            }
            GridLayout {
                columns:root.width<900?1:3;Layout.fillWidth:true;columnSpacing:14;rowSpacing:12
                StartCard {objectName:"homeCapture";heading:qsTr("Capture the sky");detail:qsTr("Camera · SER video · FITS exposures");iconName:"camera";onClicked:root.navigate("capture")}
                StartCard {objectName:"homeProcessing";heading:qsTr("Process an image");detail:qsTr("Planets, Moon and Sun · Still images");iconName:"processing";enabled:!root.recording;onClicked:root.navigate("processing")}
                StartCard {objectName:"homeDeepSky";heading:qsTr("Explore deep sky");detail:qsTr("Calibration · Stacking · Finishing");iconName:"stars";enabled:!root.recording;onClicked:root.navigate("deepSky")}
            }
            ColumnLayout {
                Layout.fillWidth:true;spacing:0
                Label {text:qsTr("Continue your work");color:Atelier.text;font.pixelSize:17;Layout.bottomMargin:12}
                RowLayout {
                    visible:root.deepSkyOpen;Layout.fillWidth:true;Layout.topMargin:10;Layout.bottomMargin:10
                    ColumnLayout {Layout.fillWidth:true;spacing:3
                        Label {text:qsTr("Current deep-sky project");color:Atelier.text}
                        Label {text:qsTr("%1 images · current session").arg(root.deepSkyCount);color:Atelier.secondary;font.pixelSize:12}
                    }
                    Item {Layout.fillWidth:true}
                    IconButton {objectName:"homeResumeDeepSky";text:qsTr("Resume");enabled:!root.recording;onClicked:root.navigate("deepSky")}
                }
                RowLayout {
                    visible:root.processingOpen;Layout.fillWidth:true;Layout.topMargin:10;Layout.bottomMargin:10
                    ColumnLayout {Layout.fillWidth:true;spacing:3
                        Label {text:root.processingName;color:Atelier.text;elide:Text.ElideMiddle;Layout.fillWidth:true}
                        Label {text:qsTr("Processing · current session");color:Atelier.secondary;font.pixelSize:12}
                    }
                    Item {Layout.fillWidth:true}
                    IconButton {objectName:"homeResumeProcessing";text:qsTr("Resume");enabled:!root.recording;onClicked:root.navigate("processing")}
                }
                Repeater {
                    model:root.projects.slice(0,3)
                    delegate:ColumnLayout {
                        required property var modelData
                        Layout.fillWidth:true;spacing:0
                        Rectangle {Layout.fillWidth:true;Layout.preferredHeight:1;color:Atelier.border}
                        RowLayout {
                            Layout.fillWidth:true;Layout.topMargin:12;Layout.bottomMargin:12
                            ColumnLayout {Layout.fillWidth:true;spacing:3
                                Label {text:modelData.name;color:Atelier.text;elide:Text.ElideMiddle;Layout.fillWidth:true}
                                Label {text:modelData.exists?qsTr("Saved deep-sky project"):qsTr("File unavailable");color:Atelier.secondary;font.pixelSize:12}
                            }
                            IconButton {text:qsTr("Open");enabled:modelData.exists && !root.recording && !root.projectBusy;onClicked:root.openProject(modelData.url);DarkToolTip {visible:parent.hovered;text:modelData.path}}
                            IconButton {objectName:"homeRemoveProject";text:"×";Accessible.name:qsTr("Remove from recent projects");onClicked:root.library.removeRecentProject(modelData.url)}
                        }
                    }
                }
                Repeater {
                    model:root.videos.slice(0,2)
                    delegate:ColumnLayout {
                        required property var modelData
                        Layout.fillWidth:true;spacing:0
                        Rectangle {Layout.fillWidth:true;Layout.preferredHeight:1;color:Atelier.border}
                        RowLayout {
                            Layout.fillWidth:true;Layout.topMargin:12;Layout.bottomMargin:12
                            ColumnLayout {Layout.fillWidth:true;spacing:3
                                Label {text:modelData.name;color:Atelier.text;elide:Text.ElideMiddle;Layout.fillWidth:true}
                                Label {text:modelData.exists?qsTr("Recent SER video"):qsTr("File unavailable");color:Atelier.secondary;font.pixelSize:12}
                            }
                            IconButton {text:qsTr("Open");enabled:modelData.exists && !root.recording && !root.processingBusy;onClicked:root.openVideo(modelData.url)}
                            IconButton {objectName:"homeRemoveVideo";text:"×";Accessible.name:qsTr("Remove from recent videos");onClicked:root.library.removeRecentVideo(modelData.url)}
                        }
                    }
                }
                Label {visible:!root.deepSkyOpen && !root.processingOpen && root.projects.length===0 && root.videos.length===0;text:qsTr("Your recent projects and videos will appear here.");color:Atelier.secondary;wrapMode:Text.Wrap;Layout.fillWidth:true;Layout.topMargin:12}
            }
            Rectangle {Layout.fillWidth:true;Layout.preferredHeight:1;color:Atelier.border}
            RowLayout {
                Layout.fillWidth:true
                ColumnLayout {Layout.fillWidth:true;spacing:4
                    Label {text:qsTr("First session?");color:Atelier.text;font.pixelSize:16}
                    Label {text:qsTr("Learn acquisition, calibration and finishing at your own pace.");color:Atelier.secondary;wrapMode:Text.Wrap;Layout.fillWidth:true}
                }
                IconButton {text:qsTr("Open the guide");iconName:"book";onClicked:root.navigate("guide")}
            }
            Item {Layout.preferredHeight:8}
        }
    }
}
