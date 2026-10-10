import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Dialogs
import "Atelier.js" as Atelier

Popup {
    id: dialog
    objectName: "equipmentProfilesDialog"
    required property var controller
    required property var library
    property real hostWidth: 1100
    property real hostHeight: 800
    property var currentResponse: ({})
    property string message: ""
    property bool libraryFilesOpen: importLibrary.visible || exportLibrary.visible || importResponse.visible
    signal responseSelected()
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(600,hostWidth-32)
    height: Math.min(hostHeight-32,760,panel.implicitHeight+40)
    padding: 20
    modal: true
    focus: true
    closePolicy: controller.busy ? Popup.NoAutoClose : Popup.CloseOnEscape
    background: Rectangle {color:Atelier.surface;radius:10;border.color:Atelier.border}
    function refreshCurrent() {
        currentResponse=controller.colorResponse();
        profileName.text=currentResponse.name||"";camera.text=currentResponse.camera||"";
        filters.text=currentResponse.filters||"";source.text=currentResponse.source||"";notes.text=currentResponse.notes||"";
        for(const field of [profileName,camera,filters,source,notes])field.showStart();
    }
    function metadata() {return {camera:camera.text,filters:filters.text,source:source.text,notes:notes.text};}
    function saveProfile(replace) {
        if(library.saveEquipmentProfile(profileName.text,currentResponse,metadata(),replace)) {
            const name=profileName.text.trim();controller.useEquipmentProfile(library.equipmentProfile(name));
            profiles.currentIndex=Array.from(library.equipmentProfileNames).indexOf(name);
            refreshCurrent();responseSelected();
        }
        message=library.status;
    }
    onOpened: {refreshCurrent();message="";}
    component Note: Label {Layout.fillWidth:true;wrapMode:Text.Wrap;color:Atelier.secondary;font.pixelSize:12;textFormat:Text.PlainText}
    component Field: RowLayout {
        id:row
        required property string label
        required property string help
        property alias text:field.text
        property int limit:512
        function showStart() {field.cursorPosition=0;}
        Layout.fillWidth:true
        Label {text:row.label;color:Atelier.text;Layout.fillWidth:true;wrapMode:Text.Wrap}
        HelpIcon {text:row.help}
        TextField {id:field;objectName:row.objectName+"Field";Layout.preferredWidth:280;Layout.maximumWidth:dialog.width*.52;selectByMouse:true;maximumLength:row.limit;enabled:!dialog.controller.busy;color:Atelier.text;font.pixelSize:13;Accessible.name:row.label;background:Rectangle {color:Atelier.field;radius:4;border.color:field.activeFocus?Atelier.accent:Atelier.border}}
    }
    ColumnLayout {
        id:panel
        anchors.fill:parent
        spacing:12
        RowLayout {Layout.fillWidth:true
            Label {text:qsTr("Equipment profiles");color:Atelier.text;font.pixelSize:16;font.weight:Font.DemiBold;Layout.fillWidth:true}
            IconButton {text:qsTr("Close");onClicked:dialog.close()}
        }
        Flickable {
            Layout.fillWidth:true;Layout.fillHeight:true;implicitHeight:body.implicitHeight
            contentHeight:body.implicitHeight;contentWidth:width;clip:true;boundsBehavior:Flickable.StopAtBounds
            ColumnLayout {
                id:body;width:parent.width;spacing:12
                Note {text:qsTr("Reuse your imported measured RGB responses. Each profile describes one complete camera and filter combination. Selecting a profile does not change an existing calibration.")}
                RowLayout {Layout.fillWidth:true
                    DarkComboBox {id:profiles;objectName:"equipmentProfileChoice";Layout.fillWidth:true;model:dialog.library.equipmentProfileNames;enabled:count>0&&!dialog.controller.busy;displayText:count?currentText:qsTr("No saved profiles");explanation:qsTr("Choose a saved camera/filter combination, then use it. The response data is copied into the project; removing a library entry will not alter that project.")}
                    IconButton {objectName:"equipmentProfileUse";text:qsTr("Use profile");enabled:profiles.count>0&&!dialog.controller.busy;onClicked:{if(dialog.controller.useEquipmentProfile(dialog.library.equipmentProfile(profiles.currentText))){dialog.refreshCurrent();dialog.responseSelected();}dialog.message=dialog.controller.status;}}
                }
                RowLayout {Layout.fillWidth:true
                    IconButton {text:qsTr("Import library");Layout.fillWidth:true;onClicked:importLibrary.open()}
                    IconButton {text:qsTr("Export library");Layout.fillWidth:true;enabled:profiles.count>0;onClicked:exportLibrary.open()}
                    IconAction {text:qsTr("Remove selected profile");iconName:"trash";enabled:profiles.count>0;onClicked:removeProfile.open()}
                }
                Rectangle {Layout.fillWidth:true;Layout.preferredHeight:1;color:Atelier.border}
                RowLayout {Layout.fillWidth:true
                    Label {text:qsTr("Save the currently loaded response");color:Atelier.text;font.weight:Font.Medium;Layout.fillWidth:true;wrapMode:Text.Wrap}
                    IconButton {text:qsTr("Import RGB CSV");onClicked:importResponse.open()}
                }
                Note {objectName:"equipmentResponseSummary";text:dialog.currentResponse.response?qsTr("%1 · %2 wavelength samples").arg(dialog.currentResponse.fileName).arg(dialog.currentResponse.response.length):qsTr("Import a complete RGB photon response first: sensor QE × RGB filters × additional transmission. No camera response is generated automatically.")}
                Field {id:profileName;objectName:"equipmentProfileName";label:qsTr("Profile name");limit:80;help:qsTr("Use a distinct name for each optical combination, for example camera + RGB filters + light-pollution filter. Saving requires a name; replacing an existing profile is a separate explicit action.")}
                Field {id:camera;objectName:"equipmentProfileCamera";label:qsTr("Camera / sensor");help:qsTr("Record the sensor or camera whose quantum efficiency is included in the imported response. This description does not change the curve.")}
                Field {id:filters;objectName:"equipmentProfileFilters";label:qsTr("Filters");help:qsTr("List the RGB filters and any additional optical filter already included in this effective response. A different filter combination needs a different measured response.")}
                Field {id:source;objectName:"equipmentProfileSource";label:qsTr("Measurement source");help:qsTr("Record the measurement, manufacturer document or URL from which the response was obtained. This is provenance text, not a curve download. Ensure all three channels use the same relative normalization.")}
                Field {id:notes;objectName:"equipmentProfileNotes";label:qsTr("Notes");help:qsTr("Optional measurement date, instrument configuration or limitations. Metadata accompanies the curve but cannot establish that it matches the actual equipment.")}
                Note {visible:!!dialog.currentResponse.responseHash;text:qsTr("Source SHA256: %1").arg(dialog.currentResponse.responseHash||"");font.pixelSize:11;wrapMode:Text.WrapAnywhere}
            }
        }
        Rectangle {Layout.fillWidth:true;Layout.preferredHeight:1;color:Atelier.border}
        Note {objectName:"equipmentProfilesStatus";visible:dialog.message.length>0;text:dialog.message}
        RowLayout {Layout.fillWidth:true
            IconButton {objectName:"equipmentProfileSave";text:qsTr("Save new profile");Layout.fillWidth:true;enabled:!!dialog.currentResponse.response&&profileName.text.trim().length>0&&!dialog.controller.busy;onClicked:dialog.saveProfile(false)}
            IconButton {objectName:"equipmentProfileReplace";text:qsTr("Replace this profile");enabled:!!dialog.currentResponse.response&&Array.from(dialog.library.equipmentProfileNames).some(name=>name.toLowerCase()===profileName.text.trim().toLowerCase())&&!dialog.controller.busy;onClicked:dialog.saveProfile(true)}
        }
    }
    FileDialog {id:importResponse;title:qsTr("Measured equipment response");nameFilters:[qsTr("RGB response (*.csv)")];onAccepted:{if(dialog.controller.importColorResponse(selectedFile)){dialog.refreshCurrent();dialog.responseSelected();}dialog.message=dialog.controller.status;}}
    FileDialog {id:importLibrary;title:qsTr("Import equipment library");nameFilters:[qsTr("Equipment library (*.json)")];onAccepted:{dialog.library.importEquipmentProfiles(selectedFile);dialog.message=dialog.library.status;}}
    FileDialog {id:exportLibrary;title:qsTr("Export equipment library");fileMode:FileDialog.SaveFile;defaultSuffix:"json";nameFilters:[qsTr("Equipment library (*.json)")];onAccepted:{dialog.library.exportEquipmentProfiles(selectedFile);dialog.message=dialog.library.status;}}
    DarkDialog {id:removeProfile;anchors.centerIn:parent;parent:Overlay.overlay;modal:true;title:qsTr("Remove this equipment profile?");standardButtons:Dialog.Cancel|Dialog.Ok;width:Math.min(460,dialog.hostWidth-48)
        Label {width:parent.width;text:qsTr("Remove %1 from the library? Existing projects and calibrations keep their own response data.").arg(profiles.currentText);wrapMode:Text.Wrap;color:Atelier.text}
        onAccepted:{dialog.library.removeEquipmentProfile(profiles.currentText);dialog.message=dialog.library.status;}
    }
}
