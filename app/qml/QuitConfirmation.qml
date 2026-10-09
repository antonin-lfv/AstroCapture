import "Atelier.js" as Atelier
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
    property bool canSaveSession:false
    property bool sessionSaving:false
    property var exportTargets: []
    property string saveStatus: ""
    property string preferredTarget: ""
    signal saveProjectRequested()
    signal saveSettingsRequested()
    signal exportRequested(string target)
    signal quitRequested()
    signal saveSessionRequested()
    onOpened: {
        const index=exportTargets.findIndex(target=>target.id===preferredTarget);
        exportChoice.currentIndex=index>=0?index:0;
    }
    ColumnLayout {
        width: parent.width
        spacing: 14
        Label {
            text: qsTr("Save the session to resume your work, or save the project, finished image or settings before leaving.")
            color: Atelier.text; wrapMode: Text.Wrap; Layout.fillWidth: true
        }
        Label {
            visible: dialog.hasProject
            text: qsTr("The deep-sky project keeps source references, stacking options, finishing settings and the caption. A TIFF keeps the finished image.")
            color: Atelier.secondary; wrapMode: Text.Wrap; Layout.fillWidth: true
        }
        Label {
            visible: dialog.recording || dialog.busy
            text: dialog.recording ? qsTr("Recording is in progress. Stop it before quitting.") : qsTr("A task is in progress. Wait for it to finish, or return to the workspace to cancel it.")
            color: "#edb06c"; wrapMode: Text.Wrap; Layout.fillWidth: true
        }
        Label {visible:dialog.sessionSaving;text:qsTr("The automatic save is in progress. Saving the session before quitting waits for the latest changes.");color:Atelier.secondary;wrapMode:Text.Wrap;Layout.fillWidth:true}
        Label {text:dialog.saveStatus;visible:text.length>0;color:Atelier.secondary;wrapMode:Text.Wrap;Layout.fillWidth:true}
        IconButton {
            objectName:"quitSaveSession";text:qsTr("Save session and quit");primary:true
            visible:dialog.canSaveSession;enabled:!dialog.busy && !dialog.recording;Layout.fillWidth:true
            onClicked:dialog.saveSessionRequested()
        }
        IconButton {
            objectName: "quitSaveProject";text:qsTr("Save project and settings")
            visible:dialog.hasProject;enabled:!dialog.busy && !dialog.recording;Layout.fillWidth:true
            onClicked:dialog.saveProjectRequested()
        }
        DarkComboBox {explanation: qsTr("Chooses which workspace result to save as TIFF before quitting. A TIFF preserves the finished image, not the source files, processing recipe or session. Save the project or session to continue editing later.");
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
