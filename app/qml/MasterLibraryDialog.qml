import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Dialogs
import "Atelier.js" as Atelier
Popup {
    id:dialog
    objectName:"masterLibraryDialog"
    required property var controller
    required property var library
    property real hostWidth:1100
    property real hostHeight:800
    property bool fileDialogOpen:importFile.visible
    property int mode:0
    readonly property var entries:Array.from(controller.calibrationMasters).filter(e=>
        (!matchingOnly.checked||e.compatibility<2)&&
        (roleFilter.currentIndex===0||e.role===["","Dark","Bias","Flat","DarkFlat"][roleFilter.currentIndex])&&
        (!searchField.text.trim()||[e.name,e.camera,e.group,e.exposure,e.temperature,e.gain].join(" ").toLowerCase().includes(searchField.text.trim().toLowerCase()))
    ).sort((a,b)=>a.compatibility-b.compatibility||(b.masterCount||0)-(a.masterCount||0)||a.name.localeCompare(b.name))
    readonly property var entry:entries[choice.currentIndex]||({})
    parent:Overlay.overlay
    anchors.centerIn:parent
    width:Math.min(mode===1?760:620,hostWidth-32)
    height:Math.min(780,hostHeight-32,panel.implicitHeight+40)
    padding:20;modal:true;focus:true
    closePolicy:controller.busy?Popup.NoAutoClose:Popup.CloseOnEscape
    background:Rectangle {color:Atelier.surface;radius:10;border.color:Atelier.border}
    onOpened:batchPane.refresh()
    Connections {target:dialog.controller;function onChanged(){if(dialog.visible&&!dialog.controller.busy)batchPane.refresh()}}
    component Note:Label {Layout.fillWidth:true;wrapMode:Text.Wrap;color:Atelier.secondary;font.pixelSize:12;textFormat:Text.PlainText}
    ColumnLayout {
        id:panel;anchors.fill:parent;spacing:12
        RowLayout {Layout.fillWidth:true
            Label {text:qsTr("Master library");color:Atelier.text;font.pixelSize:16;font.weight:Font.DemiBold;Layout.fillWidth:true}
            IconButton {text:qsTr("Close");enabled:!dialog.controller.busy;onClicked:dialog.close()}
        }
        RowLayout {Layout.fillWidth:true
            IconButton {text:qsTr("Find a master");Layout.fillWidth:true;primary:dialog.mode===0;onClicked:dialog.mode=0}
            IconButton {text:qsTr("Create masters in batches");Layout.fillWidth:true;primary:dialog.mode===1;onClicked:{batchPane.refresh();dialog.mode=1}}
        }
        Flickable {id:libraryScroll;visible:dialog.mode===0;
            Layout.fillWidth:true;Layout.fillHeight:true;implicitHeight:body.implicitHeight
            contentWidth:width;contentHeight:body.implicitHeight;clip:true;boundsBehavior:Flickable.StopAtBounds
            ColumnLayout {id:body;width:parent.width;spacing:12
                Note {text:qsTr("Reuse calibration masters across sessions. FITS Float32 files preserve linear precision. Select your lights first to check compatibility; dark-flats are checked against selected flats.")}
                RowLayout {Layout.fillWidth:true
                    TextField {id:searchField;objectName:"masterLibrarySearch";Layout.fillWidth:true;placeholderText:qsTr("Search name, camera, filter or settings");color:Atelier.text;placeholderTextColor:Atelier.muted;selectByMouse:true;Accessible.name:qsTr("Search the master library");background:Rectangle {color:Atelier.field;radius:Atelier.controlRadius;border.color:searchField.activeFocus?Atelier.accent:Atelier.border}}
                    HelpIcon {text:qsTr("Search saved names, cameras, filter groups, exposure, temperature or gain. Matching entries come first, then entries needing metadata review. More source frames do not guarantee a better master; inspect its diagnostics and acquisition conditions.")}
                }
                DarkComboBox {id:roleFilter;objectName:"masterLibraryRoleFilter";Layout.fillWidth:true;model:[qsTr("All roles"),qsTr("Dark"),qsTr("Bias"),qsTr("Flat"),qsTr("Dark flat")];explanation:qsTr("Filter the library by calibration role. Flat compatibility is checked against selected lights in its filter group; dark-flats are checked against selected flats.")}
                HelpCheckBox {id:matchingOnly;objectName:"masterLibraryMatchingOnly";Layout.fillWidth:true;text:qsTr("Hide known incompatible masters");explanation:qsTr("Keeps compatible masters and those with missing metadata that require review. No master is applied automatically, and a known mismatch cannot be overridden.")}
                DarkComboBox {id:choice;objectName:"masterLibraryChoice";Layout.fillWidth:true;enabled:count>0&&!dialog.controller.busy;model:Array.from(dialog.entries).map(e=>(e.compatibility===0?"✓ ":"")+e.name+" · "+e.role);displayText:count?currentText:qsTr("No saved masters");onCurrentIndexChanged:reviewed.checked=false;explanation:qsTr("Each master keeps its camera, binning, gain, offset, temperature, exposure, filter and source count when known. SHA256 verifies the saved FITS before use. Missing metadata requires explicit review; a mismatch cannot be overridden.")}
                Note {visible:choice.count>0;text:{const e=dialog.entry;return (e.camera||qsTr("Unknown camera"))+" · "+e.width+" × "+e.height+" · "+(e.bayer==="None"?(e.channels===3?"RVB":qsTr("Monochrome")):e.bayer)+"\n"+qsTr("Exposure %1 s · gain %2 · temperature %3 °C · binning %4 × %5").arg(Number(e.exposure||0).toFixed(3)).arg(e.gain===undefined?"?":e.gain).arg(e.temperature===undefined?"?":e.temperature).arg(e.binX||"?").arg(e.binY||"?")+"\n"+qsTr("Sources %1 · median %2 · MAD noise %3 · rejected samples %4").arg(e.masterCount||"?").arg(Number(e.median||0).toPrecision(5)).arg(Number(e.noiseMAD||0).toPrecision(4)).arg(e.rejectedSamples||0);}}
                Note {visible:choice.count>0&&Boolean(dialog.entry.metadataReviewed);text:qsTr("This master was created after an explicit review of missing metadata. Compatibility must still be checked for this session.")}
                Note {objectName:"masterCompatibility";visible:choice.count>0;color:dialog.entry.compatibility===2?Atelier.danger:Atelier.secondary;text:dialog.entry.compatibility===0?qsTr("Compatible with the selected frames"):dialog.entry.compatibility===1?qsTr("Review missing metadata: %1").arg(dialog.entry.missing||""):qsTr("Incompatible: %1").arg(dialog.entry.mismatch||"")}
                HelpCheckBox {id:reviewed;objectName:"masterMetadataReviewed";visible:choice.count>0&&dialog.entry.compatibility===1;Layout.fillWidth:true;text:qsTr("I checked the missing equipment information");explanation:qsTr("Only confirm after checking the source files and observing log. Unknown metadata cannot establish compatibility automatically. Exposure scaling of darks is never applied.")}
                RowLayout {Layout.fillWidth:true
                    IconButton {objectName:"useLibraryMaster";text:qsTr("Use this master");primary:true;Layout.fillWidth:true;enabled:choice.count>0&&!dialog.controller.busy&&(dialog.entry.compatibility===0||(dialog.entry.compatibility===1&&reviewed.checked));onClicked:dialog.controller.useCalibrationMaster(dialog.entry.id,reviewed.checked)}
                    IconAction {iconName:"folder";text:qsTr("Show master in Finder");enabled:choice.count>0;onClicked:dialog.library.revealImage("file://"+dialog.entry.path)}
                    IconAction {iconName:"trash";text:qsTr("Remove from library");enabled:choice.count>0&&!dialog.controller.busy;onClicked:removeConfirm.open()}
                }
                Rectangle {Layout.fillWidth:true;Layout.preferredHeight:1;color:Atelier.border}
                Label {text:qsTr("Create or import a master");color:Atelier.text;font.weight:Font.Medium;Layout.fillWidth:true}
                DarkComboBox {id:role;objectName:"masterRole";Layout.fillWidth:true;model:[qsTr("Dark"),qsTr("Bias"),qsTr("Flat"),qsTr("Dark flat")];explanation:qsTr("Dark includes the bias and matches light exposure. Bias corrects the electronic pedestal. Dark-flat matches flat exposure. Flat must be bias/dark-flat corrected and normalized. Masters and their raw sources must not be mixed.")}
                RowLayout {Layout.fillWidth:true
                    TextField {id:masterName;objectName:"newMasterName";Layout.fillWidth:true;placeholderText:qsTr("Master name · camera, exposure, temperature…");maximumLength:120;selectByMouse:true;enabled:!dialog.controller.busy;color:Atelier.text;placeholderTextColor:Atelier.muted;Accessible.name:qsTr("Master name");background:Rectangle {color:Atelier.field;radius:4;border.color:masterName.activeFocus?Atelier.accent:Atelier.border}}
                    HelpIcon {text:qsTr("Give this master a recognizable name. Creation averages selected raw frames with median/MAD rejection from three frames onward. It uses temporary disk files to limit memory. Select frames of the chosen role and one filter/configuration. One source is allowed but gives no multi-frame noise reduction. Nonfinite samples and unusable flats are rejected.")}
                }
                Note {text:qsTr("To create a flat, also select its matching dark-flat or bias. The library stores the corrected normalized flat, so integration will not subtract its pedestal again.")}
                IconButton {objectName:"createLibraryMaster";text:qsTr("Create from selected source frames");Layout.fillWidth:true;enabled:!dialog.controller.busy&&masterName.text.trim().length>0;onClicked:dialog.controller.createCalibrationMaster(["Dark","Bias","Flat","DarkFlat"][role.currentIndex],masterName.text)}
                HelpCheckBox {id:correctedFlat;visible:role.currentIndex===2;Layout.fillWidth:true;text:qsTr("Imported flat is already bias/dark-flat corrected");explanation:qsTr("Required for external flats without AstroCapture provenance. Import normalizes illumination once; it cannot infer whether a pedestal has already been removed. Raw uncorrected flats should use Create from selected source frames.")}
                IconButton {text:qsTr("Import existing FITS master");Layout.fillWidth:true;enabled:!dialog.controller.busy&&masterName.text.trim().length>0&&(role.currentIndex!==2||correctedFlat.checked);onClicked:importFile.open()}
            }
        }
        Flickable {objectName:"batchMasterScroll";visible:dialog.mode===1;Layout.fillWidth:true;Layout.fillHeight:true;implicitHeight:batchPane.implicitHeight;contentWidth:width;contentHeight:batchPane.implicitHeight;clip:true;boundsBehavior:Flickable.StopAtBounds
            BatchMasterPane {id:batchPane;objectName:"batchMasterPane";width:parent.width;controller:dialog.controller}
        }
        IconButton {objectName:"createBatchMasters";visible:dialog.mode===1;busy:dialog.controller.busy;text:qsTr("Create %1 masters").arg(batchPane.selectedCount);Layout.fillWidth:true;primary:true;enabled:batchPane.canCreate;onClicked:batchPane.start()}
        IconButton {visible:dialog.mode===1;text:qsTr("Temporary folder");enabled:!dialog.controller.busy;onClicked:temporaryFolder.open()}
        Rectangle {Layout.fillWidth:true;Layout.preferredHeight:1;color:Atelier.border}
        Note {objectName:"masterLibraryStatus";text:dialog.controller.status}
        IconButton {text:qsTr("Cancel calculation");visible:dialog.controller.busy;onClicked:dialog.controller.cancel()}
    }
    FolderDialog {id:temporaryFolder;title:qsTr("Choose a temporary folder");currentFolder:dialog.controller.integrationTemporaryDirectoryUrl;onAccepted:{dialog.controller.chooseIntegrationTemporaryDirectory(selectedFolder);batchPane.refresh()}}
    FileDialog {id:importFile;title:qsTr("Import a calibration master");nameFilters:[qsTr("FITS master (*.fits *.fit *.fts)")];onAccepted:dialog.controller.importCalibrationMaster(selectedFile,["Dark","Bias","Flat","DarkFlat"][role.currentIndex],masterName.text,correctedFlat.checked)}
    Popup {
        id:removeConfirm;parent:Overlay.overlay;anchors.centerIn:parent;modal:true;focus:true
        width:Math.min(460,dialog.hostWidth-48);height:removePanel.implicitHeight+40;padding:20
        background:Rectangle {color:Atelier.surface;radius:Atelier.dialogRadius;border.color:Atelier.border}
        ColumnLayout {id:removePanel;anchors.fill:parent;spacing:16
            Label {Layout.fillWidth:true;text:qsTr("Remove this library entry?");wrapMode:Text.Wrap;color:Atelier.text;font.pixelSize:16;font.bold:true}
            Note {text:qsTr("Remove %1 from the list? Its FITS file stays on disk so existing projects remain usable.").arg(dialog.entry.name||"")}
            RowLayout {Layout.fillWidth:true
                IconButton {text:qsTr("Close");Layout.fillWidth:true;onClicked:removeConfirm.close()}
                IconButton {text:qsTr("Remove from library");Layout.fillWidth:true;enabled:!dialog.controller.busy;onClicked:{dialog.controller.removeCalibrationMasterEntry(dialog.entry.id);removeConfirm.close()}}
            }
        }
    }
}
