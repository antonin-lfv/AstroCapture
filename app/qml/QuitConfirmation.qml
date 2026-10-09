import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

DarkDialog {
    id: dialog
    objectName: "quitConfirmation"
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(520, parent ? parent.width - 32 : 520)
    modal: true
    focus: true
    title: qsTr("Save before quitting?")
    closePolicy: Popup.CloseOnEscape
    property bool hasProject: false
    property bool hasProcessing: false
    property bool busy: false
    property bool recording: false
    property var exportTargets: []
    property string saveStatus: ""
    property string preferredTarget: ""
    signal saveProjectRequested()
    signal saveSettingsRequested()
    signal exportRequested(string target)
    signal quitRequested()
    onOpened: {
        const index=exportTargets.findIndex(target=>target.id===preferredTarget);
        exportChoice.currentIndex=index>=0?index:0;
    }
    ColumnLayout {
        width: parent.width
        spacing: 14
        Label {
            text: qsTr("Your current work will close. Save the project, the finished image or your settings before leaving.")
            color: "#e8edf5"; wrapMode: Text.Wrap; Layout.fillWidth: true
        }
        Label {
            visible: dialog.hasProject
            text: qsTr("The deep-sky project keeps source references, stacking options, finishing settings and the caption. A TIFF keeps the finished image.")
            color: "#a8b5c6"; wrapMode: Text.Wrap; Layout.fillWidth: true
        }
        Label {
            visible: dialog.recording || dialog.busy
            text: dialog.recording ? qsTr("Recording is in progress. Stop it before quitting.") : qsTr("A task is in progress. Wait for it to finish, or return to the workspace to cancel it.")
            color: "#edb06c"; wrapMode: Text.Wrap; Layout.fillWidth: true
        }
        Label {text:dialog.saveStatus;visible:text.length>0;color:"#a8b5c6";wrapMode:Text.Wrap;Layout.fillWidth:true}
        IconButton {
            objectName: "quitSaveProject";text:qsTr("Save project and settings")
            visible:dialog.hasProject;enabled:!dialog.busy && !dialog.recording;Layout.fillWidth:true
            onClicked:dialog.saveProjectRequested()
        }
        DarkComboBox {
            id:exportChoice;objectName:"quitExportTarget"
            visible:dialog.exportTargets.length>1;Layout.fillWidth:true
            model:dialog.exportTargets.map(target=>target.name)
        }
        IconButton {
            objectName:"quitExportTiff";text:qsTr("Save finished TIFF")
            visible:dialog.exportTargets.length>0;enabled:!dialog.busy && !dialog.recording && dialog.exportTargets.length>0 && dialog.exportTargets[Math.max(0,exportChoice.currentIndex)].ready;Layout.fillWidth:true
            onClicked:dialog.exportRequested(dialog.exportTargets[Math.max(0,exportChoice.currentIndex)].id)
        }
        IconButton {
            objectName:"quitSaveSettings";text:qsTr("Save processing settings")
            visible:dialog.hasProcessing;enabled:!dialog.busy && !dialog.recording;Layout.fillWidth:true
            onClicked:dialog.saveSettingsRequested()
        }
        RowLayout {
            Layout.fillWidth:true
            IconButton {objectName:"quitCancel";text:qsTr("Keep working");Layout.fillWidth:true;onClicked:dialog.close()}
            IconButton {
                objectName:"quitDiscard";text:qsTr("Quit anyway");Layout.fillWidth:true
                enabled:!dialog.busy && !dialog.recording;active:true;activeBorderColor:"#ba5158"
                onClicked:dialog.quitRequested()
            }
        }
    }
}
