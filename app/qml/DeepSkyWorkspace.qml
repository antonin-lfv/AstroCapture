import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Dialogs

Control {
    id: root
    required property DeepSkyController controller
    required property AppLibrary library
    signal editResult()
    property bool canEditResult: true
    property bool shortcutsAllowed: true
    property string editError: ""
    property bool imageFocus: false
    property int finishingTool:0
    property int editingOperationIndex:-1
    property var operationBackup:null
    property var operations:[]
    property var backgroundPoints:[]
    property bool editingBackgroundSamples:false
    property bool frameListOpen:true
    property var rawViewState: null
    function compareRaw(enabled) {
        if(enabled) {
            if(controller.busy || !controller.hasRawComparison || !controller.hasResult)return;
            rawViewState={fitMode:viewport.fitMode,zoom:viewport.zoom,panX:viewport.panX,panY:viewport.panY};
            controller.showRawComparison(true);
        } else {
            controller.showRawComparison(false);
            if(rawViewState) {
                viewport.fitMode=rawViewState.fitMode;viewport.zoom=rawViewState.zoom;
                viewport.panX=rawViewState.panX;viewport.panY=rawViewState.panY;
                viewport.constrainPan();rawViewState=null;
            }
        }
    }
    property alias finishEdits: edits
    DeepSkyEdits {id:edits;objectName:"deepSkyEdits";onValuesChanged:root.restoreFinish(values)}
    property string importRole: "Auto"
    readonly property var roles:["Auto","Light","Dark","Flat","Bias","DarkFlat","BadPixels"]
    readonly property var roleNames:[qsTr("Detect image type"),qsTr("Lights"),qsTr("Darks"),qsTr("Flats"),qsTr("Bias"),qsTr("Dark flats"),qsTr("Bad pixel map")]
    property string frameFilter:"All"
    readonly property var visibleFrames:controller.frames.map((row,index)=>Object.assign({sourceIndex:index},row)).filter(row=>frameFilter==="All" || row.role===frameFilter)
    function roleLabel(role) {const index=roles.indexOf(role);return index>=0?roleNames[index]:role;}
    function selectedRole() {return roles[rolePicker.currentIndex];}
    property bool linearExport: true
    property int workflowStep: 0
    onWorkflowStepChanged:{frameListOpen=workflowStep!==6;if(settingsScroll.contentItem)settingsScroll.contentItem.contentY=0}
    property bool updatingControls: false
    property var pendingProjectFinish: null
    readonly property bool finishDirty: controller.hasResult && (editError.length>0 || Object.keys(edits.values).some(key=>JSON.stringify(edits.values[key])!==JSON.stringify(controller.finishSettings[key])))
    readonly property bool finishEditable: controller.hasResult && (!controller.busy || controller.adjusting)
    function restoreFinish(finish) {
        const previous=updatingControls;updatingControls=true;
        operations=finish.operations||[];scnr.checked=finish.scnr||false;scnrMethod.currentIndex=finish.scnrMethod||0;scnrAmount.value=finish.scnrAmount===undefined?1:finish.scnrAmount;scnrPreserve.checked=finish.scnrPreserve===undefined?true:finish.scnrPreserve;
        backgroundPoints=finish.backgroundPoints||[];background.checked=finish.background;stretch.checked=finish.stretch;
        backgroundDegree.currentIndex=(finish.backgroundDegree||1)-1;backgroundSamples.value=finish.backgroundSamples||20;backgroundTolerance.value=finish.backgroundTolerance===undefined?1:finish.backgroundTolerance;
        rotation.currentIndex=finish.rotation||0;mirrorHorizontal.checked=finish.mirrorHorizontal||false;mirrorVertical.checked=finish.mirrorVertical||false;
        black.text=String(finish.black);white.text=String(finish.white);
        midtones.value=finish.midtones;redGain.value=finish.red;greenGain.value=finish.green;blueGain.value=finish.blue;
        asinhLuminance.currentIndex=finish.asinhLuminance||0;denoise.value=finish.denoise;asinh.checked=finish.asinh;asinhStrength.value=finish.asinhStrength;saturation.value=finish.saturation;
        updatingControls=previous;
    }
    function previewAdjustments() {
        if(!finishEditable || updatingControls)return false;
        if(!edits.applyValues(options())){editError=qsTr("Check the levels: white must exceed black and all values must be valid.");return false;}
        editError="";
        if(livePreview.checked)adjustmentTimer.restart();
        return true;
    }
    function applyStoredFinish() {adjustmentTimer.stop();editError="";controller.apply(edits.values);}
    function setFinish(values) {edits.endEdit();if(edits.applyValues(values))applyStoredFinish();}
    function commitTransformation() {
        if(!finishEditable || !previewAdjustments())return;
        const settings=Object.assign({},options());delete settings.operations;
        const names=[qsTr("Stretch"),qsTr("Background"),qsTr("Color and noise"),qsTr("Orientation")];
        const operation={name:names[root.finishingTool],enabled:true,settings:settings};
        let operations;
        if(root.editingOperationIndex>=0 && root.operationBackup){operations=Array.from(root.operationBackup.operations);operation.name=operations[root.editingOperationIndex].name;operations[root.editingOperationIndex]=operation;}
        else operations=root.operations.concat([operation]);
        root.editingOperationIndex=-1;root.operationBackup=null;
        // Commit the complete draft; subsequent controls act on its output.
        root.setFinish({operations:operations,background:false,backgroundDegree:1,backgroundSamples:20,backgroundTolerance:1,backgroundPoints:[],stretch:false,black:0,white:1,midtones:.15,red:1,green:1,blue:1,denoise:0,asinh:false,asinhStrength:20,saturation:1,rotation:0,mirrorHorizontal:false,mirrorVertical:false,scnr:false,scnrMethod:0,scnrAmount:1,scnrPreserve:true});
    }
    function editOperation(index) {
        if(!finishEditable || editingOperationIndex>=0)return;
        operationBackup=Object.assign({},options());editingOperationIndex=index;
        const parameters=Object.assign({},operations[index].settings,{operations:operations.slice(0,index)});
        root.setFinish(parameters);
    }
    function cancelOperationEdit() {if(!operationBackup)return;const backup=operationBackup;editingOperationIndex=-1;operationBackup=null;root.setFinish(backup);}
    function moveOperation(index,offset) {
        const operations=Array.from(root.operations),target=index+offset;if(target<0||target>=operations.length)return;
        [operations[index],operations[target]]=[operations[target],operations[index]];
        root.setFinish(Object.assign({},options(),{operations:operations}));
    }
    function changeOperation(index,remove) {
        const operations=root.operations.map(op=>Object.assign({},op));
        if(remove)operations.splice(index,1);else operations[index].enabled=!operations[index].enabled;
        root.setFinish(Object.assign({},options(),{operations:operations}));
    }
    function undoFinish() {adjustmentTimer.stop();edits.undo();applyStoredFinish();}
    function redoFinish() {adjustmentTimer.stop();edits.redo();applyStoredFinish();}
    function switchVersion(version) {adjustmentTimer.stop();edits.selectVersion(version);applyStoredFinish();}
    Shortcut {sequences:[StandardKey.Undo];enabled:root.visible && root.shortcutsAllowed && !root.dialogOpen && root.finishEditable && edits.canUndo;onActivated:root.undoFinish()}
    Shortcut {sequences:[StandardKey.Redo];enabled:root.visible && root.shortcutsAllowed && !root.dialogOpen && root.finishEditable && edits.canRedo;onActivated:root.redoFinish()}
    Timer {id:adjustmentTimer;interval:200;onTriggered:controller.apply(root.options())}
    readonly property bool dialogOpen: inputDialog.visible || folderDialog.visible || exportDialog.visible || projectOpen.visible || projectSave.visible || presetDialog.visible || projectMenu.visible
    background: Rectangle {color:"#0b0d10"}
    palette.window: "#0b0d10"
    palette.windowText: "#e8edf5"
    palette.base: "#1c2026"
    palette.text: "#e8edf5"
    palette.button: "#1c2026"
    palette.buttonText: "#e8edf5"
    palette.highlight: "#6ca5f8"
    palette.highlightedText: "#0b0d10"
    palette.mid: "#3a4048"
    palette.light: "#3a4048"
    palette.dark: "#12161c"
    function chooseProject() {if(!controller.busy)projectOpen.open();}
    function importFiles(role) { importRole=role; inputDialog.open(); }
    function saveOptions() {
        if(root.editingOperationIndex>=0){editError=qsTr("Apply or cancel the transformation edit before saving the project.");return false;}
        if(!edits.applyValues(options())){editError=qsTr("Check the levels: white must exceed black and all values must be valid.");return false;}
        edits.endEdit();
        controller.projectOptions={calibrated:calibrated.checked,normalize:normalize.checked,badPixels:badPixels.checked,sequenceBackground:sequenceBackground.checked,sigma:sigma.value,method:integrationMethod.currentIndex,finish:edits.values,versions:edits.snapshot()};return true;
    }
    function options() {return {operations:root.operations,scnr:scnr.checked,scnrMethod:scnrMethod.currentIndex,scnrAmount:scnrAmount.value,scnrPreserve:scnrPreserve.checked,backgroundPoints:root.backgroundPoints,backgroundDegree:backgroundDegree.currentIndex+1,backgroundSamples:backgroundSamples.value,backgroundTolerance:backgroundTolerance.value,rotation:rotation.currentIndex,mirrorHorizontal:mirrorHorizontal.checked,mirrorVertical:mirrorVertical.checked,background: background.checked, stretch: stretch.checked, black:Number(black.text),white:Number(white.text),midtones:midtones.value,red:redGain.value,green:greenGain.value,blue:blueGain.value,denoise:denoise.value,asinh:asinh.checked,asinhStrength:asinhStrength.value,asinhLuminance:asinhLuminance.currentIndex,saturation:saturation.value};}
    Connections {
        target: root.controller
        function onOperationFinished(success, message) {
            if(controller.busy)return;
            completionToast.success=success;
            completionToast.message=success?qsTr("Operation complete"):message;
            completionToast.open();toastTimer.restart();
        }
        function onChanged() {
            if(controller.busy)completionToast.close();
            if(!controller.hasResult && root.workflowStep===6)root.workflowStep=0;
            if (!controller.busy && controller.hasResult && root.pendingProjectFinish) {
                const finish = root.pendingProjectFinish;
                root.pendingProjectFinish = null;
                controller.apply(finish);
            }
            if(!controller.busy && controller.rawComparisonName){const index=rawComparisonChoice.lightRows.findIndex(row=>row.name===controller.rawComparisonName);if(index>=0)rawComparisonChoice.currentIndex=index;}}
        function onProjectOptionsChanged() {
            root.updatingControls=true;
            const settings=controller.projectOptions;
            if(settings.calibrated!==undefined)calibrated.checked=settings.calibrated;
            if(settings.normalize!==undefined)normalize.checked=settings.normalize;
            if(settings.badPixels!==undefined)badPixels.checked=settings.badPixels;
            sequenceBackground.checked=settings.sequenceBackground||false;
            if(settings.sigma!==undefined)sigma.value=settings.sigma;
            if(settings.method!==undefined)integrationMethod.currentIndex=settings.method;
            const finish=settings.finish;
            if(!controller.hasResult) {
                adjustmentTimer.stop();root.editError="";
                const initial={background:false,stretch:true,black:0,white:1,midtones:.15,red:1,green:1,blue:1,denoise:0,asinh:false,asinhStrength:20,saturation:1};
                edits.resetSession(Object.assign(initial,finish||{}));
                if(settings.versions)edits.restoreSession(settings.versions);
            }
            else if(finish) {edits.endEdit();edits.applyValues(Object.assign({},edits.values,finish));}
            root.pendingProjectFinish = !controller.hasResult && finish ? edits.values : null;
            root.updatingControls=false;
        }
        function onResultChanged() {
            root.editingOperationIndex=-1;root.operationBackup=null;
            if(controller.hasResult)root.workflowStep=6;
            if(controller.hasResult && !root.pendingProjectFinish && controller.suggestedStretch.black !== undefined) {
                root.updatingControls=true;
                const suggested=controller.suggestedStretch;
                edits.resetSession({background:false,stretch:true,black:suggested.black,white:suggested.white,midtones:suggested.midtones,red:suggested.red,green:suggested.green,blue:suggested.blue,denoise:0,asinh:false,asinhStrength:20,saturation:1});
                root.editError="";
                root.updatingControls=false;
            }
        }
    }
    component SectionTitle: RowLayout {
        property string label
        property string explanation:""
        id:heading
        Layout.fillWidth:true;Layout.minimumWidth:0;spacing:8
        Label {text:heading.label;color:"#e8edf5";font.pixelSize:14;font.bold:true;wrapMode:Text.Wrap}
        Rectangle {Layout.fillWidth:true;Layout.preferredHeight:1;color:"#344150"}
        HoverHandler {id:headingHover}
        Accessible.description: explanation
        DarkToolTip {parent:heading;text:heading.explanation;visible:headingHover.hovered && text.length>0}
    }
    component ParameterLabel: RowLayout {
        property string label
        property string explanation
        id:parameter
        Layout.fillWidth:true;Layout.minimumWidth:0;spacing:4
        Label {text:parameter.label;color:"#b5c2d2";Layout.fillWidth:true;wrapMode:Text.Wrap}
        HoverHandler {id:parameterHover}
        Accessible.description: explanation
        DarkToolTip {parent:parameter;text:parameter.explanation;visible:parameterHover.hovered && text.length>0}
    }
    ColumnLayout {
        id:workspaceHeader;anchors.top:parent.top;anchors.left:parent.left;anchors.right:parent.right;anchors.margins:12;spacing:8
        RowLayout {
            Layout.fillWidth:true
            Label {text:qsTr("Deep sky");color:"#eef3fa";font.pixelSize:22;font.bold:true;Layout.fillWidth:true}
            Label {text:qsTr("%1 images").arg(controller.frames.length);color:"#9daec3"}
            ToolButton {
                id:projectGear;objectName:"deepSkyProjectMenu";implicitWidth:40;implicitHeight:40;padding:6
                Accessible.name:qsTr("Project options")
                contentItem:Icon {name:"gear";color:projectGear.hovered?"#6ca5f8":"#eef3fa"}
                background:Item {}
                onClicked:projectMenu.popup()
                DarkToolTip {text:qsTr("Project options");visible:projectGear.hovered}
            }
        }
        Rectangle {Layout.fillWidth:true;Layout.preferredHeight:1;color:"#344150"}
    }
    Menu {
        id:projectMenu
        MenuItem {text:qsTr("Open project…");enabled:!controller.busy;onTriggered:projectOpen.open()}
        MenuItem {text:qsTr("Save project…");enabled:!controller.busy && controller.frames.length>0;onTriggered:if(root.saveOptions())projectSave.open();}
        MenuSeparator {}
        MenuItem {text:qsTr("Clear project");enabled:!controller.busy && controller.frames.length>0;onTriggered:clearConfirm.open()}
    }
    DropArea {
        anchors.fill: parent; z:100
        enabled: root.visible && !root.controller.busy && !root.dialogOpen
        function accepts(event) {return event.hasUrls && event.urls.length>0 && event.urls.every(url => /^file:/i.test(url.toString()) && /\.(fits?|fts|fz|tiff?|png|cr2|cr3|nef|nrw|arw|dng|orf|raf|rw2|pef)$/i.test(url.toString()));}
        onEntered: drag => {drag.accepted=accepts(drag);}
        onDropped: drop => {if(!accepts(drop))return; controller.addFiles(drop.urls,root.selectedRole(),group.currentText,Number(aduScale.text));drop.acceptProposedAction();}
        Rectangle {anchors.fill:parent;anchors.margins:8;visible:parent.containsDrag;color:"#e6111c2c";border.color:"#6ca5f8";radius:8
            Label {anchors.centerIn:parent;text:qsTr("Drop deep-sky light frames");color:"#e8edf5";font.pixelSize:22}
        }
    }
    RowLayout {
        anchors.fill:parent; anchors.margins:12;anchors.topMargin:workspaceHeader.height+24; spacing:12;layoutDirection:Qt.RightToLeft
        ScrollView {
            id:settingsScroll;objectName:"deepSkyInspector"
            Layout.preferredWidth:350; Layout.minimumWidth:280; Layout.maximumWidth:Math.max(280,root.width*.46); Layout.fillHeight:true
            padding:12;clip:true
            background:Rectangle {color:"#151a22";radius:8;border.color:"#29313d"}
            ColumnLayout {
                width:settingsScroll.availableWidth; spacing:10
        RowLayout {
            Layout.fillWidth:true;spacing:6
            Repeater {model:[qsTr("Images"),qsTr("Stack"),qsTr("Finish")]
                delegate:IconButton {required property int index;required property string modelData;objectName:index===2?"deepSkyFinishTab":index===1?"deepSkyStackTab":"deepSkyImagesTab";enabled:index!==2 || controller.hasResult;text:modelData;Layout.fillWidth:true;borderColor:root.workflowStep===[0,1,6][index]?"#6ca5f8":"#344150";onClicked:root.workflowStep=[0,1,6][index]}
            }
        }
                ColumnLayout {
                visible:root.workflowStep===0;Layout.fillWidth:true;Layout.minimumWidth:0
                SectionTitle {label:qsTr("Import");explanation:qsTr("Add individual files or import a folder and its subfolders. Automatic detection uses FITS metadata and file/folder names. Long offsets are classified as dark flats. Check and edit the roles in the image list.")}
                ParameterLabel {label:qsTr("Image type");explanation:qsTr("Lights contain the object. Darks correct thermal signal. Flats correct dust and uneven illumination. Bias or dark flats correct the flats. Automatic detection can be overridden for each image.")}
                ComboBox {id:rolePicker;objectName:"deepSkyImportRole";Layout.fillWidth:true;Layout.minimumWidth:0;model:root.roleNames;enabled:!controller.busy}
                IconButton {text:qsTr("Add files…");Layout.fillWidth:true;Layout.minimumWidth:0;enabled:!controller.busy;onClicked:root.importFiles(root.selectedRole())}
                IconButton {text:qsTr("Import folder…");Layout.fillWidth:true;Layout.minimumWidth:0;enabled:!controller.busy;onClicked:folderDialog.open()}
                Label {text:qsTr("Drop files into this workspace, or import a complete folder. Originals stay unchanged.");color:"#9daec3";wrapMode:Text.Wrap;Layout.fillWidth:true;Layout.minimumWidth:0;font.pixelSize:11}
                SectionTitle {label:qsTr("Import settings")}
                ParameterLabel {label:qsTr("Filter / group");explanation:qsTr("Auto reads the FITS filter. R, G, B, L, HA, OIII and SII are integrated separately. Color-camera data normally belongs to RGB. You can change each image group in the list.")}
                ComboBox {id:group;objectName:"deepSkyImportGroup";Layout.fillWidth:true;Layout.minimumWidth:0;model:["Auto","Mono","RGB","R","G","B","L","HA","OIII","SII"];enabled:!controller.busy}
                ParameterLabel {label:qsTr("ADU scale");explanation:qsTr("Leave 0 for automatic import. Override only when you know the numeric scale of a scientific FITS. Use the same convention for lights and calibration images.")}
                TextField {id:aduScale;Layout.fillWidth:true;Layout.minimumWidth:0;text:"0";enabled:!controller.busy;validator:DoubleValidator {bottom:0;top:1e12;locale:"C"}}

                }
                ColumnLayout {
                visible:root.workflowStep===1;Layout.fillWidth:true;Layout.minimumWidth:0
                SectionTitle {label:qsTr("Calibration");explanation:qsTr("Raw images need matching darks and flats. A raw dark already includes bias. Dark exposure, gain and known temperature must match. For the Siril tutorial, import Offsets_3s as Dark flats.")}
                ParameterLabel {label:qsTr("Input calibration");explanation:qsTr("Enable Lights already calibrated only when the files have already been preprocessed, for example calibrated iTelescope FITS. In that mode the imported calibration files are not applied.")}
                CheckBox {id:calibrated;objectName:"deepSkyCalibrated";text:qsTr("Lights already calibrated");checked:false;enabled:!controller.busy;Layout.fillWidth:true}
                Label {text:calibrated.checked?qsTr("Already calibrated"):qsTr("Raw images · calibration enabled");color:"#89a9cc";Layout.fillWidth:true;font.pixelSize:11}
                CheckBox {Layout.fillWidth:true;Layout.minimumWidth:0;implicitWidth:settingsScroll.availableWidth;contentItem:Text {text:parent.text;font:parent.font;color:"#e8edf5";wrapMode:Text.Wrap;leftPadding:parent.indicator.width+parent.spacing;verticalAlignment:Text.AlignVCenter} id:badPixels;text:qsTr("Correct bad pixels");checked:true;enabled:!controller.busy && !calibrated.checked}
                ParameterLabel {label:qsTr("Bad pixels");explanation:qsTr("Use an imported map or detect isolated hot pixels in the master dark. The correction runs before demosaicing and preserves the Bayer pattern.")}
                IconButton {text:qsTr("Bad pixel map…");Layout.fillWidth:true;Layout.minimumWidth:0;enabled:!controller.busy;onClicked:root.importFiles("BadPixels")}
                }
                Rectangle {
                    id:advancedPanel;objectName:"deepSkyAdvancedPanel";visible:root.workflowStep===1
                    Layout.fillWidth:true;Layout.minimumWidth:0;implicitHeight:advancedContents.implicitHeight+24
                    color:"#141a22";radius:8;border.color:"#344150"
                    ColumnLayout {
                        id:advancedContents;anchors.left:parent.left;anchors.right:parent.right;anchors.top:parent.top;anchors.margins:12;spacing:12
                        ToolButton {
                            id:advanced;objectName:"deepSkyAdvanced";property bool expanded:false
                            Layout.fillWidth:true;implicitHeight:36;padding:0
                            contentItem:RowLayout {
                                Label {text:qsTr("Advanced stack settings");color:"#eef3fa";font.pixelSize:16;font.bold:true;Layout.fillWidth:true;wrapMode:Text.Wrap}
                                Label {text:advanced.expanded?"▾":"▸";color:"#9aa7b8";font.pixelSize:18}
                            }
                            background:Item {}
                            onClicked:expanded=!expanded
                        }
                        ColumnLayout {
                            visible:advanced.expanded;Layout.fillWidth:true;spacing:12
                            Label {text:qsTr("Normalization, stacking method and rejection threshold.");color:"#b5c3d4";font.pixelSize:12;wrapMode:Text.Wrap;Layout.fillWidth:true}
                            ParameterLabel {label:qsTr("Normalization");explanation:qsTr("Match stellar flux and background offsets to the reference. Disable for data normalized externally. This is global normalization, not photometric color calibration.")}
                            CheckBox {id:normalize;text:qsTr("Normalize to the reference");checked:true;enabled:!controller.busy;Layout.fillWidth:true}
                            ParameterLabel {label:qsTr("Gradient before stacking");explanation:qsTr("Optional degree-1 subtraction after calibration and debayering, before alignment. Each exposure gets its own automatic grid (20 samples per line, tolerance 1). Leave disabled for fields filled with nebulosity.")}
                            CheckBox {id:sequenceBackground;objectName:"deepSkySequenceBackground";text:qsTr("Correct each calibrated exposure");enabled:!controller.busy;Layout.fillWidth:true}
                            Rectangle {Layout.fillWidth:true;Layout.preferredHeight:1;color:"#344150"}
                            ParameterLabel {label:qsTr("Integration");explanation:qsTr("Robust rejection requires at least three accepted lights. Lower sigma rejects more aggressively. Mean without rejection keeps all usable samples. Borders are cropped to common coverage.")}
                            ComboBox {id:integrationMethod;Layout.fillWidth:true;Layout.minimumWidth:0;model:[qsTr("Mean with robust rejection"),qsTr("Mean without rejection")];enabled:!controller.busy}
                            RowLayout {
                                Layout.fillWidth:true
                                ParameterLabel {label:qsTr("Rejection σ");explanation:qsTr("Lower sigma removes more outliers; higher sigma keeps more samples. Start at 3. Rejection needs at least three accepted lights.")}
                                SpinBox {id:sigma;from:1;to:10;value:3;enabled:!controller.busy && integrationMethod.currentIndex===0}
                            }
                        }
                    }
                }
                ParameterLabel {visible:root.workflowStep===1;label:qsTr("Reference and alignment");explanation:qsTr("Reference selects one light per filter group. Other frames are aligned to its stars. Choose a sharp frame with many recognizable stars; this is not the frame used as the final output.")}
                Label {visible:root.workflowStep===1;text:qsTr("Check the stars, FWHM and reference in the image list before stacking.");color:"#9daec3";font.pixelSize:12;wrapMode:Text.Wrap;Layout.fillWidth:true}
                Rectangle {objectName:"deepSkyStackDivider";visible:root.workflowStep===1;Layout.fillWidth:true;Layout.preferredHeight:1;color:"#526174";Layout.topMargin:8;Layout.bottomMargin:8}
                IconButton {visible:root.workflowStep===1;objectName:"deepSkyIntegrate";text:qsTr("Create the stacked image");Layout.fillWidth:true;Layout.minimumWidth:0;enabled:!controller.busy && controller.frames.some(row=>row.selected && row.role==="Light");onClicked:controller.integrate(calibrated.checked,integrationMethod.currentIndex===0?sigma.value:0,normalize.checked,badPixels.checked,sequenceBackground.checked)}
                ColumnLayout {
                visible:root.workflowStep===6;Layout.fillWidth:true;Layout.minimumWidth:0
                GridLayout {
                    columns:2;Layout.fillWidth:true
                    Repeater {model:[qsTr("Stretch"),qsTr("Background"),qsTr("Color and noise"),qsTr("Orientation")]
                        delegate:IconButton {required property int index;required property string modelData;text:modelData;Layout.fillWidth:true;borderColor:root.finishingTool===index?"#6ca5f8":"#344150";onClicked:root.finishingTool=index}
                    }
                }
                RowLayout {
                    Layout.fillWidth:true
                    IconButton {objectName:"deepSkyUndo";text:qsTr("Undo");Layout.fillWidth:true;enabled:root.finishEditable && edits.canUndo;onClicked:root.undoFinish()}
                    IconButton {objectName:"deepSkyRedo";text:qsTr("Redo");Layout.fillWidth:true;enabled:root.finishEditable && edits.canRedo;onClicked:root.redoFinish()}
                }
            ColumnLayout {
                visible:root.workflowStep===6 && controller.hasResult && root.finishingTool===0;Layout.fillWidth:true;spacing:3
                ColumnLayout {
                    Layout.fillWidth:true
                    Label {text:qsTr("Input histogram and stretch curve");color:"#e8edf5";Layout.fillWidth:true;wrapMode:Text.Wrap}
                    RowLayout {Layout.fillWidth:true
                    ComboBox {id:histogramControl;objectName:"deepSkyHistogramControl";onCurrentIndexChanged:linearHistogram.requestPaint();model:[qsTr("Black"),qsTr("Midtones"),qsTr("White")];Layout.preferredWidth:120;enabled:root.finishEditable}
                    Label {text:qsTr("Zoom");color:"#a8b5c6"}
                    SpinBox {id:histogramZoom;from:1;to:50;value:1;Layout.preferredWidth:100}
                    }
                }
                Canvas {
                    id:linearHistogram;objectName:"deepSkyLinearHistogram";Layout.fillWidth:true;Layout.preferredHeight:115
                    property var stats:controller.linearHistogram
                    property real blackPoint:Number(black.text)
                    property real whitePoint:Number(white.text)
                    property real midpoint:midtones.value
                    property real zoom:histogramZoom.value
                    property bool useAsinh:asinh.checked
                    property real strength:asinhStrength.value
                    property real low:stats.minimum===undefined?0:stats.minimum
                    property real high:low+(Math.max(stats.maximum||1,whitePoint)-low)/zoom
                    onStatsChanged:requestPaint();onBlackPointChanged:requestPaint();onWhitePointChanged:requestPaint();onMidpointChanged:requestPaint();onZoomChanged:requestPaint();onUseAsinhChanged:requestPaint();onStrengthChanged:requestPaint()
                    onWidthChanged:requestPaint();onHeightChanged:requestPaint()
                    function pointX(value) {return Math.max(0,Math.min(width,(value-low)/(high-low)*width));}
                    onPaint: {
                        const ctx=getContext("2d");ctx.clearRect(0,0,width,height);ctx.fillStyle="#12161c";ctx.fillRect(0,0,width,height);
                        const channels=stats.channels||[],colors=["#cf6060","#60ba80","#729dde"];
                        for(let c=0;c<channels.length;c++) {
                            const bins=channels[c];let peak=1;for(let i=0;i<bins.length;i++)peak=Math.max(peak,bins[i]);
                            ctx.strokeStyle=colors[c];ctx.beginPath();
                            for(let i=0;i<bins.length;i++){const value=low+(stats.maximum-low)*i/(bins.length-1);if(value>high)break;const x=(value-low)/(high-low)*width,y=height-15-(height-20)*Math.log(1+bins[i])/Math.log(1+peak);if(i===0)ctx.moveTo(x,y);else ctx.lineTo(x,y);}ctx.stroke();
                        }
                        if(whitePoint>blackPoint) {
                            ctx.strokeStyle="#e8edf5";ctx.beginPath();
                            for(let x=0;x<=width;x++){const value=low+x/width*(high-low),v=Math.max(0,Math.min(1,(value-blackPoint)/(whitePoint-blackPoint)));const out=useAsinh?Math.asinh(strength*v)/Math.asinh(strength):((midpoint-1)*v)/((2*midpoint-1)*v-midpoint);const y=height-15-(height-20)*out;if(x===0)ctx.moveTo(x,y);else ctx.lineTo(x,y);}ctx.stroke();
                        }
                        const points=[blackPoint,blackPoint+midpoint*(whitePoint-blackPoint),whitePoint],labels=["B","M","W"];
                        for(let i=0;i<3;i++){if(i===1&&useAsinh)continue;const x=pointX(points[i]);ctx.fillStyle=i===histogramControl.currentIndex?"#6ca5f8":"#e8edf5";ctx.fillRect(Math.max(0,Math.min(width-2,x)),0,2,height-15);ctx.fillText(labels[i],Math.max(2,Math.min(width-10,x)),height-2);}
                    }
                    MouseArea {
                        anchors.fill:parent;enabled:root.finishEditable
                        function changePoint(x) {
                            const value=linearHistogram.low+Math.max(0,Math.min(1,x/width))*(linearHistogram.high-linearHistogram.low);
                            if(histogramControl.currentIndex===0)black.text=Math.min(value,Number(white.text)-1e-6).toPrecision(7);
                            else if(histogramControl.currentIndex===2)white.text=Math.max(value,Number(black.text)+1e-6).toPrecision(7);
                            else if(!asinh.checked)midtones.value=Math.max(.001,Math.min(.999,(value-Number(black.text))/(Number(white.text)-Number(black.text))));
                            root.previewAdjustments();linearHistogram.requestPaint();
                        }
                        onPressed:mouse=>{edits.beginEdit();changePoint(mouse.x);}
                        onReleased:edits.endEdit()
                        onCanceled:edits.endEdit()
                        onPositionChanged:mouse=>{if(pressed)changePoint(mouse.x);}
                    }
                }
                Label {text:qsTr("Select Black, Midtones or White, then drag in the graph. Zoom only changes the graph view.");color:"#a8b5c6";font.pixelSize:11;wrapMode:Text.Wrap;Layout.fillWidth:true}
                Rectangle {Layout.fillWidth:true;implicitHeight:lossInfo.implicitHeight+20;color:"#18212c";radius:6
                    ColumnLayout {id:lossInfo;anchors {left:parent.left;right:parent.right;top:parent.top;margins:10} spacing:4
                        Label {text:qsTr("Estimated clipping · shadows %1% · highlights %2%").arg(Number(controller.linearHistogram.blackLoss||0).toFixed(2)).arg(Number(controller.linearHistogram.whiteLoss||0).toFixed(2));color:"#e8edf5";font.pixelSize:12;wrapMode:Text.Wrap;Layout.fillWidth:true}
                        Label {text:{const h=controller.linearHistogram;const channels=h.shadowChannels||[],high=h.highlightChannels||[];return ["R","G","B"].map((c,i)=>c+" : "+Number(channels[i]||0).toFixed(2)+"% / "+Number(high[i]||0).toFixed(2)+"%").join(" · ");} color:"#a8b5c6";font.pixelSize:11;wrapMode:Text.Wrap;Layout.fillWidth:true}
                        Label {text:qsTr("Shadow / highlight estimates at the level thresholds, after background correction and gains. These are channel samples, not the fraction of whole pixels lost. Later color operations may also clip.");color:"#a8b5c6";font.pixelSize:11;wrapMode:Text.Wrap;Layout.fillWidth:true}
                    }
                }
            }
                ColumnLayout {Layout.fillWidth:true;Layout.minimumWidth:0;visible:controller.groups.length>1
                SectionTitle {label:qsTr("Channel composition");explanation:qsTr("Assign R/G/B or a narrowband palette. RGB uses R/G/B, SHO uses SII/HA/OIII and HOO uses HA/OIII/OIII. Optional luminance adds detail. Each chosen filter must have an integrated result.")}
                ComboBox {id:resultGroup;Layout.fillWidth:true;Layout.minimumWidth:0;model:controller.groups;enabled:!controller.busy && count>0;onActivated:controller.selectResult(currentText)}
                Label {text:qsTr("Combine integrated filters: RGB = R/G/B, SHO = SII/HA/OIII, HOO = HA/OIII/OIII.");color:"#a8b5c6";wrapMode:Text.Wrap;Layout.fillWidth:true;Layout.minimumWidth:0;font.pixelSize:11}
                GridLayout {
                    columns:2;Layout.fillWidth:true;Layout.minimumWidth:0;enabled:!controller.busy && controller.groups.length>0
                    Label {text:qsTr("Red");color:"#a8b5c6"}
                    ComboBox {id:redChannel;Layout.fillWidth:true;Layout.minimumWidth:0;model:controller.groups}
                    Label {text:qsTr("Green");color:"#a8b5c6"}
                    ComboBox {id:greenChannel;Layout.fillWidth:true;Layout.minimumWidth:0;model:controller.groups}
                    Label {text:qsTr("Blue");color:"#a8b5c6"}
                    ComboBox {id:blueChannel;Layout.fillWidth:true;Layout.minimumWidth:0;model:controller.groups}
                    Label {text:qsTr("Luminance");color:"#a8b5c6"}
                    ComboBox {id:luminanceChannel;Layout.fillWidth:true;Layout.minimumWidth:0;model:[qsTr("None")].concat(controller.groups)}
                }
                IconButton {text:qsTr("Combine channels");Layout.fillWidth:true;Layout.minimumWidth:0;enabled:!controller.busy && controller.groups.length>0;onClicked:controller.compose(redChannel.currentText,greenChannel.currentText,blueChannel.currentText,luminanceChannel.currentIndex>0?luminanceChannel.currentText:"")}
                }
                ToolButton {id:recipeSettings;property bool expanded:false;Layout.fillWidth:true;text:qsTr("Versions A/B and presets")+(expanded?" ▾":" ▸");onClicked:expanded=!expanded}
                ColumnLayout {visible:recipeSettings.expanded;Layout.fillWidth:true
                SectionTitle {label:qsTr("Presets and history");explanation:qsTr("Presets save finishing controls only. They do not change calibration, filters or channel composition. Absolute black and white levels depend on your data: check the histogram after loading. Each A/B version has its own undo history, limited to 100 changes.")}
                RowLayout {
                    Layout.fillWidth:true
                    ComboBox {id:presetChooser;objectName:"deepSkyPresetChooser";Layout.fillWidth:true;Layout.minimumWidth:0;model:root.library.deepSkyPresetNames;displayText:currentIndex>=0?currentText:qsTr("Your presets")}
                    IconButton {text:qsTr("Load");enabled:root.finishEditable && presetChooser.currentIndex>=0;onClicked:root.setFinish(root.library.deepSkyPreset(presetChooser.currentText))}
                }
                IconButton {text:qsTr("Manage presets…");Layout.fillWidth:true;enabled:root.finishEditable;onClicked:{presetName.text=presetChooser.currentIndex>=0?presetChooser.currentText:"";presetDialog.open();}}
                SectionTitle {label:qsTr("Versions A/B");explanation:qsTr("A and B use the same linear stack and framing, with independent finishing controls and histories. Switch versions to compare; edits and final exports use the selected version. Copy duplicates the current settings into the other version. A new stack or channel composition starts a fresh pair.")}
                RowLayout {
                    Layout.fillWidth:true
                    IconButton {objectName:"deepSkyVersionA";text:qsTr("Version A");Layout.fillWidth:true;borderColor:edits.version==="A"?"#6ca5f8":"#344150";enabled:root.finishEditable;onClicked:root.switchVersion("A")}
                    IconButton {objectName:"deepSkyVersionB";text:qsTr("Version B");Layout.fillWidth:true;borderColor:edits.version==="B"?"#6ca5f8":"#344150";enabled:root.finishEditable;onClicked:root.switchVersion("B")}
                }
                IconButton {text:edits.version==="A"?qsTr("Copy A to B"):qsTr("Copy B to A");Layout.fillWidth:true;enabled:root.finishEditable;onClicked:edits.copyToOther()}
                Label {text:qsTr("Selected version %1").arg(edits.version);color:"#a8b5c6";wrapMode:Text.Wrap;Layout.fillWidth:true}
                }
                Label {text:qsTr("Preview has unapplied settings. Apply them before final export.");visible:root.finishDirty && !livePreview.checked;color:"#edb06c";wrapMode:Text.Wrap;Layout.fillWidth:true}
                Label {text:root.editError;color:"#edb06c";visible:text.length>0;wrapMode:Text.Wrap;Layout.fillWidth:true}
                SectionTitle {visible:root.finishingTool===0;label:qsTr("Display and stretch");explanation:qsTr("Draft adjustments start from the output of applied transformations, or from the linear stack when history is empty. Live preview can be disabled. Automatic starting point is optional; Reset to linear restores un-stretched pixels and neutral gains.")}
                CheckBox {Layout.fillWidth:true;Layout.minimumWidth:0;implicitWidth:settingsScroll.availableWidth;contentItem:Text {text:parent.text;font:parent.font;color:"#e8edf5";wrapMode:Text.Wrap;leftPadding:parent.indicator.width+parent.spacing;verticalAlignment:Text.AlignVCenter} id:livePreview;objectName:"deepSkyLivePreview";text:qsTr("Live adjustment preview");checked:true;enabled:controller.hasResult}
                ColumnLayout {visible:root.finishingTool===0;Layout.fillWidth:true;Layout.minimumWidth:0;enabled:root.finishEditable
                    IconButton {text:qsTr("Automatic starting point");Layout.fillWidth:true;Layout.minimumWidth:0;onClicked:{const s=controller.suggestedStretch;root.setFinish({background:false,stretch:true,black:s.black,white:s.white,midtones:s.midtones,red:s.red,green:s.green,blue:s.blue,denoise:0,asinh:false,asinhStrength:20,saturation:1});}}
                    IconButton {text:qsTr("Reset to linear");Layout.fillWidth:true;Layout.minimumWidth:0;onClicked:root.setFinish({background:false,stretch:false,black:0,white:1,midtones:.15,red:1,green:1,blue:1,denoise:0,asinh:false,asinhStrength:20,saturation:1})}
                }
                ColumnLayout {visible:root.finishingTool===1;Layout.fillWidth:true
                SectionTitle {label:qsTr("Background");explanation:qsTr("The optional correction fits a polynomial independently in each color channel. Bright grid samples are rejected using the tolerance. Compare with correction disabled to protect extended nebulosity. Enable sample editing to add or remove points directly on the image.")}
                CheckBox {Layout.fillWidth:true;Layout.minimumWidth:0;implicitWidth:settingsScroll.availableWidth;contentItem:Text {text:parent.text;font:parent.font;color:"#e8edf5";wrapMode:Text.Wrap;leftPadding:parent.indicator.width+parent.spacing;verticalAlignment:Text.AlignVCenter} id:background;onClicked:root.previewAdjustments();text:qsTr("Correct background gradient");enabled:root.finishEditable}
                CheckBox {text:qsTr("Show extracted background model");checked:controller.showingBackgroundModel;enabled:!controller.busy && controller.hasBackgroundModel && rotation.currentIndex===0 && !mirrorHorizontal.checked && !mirrorVertical.checked;onClicked:controller.showingBackgroundModel=checked;DarkToolTip {visible:parent.hovered;text:qsTr("Displays the fitted background with an automatic display stretch. Export always uses the processed image. Inspect the model for traces of real astronomical objects.")}}
                CheckBox {text:qsTr("Edit background samples on image");checked:root.editingBackgroundSamples;enabled:root.finishEditable && background.checked;onClicked:{root.editingBackgroundSamples=checked;if(checked){rotation.currentIndex=0;mirrorHorizontal.checked=false;mirrorVertical.checked=false;root.previewAdjustments();}}}
                Label {visible:root.editingBackgroundSamples;text:qsTr("Left click adds a sky sample. Right click removes one. Avoid stars and nebulosity. Rotate and mirror are temporarily unavailable while editing samples.");color:"#a8b5c6";wrapMode:Text.Wrap;Layout.fillWidth:true}
                IconButton {visible:background.checked;text:qsTr("Regenerate automatic samples");Layout.fillWidth:true;onClicked:{root.backgroundPoints=[];root.previewAdjustments()}}
                GridLayout {
                    columns:2;Layout.fillWidth:true;visible:background.checked;enabled:root.finishEditable
                    ParameterLabel {label:qsTr("Polynomial degree");explanation:qsTr("Start with degree 1. Higher degrees fit curved gradients but can remove real nebulosity.")}
                    ComboBox {id:backgroundDegree;objectName:"deepSkyBackgroundDegree";model:["1","2","3","4"];onActivated:root.previewAdjustments()}
                    ParameterLabel {label:qsTr("Samples per line");explanation:qsTr("Density of the automatic background grid. More samples do not guarantee a better model.")}
                    SpinBox {id:backgroundSamples;from:2;to:50;value:20;onValueModified:root.previewAdjustments()}
                    ParameterLabel {label:qsTr("Grid tolerance");explanation:qsTr("Reject samples above the median plus this multiple of the median absolute deviation. Increase if too few samples remain.")}
                    Slider {id:backgroundTolerance;Layout.fillWidth:true;from:0;to:10;value:1;onMoved:root.previewAdjustments()}
                }
                }
                ColumnLayout {visible:root.finishingTool===3;Layout.fillWidth:true;enabled:!root.editingBackgroundSamples
                SectionTitle {label:qsTr("Orientation");explanation:qsTr("Rotate clockwise, then mirror the finished image. The original linear integration remains unchanged. Orientation is saved with the project and can be undone.")}
                ComboBox {id:rotation;objectName:"deepSkyRotation";Layout.fillWidth:true;model:[qsTr("Original orientation"),"90°","180°","270°"];enabled:root.finishEditable;onActivated:root.previewAdjustments()}
                CheckBox {id:mirrorHorizontal;text:qsTr("Mirror horizontally");enabled:root.finishEditable;onClicked:root.previewAdjustments()}
                CheckBox {id:mirrorVertical;text:qsTr("Mirror vertically");enabled:root.finishEditable;onClicked:root.previewAdjustments()}
                }
                ColumnLayout {visible:root.finishingTool===0;Layout.fillWidth:true
                CheckBox {Layout.fillWidth:true;Layout.minimumWidth:0;implicitWidth:settingsScroll.availableWidth;contentItem:Text {text:parent.text;font:parent.font;color:"#e8edf5";wrapMode:Text.Wrap;leftPadding:parent.indicator.width+parent.spacing;verticalAlignment:Text.AlignVCenter} id:stretch;onClicked:{if(!checked)scnr.checked=false;root.previewAdjustments();}text:qsTr("Stretch faint details");checked:true;enabled:root.finishEditable}
                GridLayout {
                    columns:2;Layout.fillWidth:true;Layout.minimumWidth:0;enabled:root.finishEditable
                    ParameterLabel {label:qsTr("Black");explanation:qsTr("Values below black are clipped. Raise it carefully to darken the sky without removing faint nebulosity. The histogram reports estimated shadow clipping.")}
                    TextField {id:black;onEditingFinished:root.previewAdjustments();Layout.fillWidth:true;Layout.minimumWidth:0;text:"0";validator:DoubleValidator {locale:"C"}}
                    ParameterLabel {label:qsTr("White");explanation:qsTr("Values above white are clipped. Lowering it can burn bright star cores. Keep the automatic maximum unless you intentionally want clipping.")}
                    TextField {id:white;onEditingFinished:root.previewAdjustments();Layout.fillWidth:true;Layout.minimumWidth:0;text:"1";validator:DoubleValidator {locale:"C"}}
                }
                CheckBox {Layout.fillWidth:true;Layout.minimumWidth:0;implicitWidth:settingsScroll.availableWidth;contentItem:Text {text:parent.text;font:parent.font;color:"#e8edf5";wrapMode:Text.Wrap;leftPadding:parent.indicator.width+parent.spacing;verticalAlignment:Text.AlignVCenter} id:asinh;onClicked:root.previewAdjustments();text:qsTr("Asinh stretch · preserve color ratios");enabled:root.finishEditable && stretch.checked}
                ParameterLabel {visible:asinh.checked;label:qsTr("Asinh luminance");explanation:qsTr("Human weighted uses eye sensitivity; mean gives equal weight to RGB; maximum preserves the historical behavior. A shared factor links the channels, but black/white clipping can still alter colors.")}
                ComboBox {id:asinhLuminance;visible:asinh.checked;Layout.fillWidth:true;model:[qsTr("Maximum channel"),qsTr("Human weighted"),qsTr("RGB mean")];enabled:root.finishEditable;onActivated:root.previewAdjustments()}
                ParameterLabel {visible:asinh.checked;label:qsTr("Asinh strength · %1").arg(asinhStrength.value.toFixed(0));explanation:qsTr("Controls the asinh stretch strength. The draft uses either asinh or midtones. Commit a transformation to apply another stretch afterwards.")}
                Slider {id:asinhStrength;onMoved:root.previewAdjustments();onPressedChanged:{if(pressed)edits.beginEdit();else edits.endEdit();}visible:asinh.checked;Layout.fillWidth:true;Layout.minimumWidth:0;from:1;to:1000;value:20;enabled:root.finishEditable}
                ParameterLabel {visible:!asinh.checked;label:qsTr("Midtones · %1").arg(midtones.value.toFixed(3));explanation:qsTr("Lower values brighten faint detail. This changes the stretch curve without changing the black and white thresholds.")}
                Slider {id:midtones;onMoved:root.previewAdjustments();onPressedChanged:{if(pressed)edits.beginEdit();else edits.endEdit();}visible:!asinh.checked;Layout.fillWidth:true;Layout.minimumWidth:0;from:.001;to:.999;value:.15;enabled:root.finishEditable}
                }
                ColumnLayout {visible:root.finishingTool===2;Layout.fillWidth:true
                SectionTitle {label:qsTr("Color and noise")}
                ParameterLabel {label:qsTr("Red gain · %1").arg(redGain.value.toFixed(2));explanation:qsTr("Multiply the red channel before stretching. Initial gains estimate a neutral background; adjust manually for your intended colors.")}
                Slider {id:redGain;onMoved:root.previewAdjustments();onPressedChanged:{if(pressed)edits.beginEdit();else edits.endEdit();}Layout.fillWidth:true;Layout.minimumWidth:0;from:.1;to:3;value:1;enabled:root.finishEditable}
                ParameterLabel {label:qsTr("Green gain · %1").arg(greenGain.value.toFixed(2));explanation:qsTr("Multiply the green channel before stretching. This is not SCNR or photometric calibration.")}
                Slider {id:greenGain;onMoved:root.previewAdjustments();onPressedChanged:{if(pressed)edits.beginEdit();else edits.endEdit();}Layout.fillWidth:true;Layout.minimumWidth:0;from:.1;to:3;value:1;enabled:root.finishEditable}
                ParameterLabel {label:qsTr("Blue gain · %1").arg(blueGain.value.toFixed(2));explanation:qsTr("Multiply the blue channel before stretching. Watch the background and stellar colors while adjusting.")}
                Slider {id:blueGain;onMoved:root.previewAdjustments();onPressedChanged:{if(pressed)edits.beginEdit();else edits.endEdit();}Layout.fillWidth:true;Layout.minimumWidth:0;from:.1;to:3;value:1;enabled:root.finishEditable}
                ParameterLabel {label:qsTr("Saturation · %1").arg(saturation.value.toFixed(2));explanation:qsTr("1 preserves saturation, 0 removes color, values above 1 strengthen it. The darkest background is protected to avoid amplifying color noise.")}
                Slider {id:saturation;onMoved:root.previewAdjustments();onPressedChanged:{if(pressed)edits.beginEdit();else edits.endEdit();}Layout.fillWidth:true;Layout.minimumWidth:0;from:0;to:3;value:1;enabled:root.finishEditable}
                SectionTitle {label:qsTr("Green noise reduction");explanation:qsTr("Optional SCNR on the stretched image, after background and color balance. Avoid for intentional green colors in comets, planetary nebulae or SHO palettes. This is not photometric calibration.")}
                CheckBox {id:scnr;objectName:"deepSkyScnr";text:qsTr("Remove green noise");enabled:root.finishEditable && (stretch.checked || root.operations.some(op=>op.enabled && op.settings.stretch));onClicked:root.previewAdjustments()}
                ColumnLayout {visible:scnr.checked;Layout.fillWidth:true
                    ComboBox {id:scnrMethod;Layout.fillWidth:true;model:[qsTr("Average neutral protection"),qsTr("Maximum neutral protection"),qsTr("Maximum mask protection"),qsTr("Additive mask protection")];onActivated:root.previewAdjustments()}
                    ParameterLabel {visible:scnrMethod.currentIndex>=2;label:qsTr("Amount · %1").arg(scnrAmount.value.toFixed(2));explanation:qsTr("Mask methods have an adjustable amount. Neutral methods cap green using red and blue without this parameter.")}
                    Slider {id:scnrAmount;visible:scnrMethod.currentIndex>=2;Layout.fillWidth:true;from:0;to:1;value:1;onMoved:root.previewAdjustments();onPressedChanged:{if(pressed)edits.beginEdit();else edits.endEdit()}}
                    CheckBox {id:scnrPreserve;text:qsTr("Preserve CIE lightness");checked:true;onClicked:root.previewAdjustments()}
                }
                ParameterLabel {label:qsTr("Noise smoothing · %1").arg(denoise.value.toFixed(2));explanation:qsTr("Blends a small Gaussian smoothing into the image. Use low values and inspect at 100 percent to preserve fine structures.")}
                Slider {id:denoise;onMoved:root.previewAdjustments();onPressedChanged:{if(pressed)edits.beginEdit();else edits.endEdit();}Layout.fillWidth:true;Layout.minimumWidth:0;from:0;to:1;value:0;enabled:root.finishEditable}
                }
                Rectangle {Layout.fillWidth:true;Layout.preferredHeight:1;color:"#344150"}
                IconButton {objectName:"deepSkyCommitTransformation";text:qsTr("Apply draft as a transformation");Layout.fillWidth:true;enabled:root.finishEditable && (root.operations.length<100 || root.editingOperationIndex>=0);onClicked:root.commitTransformation();DarkToolTip {visible:parent.hovered;text:qsTr("Commits all current draft adjustments into the ordered history. New adjustments then operate on that result, so stretches can be repeated. Undo restores the previous draft.")}}
                IconButton {visible:root.editingOperationIndex>=0;text:qsTr("Cancel transformation edit");Layout.fillWidth:true;onClicked:root.cancelOperationEdit()}
                Label {text:qsTr("Applied transformations · %1").arg(root.operations.length);color:"#e8edf5";Layout.fillWidth:true}
                Repeater {model:root.operations
                    delegate:RowLayout {required property int index;required property var modelData;Layout.fillWidth:true
                        CheckBox {checked:modelData.enabled;enabled:root.finishEditable && root.editingOperationIndex<0;onClicked:root.changeOperation(index,false)}
                        Label {text:(index+1)+" · "+modelData.name;color:modelData.enabled?"#e8edf5":"#7b8796";elide:Text.ElideRight;Layout.fillWidth:true}
                        IconButton {text:qsTr("Edit");enabled:root.finishEditable && root.editingOperationIndex<0;onClicked:root.editOperation(index)}
                        IconButton {text:"↑";enabled:root.finishEditable && root.editingOperationIndex<0 && index>0;onClicked:root.moveOperation(index,-1)}
                        IconButton {text:"×";enabled:root.finishEditable && root.editingOperationIndex<0;onClicked:root.changeOperation(index,true);DarkToolTip {visible:parent.hovered;text:qsTr("Remove transformation")}}
                    }
                }
                Label {visible:root.operations.length>0;text:qsTr("The draft is applied after this history. Disable a transformation to compare its effect; downstream transformations are recalculated. Linear FITS still exports the original integration.");color:"#a8b5c6";wrapMode:Text.Wrap;Layout.fillWidth:true}
                SectionTitle {label:qsTr("Comparison");explanation:qsTr("Choose an original light and hold Raw exposure. Its framing and automatic stretch are independent from the final stack. The comparison does not change exports.")}
                ComboBox {
                    id:rawComparisonChoice;objectName:"deepSkyRawChoice";Layout.fillWidth:true;Layout.minimumWidth:0
                    property var lightRows:controller.frames.map((row,index)=>({name:row.name,index:index,role:row.role})).filter(row=>row.role==="Light")
                    model:lightRows.map(row=>row.name);enabled:!controller.busy && controller.hasResult
                    onActivated:controller.chooseRawComparison(lightRows[currentIndex].index)
                }
                IconButton {text:qsTr("Raw exposure (hold)");Layout.fillWidth:true;Layout.minimumWidth:0;enabled:!controller.busy && controller.hasResult && controller.hasRawComparison;onPressedChanged:root.compareRaw(pressed)}
                IconButton {visible:!livePreview.checked;text:qsTr("Apply adjustments");Layout.fillWidth:true;Layout.minimumWidth:0;enabled:root.finishEditable;onClicked:if(root.previewAdjustments())root.applyStoredFinish();}
                SectionTitle {label:qsTr("Export");explanation:qsTr("Linear FITS preserves the integration. Final image offers 16-bit TIFF or JPEG with the current finish. Details and caption transfers the finished pixels and acquisition summary to Processing.")}
                ColumnLayout {
                    Layout.fillWidth:true;Layout.minimumWidth:0;enabled:!controller.busy && controller.hasResult
                    IconButton {text:qsTr("Linear FITS…");Layout.fillWidth:true;Layout.minimumWidth:0;onClicked:{root.linearExport=true;exportDialog.nameFilters=["FITS (*.fits)"];exportDialog.open();}}
                    IconButton {text:qsTr("Final image…");enabled:!root.finishDirty && root.editingOperationIndex<0;Layout.fillWidth:true;Layout.minimumWidth:0;onClicked:{root.linearExport=false;exportDialog.nameFilters=["TIFF (*.tif)","JPEG (*.jpg)"];exportDialog.open();}}
                }
                IconButton {text:qsTr("Details and caption…");Layout.fillWidth:true;Layout.minimumWidth:0;enabled:!controller.busy && controller.hasResult && !root.finishDirty && root.editingOperationIndex<0 && root.canEditResult;onClicked:root.editResult()}
                }

            }
        }
        ColumnLayout {
            Layout.fillWidth:true;Layout.fillHeight:true;spacing:8
            RowLayout {
                Layout.fillWidth:true;Layout.preferredHeight:40;Layout.minimumHeight:40;Layout.maximumHeight:40
                Label {text:controller.showingBackgroundModel?qsTr("Background model · automatic display stretch"):controller.comparingRaw?qsTr("Raw exposure · %1 · automatic stretch").arg(controller.rawComparisonName):controller.displayingResult?(root.finishDirty?qsTr("Stacked result · pending adjustments"):qsTr("Stacked result · version %1").arg(edits.version)):controller.sourceAutoStretch?qsTr("Source image preview · automatic stretch"):qsTr("Source image preview · linear display");color:"#e8edf5";Layout.fillWidth:true;elide:Text.ElideRight;DarkToolTip {visible:labelHover.hovered;text:parent.text} HoverHandler {id:labelHover}}
                IconButton {objectName:"deepSkySources";iconName:"folder";text:qsTr("Source files");enabled:controller.frames.length>0;borderColor:root.frameListOpen?"#6ca5f8":"#344150";onClicked:{root.imageFocus=false;root.frameListOpen=!root.frameListOpen;}}
                IconButton {objectName:"deepSkyImageFocus";text:root.imageFocus?qsTr("Show histograms"):qsTr("Enlarge preview");onClicked:root.imageFocus=!root.imageFocus;ToolTip.visible:hovered;ToolTip.text:qsTr("Temporarily hide histograms and the image list to inspect A/B. Zoom and framing stay shared.")}
                IconButton {text:qsTr("Show stacked result");visible:controller.hasResult && !controller.displayingResult;enabled:!controller.busy;onClicked:controller.restoreResult()}
            }
            RowLayout {
                visible:!controller.displayingResult && !controller.comparingRaw && controller.imageWidth>0;Layout.fillWidth:true
                Label {text:controller.sourceInterpretation;color:"#a8b5c6";Layout.fillWidth:true;elide:Text.ElideRight}
                CheckBox {text:qsTr("Color preview");checked:controller.sourceDebayer;enabled:!controller.busy;onClicked:controller.sourceDebayer=checked;DarkToolTip {visible:parent.hovered;text:qsTr("Debayer the preview when Bayer metadata is available. Calibration always uses the original sensor samples.")}}
                CheckBox {text:qsTr("Auto display stretch");checked:controller.sourceAutoStretch;enabled:!controller.busy;onClicked:controller.sourceAutoStretch=checked;DarkToolTip {visible:parent.hovered;text:qsTr("Only changes the source preview. Disable to view linear pixel brightness.")}}
            }
            ImageViewport {
                id:viewport;objectName:"deepSkyViewport";preserveViewOnSourceResize:root.rawViewState!==null;Layout.fillWidth:true;Layout.fillHeight:true;Layout.minimumHeight:100
                sourceWidth:controller.imageWidth;sourceHeight:controller.imageHeight
                DeepSkyImageItem {parent:viewport.imageContainer;anchors.fill:parent;controller:root.controller}
                Canvas {
                    id:sampleOverlay;objectName:"deepSkySampleOverlay";z:10;parent:viewport.imageContainer;anchors.fill:parent
                    visible:root.workflowStep===6 && root.finishingTool===1 && root.editingBackgroundSamples && background.checked && controller.displayingResult && !controller.comparingRaw && rotation.currentIndex===0 && !mirrorHorizontal.checked && !mirrorVertical.checked
                    property real sampleZoom:viewport.effectiveZoom
                    onSampleZoomChanged:requestPaint()
                    onVisibleChanged:if(visible)requestPaint()
                    property var points:root.backgroundPoints.length?root.backgroundPoints:controller.backgroundSamples
                    onPointsChanged:requestPaint();onWidthChanged:requestPaint();onHeightChanged:requestPaint()
                    onPaint:{const ctx=getContext("2d");ctx.clearRect(0,0,width,height);ctx.strokeStyle="#78d4bb";ctx.lineWidth=1/viewport.effectiveZoom;const r=5/viewport.effectiveZoom;for(const p of points)ctx.strokeRect(p[0]*width-r,p[1]*height-r,2*r,2*r)}
                    MouseArea {
                        anchors.fill:parent;enabled:sampleOverlay.visible && root.finishEditable;acceptedButtons:Qt.LeftButton|Qt.RightButton
                        onClicked:mouse=>{
                            let points=Array.from(sampleOverlay.points,p=>[Number(p[0]),Number(p[1])]);
                            if(mouse.button===Qt.RightButton){let index=-1,distance=144/(viewport.effectiveZoom*viewport.effectiveZoom);for(let i=0;i<points.length;i++){const dx=points[i][0]*width-mouse.x,dy=points[i][1]*height-mouse.y,d=dx*dx+dy*dy;if(d<distance){distance=d;index=i;}}if(index<0)return;points.splice(index,1);if(points.length===0)return;}
                            else points.push([mouse.x/width,mouse.y/height]);
                            root.backgroundPoints=points;root.previewAdjustments();
                        }
                    }
                }
                Label {anchors.centerIn:parent;visible:controller.imageWidth===0;text:qsTr("Add light frames to begin\nThe preview is automatically stretched; source pixels stay linear.");color:"#a8b5c6";horizontalAlignment:Text.AlignHCenter;wrapMode:Text.Wrap;width:parent.width-40}
            }
            RowLayout {
                Layout.fillWidth:true
                visible:!root.imageFocus
                Label {text:qsTr("Displayed image histogram");color:"#a8b5c6";Layout.fillWidth:true}
                ComboBox {id:histogramMode;model:["RGB",qsTr("Luminance")];Layout.preferredWidth:140}
            }
            DeepSkyHistogramItem {visible:!root.imageFocus;Layout.fillWidth:true;Layout.preferredHeight:64;controller:root.controller;luminanceOnly:histogramMode.currentIndex===1}
            RowLayout {
                Layout.fillWidth:true
                visible:!root.imageFocus && root.frameListOpen
                ComboBox {id:frameFilterChoice;objectName:"deepSkyFrameFilter";model:[qsTr("All images")].concat(root.roleNames.slice(1));Layout.preferredWidth:155;onActivated:root.frameFilter=currentIndex===0?"All":root.roles[currentIndex]}
                Label {text:qsTr("%1 / %2 images").arg(root.visibleFrames.length).arg(controller.frames.length);color:"#9daec3";Layout.fillWidth:true}
                IconButton {text:qsTr("Select all");enabled:!controller.busy;onClicked:root.visibleFrames.forEach(row=>controller.setSelected(row.sourceIndex,true))}
                IconButton {text:qsTr("Select none");enabled:!controller.busy;onClicked:root.visibleFrames.forEach(row=>controller.setSelected(row.sourceIndex,false))}
            }
            ListView {
                id:frameList;objectName:"deepSkyFrameList";visible:!root.imageFocus && root.frameListOpen;Layout.fillWidth:true;Layout.preferredHeight:Math.min(160,Math.max(64,controller.frames.length*64));clip:true;model:root.visibleFrames
                ScrollBar.vertical:ScrollBar {}
                delegate:Rectangle {
                    required property var modelData
                    required property int index
                    width:frameList.width;height:58;color:index%2?"#12161c":"#171c24"
                    RowLayout {
                        anchors.fill:parent;anchors.margins:4;spacing:5
                        CheckBox {checked:modelData.selected;enabled:!controller.busy;onClicked:controller.setSelected(modelData.sourceIndex,checked)}
                        ColumnLayout {
                            Layout.fillWidth:true;spacing:1
                            Label {text:modelData.name;color:"#e8edf5";Layout.fillWidth:true;elide:Text.ElideMiddle;MouseArea {anchors.fill:parent;enabled:!controller.busy;onClicked:controller.inspect(modelData.sourceIndex)}}
                            Label {text:root.roleLabel(modelData.role)+" · "+modelData.group+" · "+Number(modelData.exposure).toFixed(1)+" s · "+modelData.width+"×"+modelData.height+(modelData.stars!==undefined?" · "+modelData.stars+qsTr(" stars")+" · FWHM "+Number(modelData.fwhm||0).toFixed(1):"");color:"#a8b5c6";font.pixelSize:11;Layout.fillWidth:true;elide:Text.ElideRight}
                        }
                        ComboBox {model:root.roleNames.slice(1);currentIndex:Math.max(0,root.roles.indexOf(modelData.role)-1);Layout.preferredWidth:110;enabled:!controller.busy;onActivated:controller.setRole(modelData.sourceIndex,root.roles[currentIndex+1]);ToolTip.visible:hovered;ToolTip.text:qsTr("Change image type")}
                        ComboBox {model:["Mono","RGB","R","G","B","L","HA","OIII","SII"];currentIndex:Math.max(0,model.indexOf(modelData.group));Layout.preferredWidth:72;enabled:!controller.busy;onActivated:controller.setGroup(modelData.sourceIndex,currentText);ToolTip.visible:hovered;ToolTip.text:qsTr("Change filter group")}
                        IconButton {text:qsTr("Reference");borderColor:modelData.reference?"#6ca5f8":"#3a4048";visible:modelData.role==="Light";enabled:!controller.busy && modelData.selected;onClicked:controller.setReference(modelData.sourceIndex);Accessible.name:qsTr("Set reference frame");ToolTip.visible:hovered;ToolTip.text:qsTr("Align the other lights in this group to this image")}
                        IconButton {text:"×";enabled:!controller.busy;onClicked:controller.remove(modelData.sourceIndex);Accessible.name:qsTr("Remove frame")}
                    }
                }
            }
            ProgressBar {Layout.fillWidth:true;Layout.preferredHeight:6;Layout.minimumHeight:6;Layout.maximumHeight:6;opacity:controller.busy?1:0;from:0;to:100;value:controller.progress}
            RowLayout {
                Layout.fillWidth:true
                Item {
                    id: activitySpinner; objectName:"deepSkyActivitySpinner"
                    Layout.preferredWidth:20;Layout.preferredHeight:20
                    opacity:controller.busy?1:0
                    Canvas {anchors.fill:parent;onPaint:{const ctx=getContext("2d");ctx.reset();ctx.strokeStyle="#8dbaff";ctx.lineWidth=2;ctx.lineCap="round";ctx.beginPath();ctx.arc(10,10,7,0,Math.PI*1.5);ctx.stroke();}}
                    RotationAnimator {target:activitySpinner;from:0;to:360;duration:900;loops:Animation.Infinite;running:controller.busy && root.visible}
                }
                ScrollView {Layout.fillWidth:true;Layout.preferredHeight:52;Layout.minimumHeight:52;Layout.maximumHeight:52;clip:true;Label {width:parent.width;text:controller.status;color:"#a8b5c6";wrapMode:Text.Wrap}}
                IconButton {text:qsTr("Cancel");opacity:controller.busy?1:0;enabled:controller.busy;onClicked:controller.cancel()}
            }
        }
    }
    Timer {id:toastTimer;interval:3500;onTriggered:completionToast.close()}
    Popup {
        id:completionToast;objectName:"deepSkyCompletionToast"
        property bool success:true
        property string message:""
        parent:root
        x:Math.max(12,(root.width-width)/2);y:Math.max(12,root.height-height-80)
        width:Math.min(440,root.width-24);padding:14
        closePolicy:Popup.NoAutoClose
        background:Rectangle {color:"#151b24";radius:8;border.color:completionToast.success?"#4b8065":"#b97b56"}
        contentItem:Label {text:(completionToast.success?"✓  ":"")+completionToast.message;color:"#eef3fa";wrapMode:Text.Wrap;Accessible.name:text}
        enter:Transition {NumberAnimation {property:"opacity";from:0;to:1;duration:150}}
        exit:Transition {NumberAnimation {property:"opacity";from:1;to:0;duration:150}}
    }
    DarkDialog {
        id:presetDialog;objectName:"deepSkyPresetDialog";parent:Overlay.overlay;anchors.centerIn:parent;modal:true
        title:qsTr("Deep-sky presets");width:Math.min(440,root.width-32)
        ColumnLayout {
            width:parent.width;spacing:12
            Label {text:qsTr("Name this finish for your camera, filters or intent. Use a new name to duplicate it. Presets are kept locally across updates.");color:"#b5c3d4";wrapMode:Text.Wrap;Layout.fillWidth:true}
            TextField {id:presetName;objectName:"deepSkyPresetName";Layout.fillWidth:true;maximumLength:80;placeholderText:qsTr("Preset name")}
            Label {text:qsTr("This name exists. Replace overwrites its settings.");color:"#edb06c";visible:root.library.deepSkyPresetNames.indexOf(presetName.text.trim())>=0;wrapMode:Text.Wrap;Layout.fillWidth:true}
            RowLayout {
                Layout.fillWidth:true
                IconButton {text:qsTr("Cancel");onClicked:presetDialog.close()}
                IconButton {text:qsTr("Delete");enabled:root.library.deepSkyPresetNames.indexOf(presetName.text.trim())>=0;onClicked:{root.library.removeDeepSkyPreset(presetName.text.trim());presetDialog.close();}}
                Item {Layout.fillWidth:true}
                IconButton {text:root.library.deepSkyPresetNames.indexOf(presetName.text.trim())>=0?qsTr("Replace"):qsTr("Save");enabled:presetName.text.trim().length>0;onClicked:{if(root.library.saveDeepSkyPreset(presetName.text,edits.values,true))presetDialog.close();}}
            }
        }
    }
    FolderDialog {id:folderDialog;title:qsTr("Import an image folder");onAccepted:controller.addFolder(selectedFolder,root.selectedRole(),group.currentText,Number(aduScale.text))}
    FileDialog {id:inputDialog;title:qsTr("Add deep-sky images");fileMode:FileDialog.OpenFiles;nameFilters:["Astro images (*.fits *.fit *.fts *.fz *.tif *.tiff *.png *.cr2 *.cr3 *.nef *.nrw *.arw *.dng *.orf *.raf *.rw2 *.pef)"];onAccepted:controller.addFiles(selectedFiles,root.importRole,group.currentText,Number(aduScale.text))}
    FileDialog {id:exportDialog;fileMode:FileDialog.SaveFile;onAccepted:{let path=selectedFile.toString();if(!/\.(fits?|fts|tiff?|jpe?g)$/i.test(path))path+=root.linearExport?".fits":(selectedNameFilter.index===1?".jpg":".tif");controller.exportImage(path,root.linearExport);}}
    FileDialog {id:projectOpen;fileMode:FileDialog.OpenFile;nameFilters:["AstroCapture project (*.astroproject)"];onAccepted:controller.loadProject(selectedFile)}
    FileDialog {id:projectSave;fileMode:FileDialog.SaveFile;nameFilters:["AstroCapture project (*.astroproject)"];onAccepted:controller.saveProject(selectedFile.toString().endsWith(".astroproject")?selectedFile:selectedFile.toString()+".astroproject")}
    DarkDialog {id:clearConfirm;parent:Overlay.overlay;anchors.centerIn:parent;modal:true;title:qsTr("Clear this project?");standardButtons:Dialog.Ok|Dialog.Cancel;Label {text:qsTr("Export results or save the project first. Original files remain unchanged.");width:300;wrapMode:Text.Wrap}
        onAccepted:controller.clear()}
}
