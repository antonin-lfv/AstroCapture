import "Atelier.js" as Atelier
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
    property var captionOptions:({})
    property bool shortcutsAllowed: true
    property string editError: ""
    property bool imageFocus: false
    property var advancedOptions: ({})
    readonly property var advancedKeys:["pedestal","ghs", "ghsStrength", "ghsLocal", "ghsSymmetry", "ghsShadows", "ghsHighlights", "ghsInverse", "ghsMode", "maskType", "maskLow", "maskHigh", "maskFeather", "maskX", "maskY", "maskRadius", "maskStarRadius", "maskInvert", "deconvolution", "psfProfile", "psfFwhm", "psfBeta", "rlIterations", "rlDamping", "rlBlend", "rlStopping", "advancedDenoise", "noiseScales", "noiseThreshold", "noiseAmount", "noiseChroma", "photometry", "photometryReferences", "photometryAperture", "photometryInner", "photometryOuter", "photometrySaturation"]
    readonly property bool linearRecipeInput:!operations.some(op=>op.enabled && (op.settings.stretch || op.settings.scnr || op.settings.saturation!==1 || op.settings.rotation || op.settings.mirrorHorizontal || op.settings.mirrorVertical))
    property int finishingTool:0
    readonly property bool compactInspector: width < 1150
    property int inspectorWidth: library.deepSkyPanelWidth
    property bool histogramExpanded: false
    property string comparisonMode: "result"
    property real comparisonSplit: 0.5
    onFinishingToolChanged: if(settingsScroll.contentItem)settingsScroll.contentItem.contentY=0
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
    onWorkflowStepChanged:{frameListOpen=workflowStep!==6;if(workflowStep!==6)comparisonMode="result";if(settingsScroll.contentItem)settingsScroll.contentItem.contentY=0}
    property bool updatingControls: false
    property var pendingProjectFinish: null
    readonly property bool finishDirty: controller.hasResult && (editError.length>0 || Object.keys(edits.values).some(key=>JSON.stringify(edits.values[key])!==JSON.stringify(controller.finishSettings[key])))
    readonly property bool finishEditable: controller.hasResult && (!controller.busy || controller.adjusting)
    function restoreFinish(finish) {
        const previous=updatingControls;updatingControls=true;
        const advanced={};for(const key of root.advancedKeys)advanced[key]=finish[key];root.advancedOptions=advanced;
        operations=finish.operations||[];scnr.checked=finish.scnr||false;scnrMethod.currentIndex=finish.scnrMethod||0;scnrAmount.value=finish.scnrAmount===undefined?1:finish.scnrAmount;scnrPreserve.checked=finish.scnrPreserve===undefined?true:finish.scnrPreserve;
        backgroundPoints=finish.backgroundPoints||[];background.checked=finish.background;stretch.checked=finish.stretch;
        backgroundDegree.currentIndex=(finish.backgroundDegree||1)-1;backgroundSamples.value=finish.backgroundSamples||20;backgroundTolerance.value=finish.backgroundTolerance===undefined?1:finish.backgroundTolerance;
        rotation.currentIndex=finish.rotation||0;mirrorHorizontal.checked=finish.mirrorHorizontal||false;mirrorVertical.checked=finish.mirrorVertical||false;
        black.text=String(finish.black);white.text=String(finish.white);
        midtones.value=finish.midtones;redGain.value=finish.red;greenGain.value=finish.green;blueGain.value=finish.blue;
        asinhLuminance.currentIndex=finish.asinhLuminance||0;denoise.value=finish.denoise;asinh.checked=finish.asinh;asinhStrength.value=finish.asinhStrength;saturation.value=finish.saturation;
        updatingControls=previous;
    }
    function resetFamily() {
        const families=[{stretch:false,black:0,white:1,midtones:.15,asinh:false,asinhStrength:20,asinhLuminance:0,ghs:false,ghsStrength:0,ghsLocal:0,ghsSymmetry:0,ghsShadows:0,ghsHighlights:1,ghsInverse:false,ghsMode:0},
            {background:false,backgroundDegree:1,backgroundSamples:20,backgroundTolerance:1,backgroundPoints:[]},
            {red:1,green:1,blue:1,saturation:1,denoise:0,scnr:false,scnrMethod:0,scnrAmount:1,scnrPreserve:true},
            {rotation:0,mirrorHorizontal:false,mirrorVertical:false}];
        if(root.finishingTool===4) {
            const defaults={pedestal:0,ghs:false,ghsStrength:0,ghsLocal:0,ghsSymmetry:0,ghsShadows:0,ghsHighlights:1,ghsInverse:false,ghsMode:0,maskType:0,maskLow:0,maskHigh:1,maskFeather:0.05,maskX:0.5,maskY:0.5,maskRadius:0.25,maskStarRadius:2,maskInvert:false,deconvolution:false,psfProfile:0,psfFwhm:3,psfBeta:3,rlIterations:10,rlDamping:0.001,rlBlend:1,rlStopping:0.0001,advancedDenoise:false,noiseScales:3,noiseThreshold:3,noiseAmount:0.5,noiseChroma:0.5,photometry:false,photometryReferences:[],photometryAperture:5,photometryInner:8,photometryOuter:12,photometrySaturation:1};
            root.setFinish(Object.assign({},options(),defaults));
        } else root.setFinish(Object.assign({},options(),families[root.finishingTool]));
    }
    function previewAdjustments() {
        if(!finishEditable || updatingControls)return false;
        if(!edits.applyValues(options())){editError=qsTr("Check parameters and operation order: white must exceed black; photometry and deconvolution require linear, unrotated input; photometry cannot be masked.");return false;}
        editError="";
        adjustmentTimer.restart();
        return true;
    }
    function applyStoredFinish() {adjustmentTimer.stop();editError="";controller.apply(edits.values);}
    function setFinish(values) {edits.endEdit();if(!edits.applyValues(values)){editError=qsTr("Check parameters and operation order: white must exceed black; photometry and deconvolution require linear, unrotated input; photometry cannot be masked.");return false;}applyStoredFinish();return true;}
    function undoFinish() {adjustmentTimer.stop();edits.undo();applyStoredFinish();}
    function redoFinish() {adjustmentTimer.stop();edits.redo();applyStoredFinish();}
    function switchVersion(version) {adjustmentTimer.stop();edits.selectVersion(version);applyStoredFinish();}
    Shortcut {sequences:[StandardKey.Undo];enabled:root.visible && root.shortcutsAllowed && !root.dialogOpen && root.finishEditable && edits.canUndo;onActivated:root.undoFinish()}
    Shortcut {sequence:StandardKey.Save;enabled:root.visible && root.shortcutsAllowed && !root.dialogOpen && !controller.busy && controller.frames.length>0;onActivated:if(root.saveOptions())projectSave.open()}
    Shortcut {sequences:[StandardKey.Redo];enabled:root.visible && root.shortcutsAllowed && !root.dialogOpen && root.finishEditable && edits.canRedo;onActivated:root.redoFinish()}
    Timer {id:adjustmentTimer;interval:200;onTriggered:controller.apply(root.options())}
    readonly property bool dialogOpen: inputDialog.visible || folderDialog.visible || exportDialog.visible || projectOpen.visible || projectSave.visible || presetDialog.visible || projectMenu.visible || exportPanel.visible || advancedTools.catalogOpen
    background: Rectangle {color:Atelier.canvas}
    palette.window: Atelier.canvas
    palette.windowText: Atelier.text
    palette.base: Atelier.field
    palette.text: Atelier.text
    palette.button: Atelier.field
    palette.buttonText: Atelier.text
    palette.highlight: Atelier.accent
    palette.highlightedText: Atelier.canvas
    palette.mid: Atelier.border
    palette.light: Atelier.border
    palette.dark: Atelier.surface
    function chooseProject() {if(!controller.busy)projectOpen.open();}
    function importFiles(role) { importRole=role; inputDialog.open(); }
    function saveOptions() {
        if(!edits.applyValues(options())){editError=qsTr("Check parameters and operation order: white must exceed black; photometry and deconvolution require linear, unrotated input; photometry cannot be masked.");return false;}
        edits.endEdit();
        controller.projectOptions={calibrated:calibrated.checked,normalize:normalize.checked,badPixels:badPixels.checked,sequenceBackground:sequenceBackground.checked,sigma:sigma.value,method:integrationMethod.currentIndex,finish:edits.values,caption:root.captionOptions,versions:edits.snapshot()};return true;
    }
    function options() {return Object.assign({},root.advancedOptions,{operations:root.operations,scnr:scnr.checked,scnrMethod:scnrMethod.currentIndex,scnrAmount:scnrAmount.value,scnrPreserve:scnrPreserve.checked,backgroundPoints:root.backgroundPoints,backgroundDegree:backgroundDegree.currentIndex+1,backgroundSamples:backgroundSamples.value,backgroundTolerance:backgroundTolerance.value,rotation:rotation.currentIndex,mirrorHorizontal:mirrorHorizontal.checked,mirrorVertical:mirrorVertical.checked,background: background.checked, stretch: stretch.checked, black:Number(black.text),white:Number(white.text),midtones:midtones.value,red:redGain.value,green:greenGain.value,blue:blueGain.value,denoise:denoise.value,asinh:asinh.checked,asinhStrength:asinhStrength.value,asinhLuminance:asinhLuminance.currentIndex,saturation:saturation.value});}
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
            root.captionOptions=settings.caption||{};
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
        Layout.fillWidth:true;Layout.minimumWidth:0;spacing:8;Layout.topMargin:8;Layout.bottomMargin:2
        Label {text:heading.label;color:Atelier.text;font.pixelSize:13;font.weight:Font.Medium;wrapMode:Text.Wrap}
        Rectangle {Layout.fillWidth:true;Layout.preferredHeight:1;color:Atelier.border}
        HelpIcon {text:heading.explanation;visible:text.length>0}
        Accessible.description: explanation
    }
    component ParameterLabel: RowLayout {
        property string label
        property string explanation
        id:parameter
        Layout.fillWidth:true;Layout.minimumWidth:0;spacing:4
        Label {text:parameter.label;color:Atelier.secondary;Layout.fillWidth:true;wrapMode:Text.Wrap}
        HelpIcon {text:parameter.explanation;visible:text.length>0}
        Accessible.description: explanation
    }

    ColumnLayout {
        id: workspaceHeader
        anchors {top:parent.top;left:parent.left;right:parent.right}
        spacing: 0
        RowLayout {
            Layout.fillWidth:true;Layout.preferredHeight:64;Layout.minimumHeight:64;Layout.maximumHeight:64
            Layout.leftMargin:16;Layout.rightMargin:16;spacing:8
            ColumnLayout {
                Layout.fillWidth:true;spacing:2
                Label {text:root.captionOptions.title || (controller.frames.length===1?controller.frames[0].name.replace(/\.[^/.]+$/, ""):qsTr("Deep sky"));font.pixelSize:16;font.weight:Font.DemiBold;color:Atelier.text;Layout.fillWidth:true;elide:Text.ElideRight}
                Label {text:(controller.frames.length===1?qsTr("1 image"):qsTr("%1 images").arg(controller.frames.length))+(controller.imageWidth>0?" · "+controller.imageWidth+" × "+controller.imageHeight+" px · RGB":"");font.pixelSize:11;color:Atelier.secondary}
            }
            IconAction {objectName:"deepSkyProjectMenu";iconName:"gear";text:qsTr("Project options");hint:qsTr("Project and finishing presets");onClicked:projectMenu.open()}
            IconButton {objectName:"deepSkyExportButton";iconName:"export";text:qsTr("Export");primary:true;enabled:!controller.busy && controller.hasResult;onClicked:exportPanel.open()}
        }
        RowLayout {
            Layout.fillWidth:true;Layout.preferredHeight:42;Layout.minimumHeight:42;Layout.maximumHeight:42
            Layout.leftMargin:16;Layout.rightMargin:16;spacing:8
            Repeater {model:[qsTr("Images"),qsTr("Stack"),qsTr("Finish")]
                delegate:Button {
                    id:workflowTab;required property int index;required property string modelData
                    objectName:index===2?"deepSkyFinishTab":index===1?"deepSkyStackTab":"deepSkyImagesTab"
                    readonly property bool selected:root.workflowStep===[0,1,6][index]
                    enabled:index!==2 || controller.hasResult;implicitHeight:42;implicitWidth:120;text:modelData
                    onClicked:root.workflowStep=[0,1,6][index]
                    contentItem:Label {text:workflowTab.text;color:workflowTab.enabled?(workflowTab.selected?Atelier.text:Atelier.secondary):Atelier.muted;font.pixelSize:13;horizontalAlignment:Text.AlignHCenter;verticalAlignment:Text.AlignVCenter}
                    background:Rectangle {color:workflowTab.hovered?Atelier.toolbar:"transparent";Rectangle {anchors.bottom:parent.bottom;width:parent.width;height:2;color:workflowTab.selected?Atelier.accent:"transparent"} border.width:workflowTab.activeFocus?1:0;border.color:Atelier.accent}
                }
            }
            Item {Layout.fillWidth:true}
            IconAction {objectName:"deepSkySources";iconName:"folder";text:qsTr("Source files");enabled:controller.frames.length>0;visible:root.workflowStep!==6;onClicked:{root.imageFocus=false;root.frameListOpen=!root.frameListOpen}}
        }
        Rectangle {Layout.fillWidth:true;Layout.preferredHeight:1;color:Atelier.border}
    }
    AtelierMenu {
        id:projectMenu;objectName:"deepSkyProjectOptions";parent:root;x:Math.max(12,root.width-width-16);y:60
        AtelierMenuItem {text:qsTr("Open project");enabled:!controller.busy;onTriggered:projectOpen.open()}
        AtelierMenuItem {text:qsTr("Save project");enabled:!controller.busy&&controller.frames.length>0;onTriggered:if(root.saveOptions())projectSave.open()}
        MenuSeparator {}
        AtelierMenuItem {objectName:"deepSkyPresets";text:qsTr("Finishing presets");description:qsTr("All adjustment families");enabled:root.finishEditable;onTriggered:presetDialog.open()}
        AtelierMenuItem {objectName:"deepSkyImageFocus";text:root.imageFocus?qsTr("Show adjustment panel"):qsTr("Enlarge preview");onTriggered:root.imageFocus=!root.imageFocus}
        MenuSeparator {}
        AtelierMenuItem {text:qsTr("Clear project");destructive:true;enabled:!controller.busy&&controller.frames.length>0;onTriggered:clearConfirm.open()}
    }

DropArea {
        anchors.fill: parent; z:100
        enabled: root.visible && !root.controller.busy && !root.dialogOpen
        function accepts(event) {return event.hasUrls && event.urls.length>0 && event.urls.every(url => /^file:/i.test(url.toString()) && /\.(fits?|fts|fz|tiff?|png|cr2|cr3|nef|nrw|arw|dng|orf|raf|rw2|pef)$/i.test(url.toString()));}
        onEntered: drag => {drag.accepted=accepts(drag);}
        onDropped: drop => {if(!accepts(drop))return; controller.addFiles(drop.urls,root.selectedRole(),group.currentText,Number(aduScale.text));drop.acceptProposedAction();}
        Rectangle {anchors.fill:parent;anchors.margins:8;visible:parent.containsDrag;color:"#e6111c2c";border.color:Atelier.accent;radius:8
            Label {anchors.centerIn:parent;text:qsTr("Drop deep-sky light frames");color:Atelier.text;font.pixelSize:22}
        }
    }

    RowLayout {
        anchors {left:parent.left;right:parent.right;top:workspaceHeader.bottom;bottom:parent.bottom}
        spacing:0
        ColumnLayout {
            Layout.fillWidth:true;Layout.fillHeight:true;Layout.minimumWidth:0;spacing:0
            RowLayout {
                visible:!controller.displayingResult && !controller.comparingRaw && controller.imageWidth>0;Layout.fillWidth:true
                Label {text:controller.sourceInterpretation;color:Atelier.secondary;Layout.fillWidth:true;elide:Text.ElideRight}
                RowLayout {Layout.fillWidth:true;CheckBox {text:qsTr("Color preview");checked:controller.sourceDebayer;enabled:!controller.busy;onClicked:controller.sourceDebayer=checked;Layout.fillWidth:true} HelpIcon {text:qsTr("Debayer the preview when Bayer metadata is available. Calibration always uses the original sensor samples.")}}
                RowLayout {Layout.fillWidth:true;CheckBox {text:qsTr("Auto display stretch");checked:controller.sourceAutoStretch;enabled:!controller.busy;onClicked:controller.sourceAutoStretch=checked;Layout.fillWidth:true} HelpIcon {text:qsTr("Only changes the source preview. Disable to view linear pixel brightness.")}}
            }
            ImageViewport {
                id:viewport;objectName:"deepSkyViewport";preserveViewOnSourceResize:root.rawViewState!==null;Layout.fillWidth:true;Layout.fillHeight:true;Layout.minimumHeight:100
                sourceWidth:controller.imageWidth;sourceHeight:controller.imageHeight
                DeepSkyImageItem {parent:viewport.imageContainer;anchors.fill:parent;controller:root.controller}
                Item {parent:viewport.viewingSurface;anchors.fill:parent;clip:true;visible:root.workflowStep===6 && controller.displayingResult && !controller.showingMask && !controller.showingBackgroundModel && root.comparisonMode!=="result"
                    Item {width:root.comparisonMode==="before"?parent.width:parent.width*root.comparisonSplit;height:parent.height;clip:true
                        DeepSkyImageItem {x:viewport.imageContainer.x;y:viewport.imageContainer.y;width:viewport.imageContainer.width;height:viewport.imageContainer.height;scale:viewport.effectiveZoom;transformOrigin:Item.TopLeft;controller:root.controller;beforeFinish:true}
                    }
                    Rectangle {visible:root.comparisonMode==="split";x:parent.width*root.comparisonSplit;width:1;height:parent.height;color:Atelier.text
                        Rectangle {anchors.centerIn:parent;width:24;height:32;color:Atelier.surface;radius:4;border.color:Atelier.border;Label {anchors.centerIn:parent;text:"↔";color:Atelier.text}}
                        MouseArea {anchors.top:parent.top;anchors.bottom:parent.bottom;anchors.horizontalCenter:parent.horizontalCenter;width:28;cursorShape:Qt.SplitHCursor;onPositionChanged:mouse=>{if(pressed){const point=mapToItem(viewport.viewingSurface,mouse.x,mouse.y);root.comparisonSplit=Math.max(.05,Math.min(.95,point.x/viewport.viewingSurface.width));}}}
                    }
                }
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
                Label {anchors.centerIn:parent;visible:controller.imageWidth===0;text:qsTr("Add light frames to begin\nThe preview is automatically stretched; source pixels stay linear.");color:Atelier.secondary;horizontalAlignment:Text.AlignHCenter;wrapMode:Text.Wrap;width:parent.width-40}
            }
            RowLayout {
                Layout.fillWidth:true
                visible:!root.imageFocus && root.frameListOpen
                DarkComboBox {id:frameFilterChoice;objectName:"deepSkyFrameFilter";model:[qsTr("All images")].concat(root.roleNames.slice(1));Layout.preferredWidth:155;onActivated:root.frameFilter=currentIndex===0?"All":root.roles[currentIndex]}
                Label {text:qsTr("%1 / %2 images").arg(root.visibleFrames.length).arg(controller.frames.length);color:Atelier.secondary;Layout.fillWidth:true}
                IconButton {text:qsTr("Select all");enabled:!controller.busy;onClicked:root.visibleFrames.forEach(row=>controller.setSelected(row.sourceIndex,true))}
                IconButton {text:qsTr("Select none");enabled:!controller.busy;onClicked:root.visibleFrames.forEach(row=>controller.setSelected(row.sourceIndex,false))}
            }
            ListView {
                id:frameList;objectName:"deepSkyFrameList";visible:!root.imageFocus && root.frameListOpen;Layout.fillWidth:true;Layout.preferredHeight:Math.min(160,Math.max(64,controller.frames.length*64));clip:true;model:root.visibleFrames
                ScrollBar.vertical:ScrollBar {}
                delegate:Rectangle {
                    required property var modelData
                    required property int index
                    width:frameList.width;height:58;color:index%2?Atelier.surface:Atelier.surface
                    RowLayout {
                        anchors.fill:parent;anchors.margins:4;spacing:5
                        CheckBox {checked:modelData.selected;enabled:!controller.busy;onClicked:controller.setSelected(modelData.sourceIndex,checked)}
                        ColumnLayout {
                            Layout.fillWidth:true;spacing:1
                            Label {text:modelData.name;color:Atelier.text;Layout.fillWidth:true;elide:Text.ElideMiddle;MouseArea {anchors.fill:parent;enabled:!controller.busy;onClicked:controller.inspect(modelData.sourceIndex)}}
                            Label {text:root.roleLabel(modelData.role)+" · "+modelData.group+" · "+Number(modelData.exposure).toFixed(1)+" s · "+modelData.width+"×"+modelData.height+(modelData.stars!==undefined?" · "+modelData.stars+qsTr(" stars")+" · FWHM "+Number(modelData.fwhm||0).toFixed(1):"");color:Atelier.secondary;font.pixelSize:11;Layout.fillWidth:true;elide:Text.ElideRight}
                        }
                        DarkComboBox {model:root.roleNames.slice(1);currentIndex:Math.max(0,root.roles.indexOf(modelData.role)-1);Layout.preferredWidth:110;enabled:!controller.busy;onActivated:controller.setRole(modelData.sourceIndex,root.roles[currentIndex+1])} HelpIcon {text:qsTr("Change image type")}
                        DarkComboBox {model:["Mono","RGB","R","G","B","L","HA","OIII","SII"];currentIndex:Math.max(0,model.indexOf(modelData.group));Layout.preferredWidth:72;enabled:!controller.busy;onActivated:controller.setGroup(modelData.sourceIndex,currentText)} HelpIcon {text:qsTr("Change filter group")}
                        IconButton {text:modelData.reference?qsTr("Alignment reference ✓"):qsTr("Use as reference");borderColor:modelData.reference?Atelier.accent:Atelier.border;visible:modelData.role==="Light";enabled:!controller.busy && modelData.selected;onClicked:controller.setReference(modelData.sourceIndex);Accessible.name:qsTr("Set reference frame");helpText:qsTr("Align the other lights in this group to this image")}
                        IconButton {text:"×";enabled:!controller.busy;onClicked:controller.remove(modelData.sourceIndex);Accessible.name:qsTr("Remove frame")}
                    }
                }
            }
            RowLayout {
                visible:controller.busy;Layout.fillWidth:true
                Item {
                    id: activitySpinner; objectName:"deepSkyActivitySpinner"
                    Layout.preferredWidth:20;Layout.preferredHeight:20;Layout.alignment:Qt.AlignTop;Layout.topMargin:2
                    opacity:controller.busy?1:0
                    Canvas {anchors.fill:parent;onPaint:{const ctx=getContext("2d");ctx.reset();ctx.strokeStyle=Atelier.accent;ctx.lineWidth=2;ctx.lineCap="round";ctx.beginPath();ctx.arc(10,10,7,0,Math.PI*1.5);ctx.stroke();}}
                    RotationAnimator {target:activitySpinner;from:0;to:360;duration:900;loops:Animation.Infinite;running:controller.busy && root.visible}
                }
                ScrollView {Layout.alignment:Qt.AlignTop;Layout.fillWidth:true;Layout.preferredHeight:36;Layout.minimumHeight:36;Layout.maximumHeight:36;clip:true;Label {width:parent.width;text:controller.status;color:Atelier.secondary;wrapMode:Text.Wrap}}
                IconButton {text:qsTr("Cancel");opacity:controller.busy?1:0;enabled:controller.busy;onClicked:controller.cancel()}
            }
        }
        Rectangle {
            visible:!root.imageFocus;Layout.preferredWidth:root.compactInspector?Atelier.compactInspectorWidth:root.inspectorWidth
            Layout.minimumWidth:Layout.preferredWidth;Layout.maximumWidth:Layout.preferredWidth;Layout.fillHeight:true
            color:Atelier.surface
            Rectangle {anchors.left:parent.left;height:parent.height;width:1;color:Atelier.border}
            MouseArea {
                anchors {left:parent.left;top:parent.top;bottom:parent.bottom}
                width:6;z:10;visible:!root.compactInspector;cursorShape:Qt.SplitHCursor
                property real startX:0
                property int startWidth:360
                onPressed:mouse=>{startX=mapToItem(root,mouse.x,mouse.y).x;startWidth=root.inspectorWidth}
                onPositionChanged:mouse=>{if(pressed)root.inspectorWidth=Math.max(320,Math.min(480,Math.round(startWidth+startX-mapToItem(root,mouse.x,mouse.y).x)))}
                onReleased: root.library.deepSkyPanelWidth = root.inspectorWidth
                Accessible.name:qsTr("Resize adjustment panel")
            }

            ColumnLayout {
                anchors.fill:parent;anchors.margins:root.compactInspector?12:16;spacing:12
                ColumnLayout {
                    visible:root.workflowStep===6&&controller.hasResult;Layout.fillWidth:true;spacing:4
                    RowLayout {
                        Layout.fillWidth:true
                        Label {text:qsTr("Input histogram");font.pixelSize:13;font.weight:Font.Medium;color:Atelier.text;Layout.fillWidth:true}
                        IconButton {text:root.histogramExpanded?"⌃":"⌄";ghost:true;visible:root.width<1000;Layout.preferredWidth:28;Accessible.name:qsTr("Show or hide histogram");onClicked:root.histogramExpanded=!root.histogramExpanded}
                    }
                    ColumnLayout {
                        visible:root.width>=1000||root.histogramExpanded;Layout.fillWidth:true;spacing:4
                        Canvas {
                    id:linearHistogram;objectName:"deepSkyLinearHistogram";Layout.fillWidth:true;Layout.preferredHeight:84
                    property var stats:controller.linearHistogram
                    property real blackPoint:Number(black.text)
                    property real whitePoint:Number(white.text)
                    property real midpoint:midtones.value
                    property real zoom:histogramZoom.value
                    property bool useAsinh:asinh.checked
                    property bool useGhs:root.advancedOptions.ghs||false
                    property var ghsPoints:controller.ghsCurve(root.advancedOptions)
                    onUseGhsChanged:requestPaint();onGhsPointsChanged:requestPaint()
                    property real strength:asinhStrength.value
                    property real low:stats.minimum===undefined?0:stats.minimum
                    property real high:low+(Math.max(stats.maximum||1,whitePoint)-low)/zoom
                    onStatsChanged:requestPaint();onBlackPointChanged:requestPaint();onWhitePointChanged:requestPaint();onMidpointChanged:requestPaint();onZoomChanged:requestPaint();onUseAsinhChanged:requestPaint();onStrengthChanged:requestPaint()
                    onWidthChanged:requestPaint();onHeightChanged:requestPaint()
                    function pointX(value) {return Math.max(0,Math.min(width,(value-low)/(high-low)*width));}
                    onPaint: {
                        const ctx=getContext("2d");ctx.clearRect(0,0,width,height);ctx.fillStyle=Atelier.surface;ctx.fillRect(0,0,width,height);
                        const channels=stats.channels||[],colors=["#cf6060","#60ba80","#729dde"];
                        for(let c=0;c<channels.length;c++) {
                            const bins=channels[c];let peak=1;for(let i=0;i<bins.length;i++)peak=Math.max(peak,bins[i]);
                            ctx.strokeStyle=colors[c];ctx.beginPath();
                            for(let i=0;i<bins.length;i++){const value=low+(stats.maximum-low)*i/(bins.length-1);if(value>high)break;const x=(value-low)/(high-low)*width,y=height-15-(height-20)*Math.log(1+bins[i])/Math.log(1+peak);if(i===0)ctx.moveTo(x,y);else ctx.lineTo(x,y);}ctx.stroke();
                        }
                        if(whitePoint>blackPoint) {
                            ctx.strokeStyle=Atelier.text;ctx.beginPath();
                            for(let x=0;x<=width;x++){const value=low+x/width*(high-low),v=Math.max(0,Math.min(1,(value-blackPoint)/(whitePoint-blackPoint)));const out=useGhs&&ghsPoints.length>0?ghsPoints[Math.min(512,Math.round(v*512))]:useAsinh?Math.asinh(strength*v)/Math.asinh(strength):((midpoint-1)*v)/((2*midpoint-1)*v-midpoint);const y=height-15-(height-20)*out;if(x===0)ctx.moveTo(x,y);else ctx.lineTo(x,y);}ctx.stroke();
                        }
                        const points=[blackPoint,blackPoint+midpoint*(whitePoint-blackPoint),whitePoint],labels=["B","M","W"];
                        for(let i=0;i<3;i++){if(i===1&&(useAsinh||useGhs))continue;const x=pointX(points[i]);ctx.fillStyle=i===histogramControl.currentIndex?Atelier.accent:Atelier.text;ctx.fillRect(Math.max(0,Math.min(width-2,x)),0,2,height-15);ctx.fillText(labels[i],Math.max(2,Math.min(width-10,x)),height-2);}
                    }
                    MouseArea {
                        anchors.fill:parent;enabled:root.finishEditable
                        function changePoint(x) {
                            const value=linearHistogram.low+Math.max(0,Math.min(1,x/width))*(linearHistogram.high-linearHistogram.low);
                            if(histogramControl.currentIndex===0)black.text=Math.min(value,Number(white.text)-1e-6).toPrecision(7);
                            else if(histogramControl.currentIndex===2)white.text=Math.max(value,Number(black.text)+1e-6).toPrecision(7);
                            else if(!asinh.checked&&!root.advancedOptions.ghs)midtones.value=Math.max(.001,Math.min(.999,(value-Number(black.text))/(Number(white.text)-Number(black.text))));
                            root.previewAdjustments();linearHistogram.requestPaint();
                        }
                        onPressed:mouse=>{edits.beginEdit();changePoint(mouse.x);}
                        onReleased:edits.endEdit()
                        onCanceled:edits.endEdit()
                        onPositionChanged:mouse=>{if(pressed)changePoint(mouse.x);}
                    }
                }
                        RowLayout {Layout.fillWidth:true
                            DarkComboBox {id:histogramControl;objectName:"deepSkyHistogramControl";model:[qsTr("Black"),qsTr("Midtones"),qsTr("White")];Layout.fillWidth:true;onCurrentIndexChanged:linearHistogram.requestPaint();enabled:root.finishEditable}
                            Label {text:qsTr("Zoom");font.pixelSize:11;color:Atelier.muted}
                            DarkSpinBox {id:histogramZoom;from:1;to:50;value:1;Layout.preferredWidth:68}
                        }
                        Label {text:qsTr("Shadows %1% · Highlights %2%").arg(Number(controller.linearHistogram.blackLoss||0).toFixed(2)).arg(Number(controller.linearHistogram.whiteLoss||0).toFixed(2));color:Atelier.muted;font.pixelSize:11;Layout.fillWidth:true;wrapMode:Text.Wrap
                            DarkToolTip {visible:lossHover.hovered;text:qsTr("Shadow / highlight estimates at the level thresholds, after background correction and gains. These are channel samples, not the fraction of whole pixels lost. Later color operations may also clip.")}
                            HoverHandler {id:lossHover}
                        }
                    }
                }
                DarkComboBox {objectName:"deepSkyFinishFamily";visible:root.workflowStep===6&&root.compactInspector;Layout.fillWidth:true;model:[qsTr("Stretch"),qsTr("Background"),qsTr("Color"),qsTr("Geometry"),qsTr("Advanced transformations")];currentIndex:root.finishingTool;onActivated:root.finishingTool=currentIndex;Accessible.name:qsTr("Adjustment family")}
                Item {id:genericSettingsSlot;visible:root.workflowStep!==6;Layout.fillWidth:true;Layout.fillHeight:true;Layout.minimumHeight:0}
                ColumnLayout {visible:root.workflowStep===6;Layout.fillWidth:true;Layout.fillHeight:true;Layout.minimumHeight:0;spacing:0
                    InspectorFamily {id:family0;title:qsTr("Stretch");expanded:root.finishingTool===0;visible:!root.compactInspector||expanded;headerVisible:!root.compactInspector;onActivated:root.finishingTool=0}
                    InspectorFamily {id:family1;title:qsTr("Background");expanded:root.finishingTool===1;visible:!root.compactInspector||expanded;headerVisible:!root.compactInspector;onActivated:root.finishingTool=1}
                    InspectorFamily {id:family2;title:qsTr("Color");expanded:root.finishingTool===2;visible:!root.compactInspector||expanded;headerVisible:!root.compactInspector;onActivated:root.finishingTool=2}
                    InspectorFamily {id:family3;title:qsTr("Geometry");expanded:root.finishingTool===3;visible:!root.compactInspector||expanded;headerVisible:!root.compactInspector;onActivated:root.finishingTool=3}
                    InspectorFamily {id:family4;title:qsTr("Advanced transformations");expanded:root.finishingTool===4;visible:!root.compactInspector||expanded;headerVisible:!root.compactInspector;onActivated:root.finishingTool=4}
                }
                ColumnLayout {visible:root.workflowStep===6;Layout.fillWidth:true;spacing:8
                    RowLayout {Layout.fillWidth:true
                        Label {text:qsTr("Comparison");color:Atelier.text;font.weight:Font.Medium;Layout.fillWidth:true}
                        Label {text:"⌘ Z";font.pixelSize:11;color:Atelier.muted}
                        IconAction {text:qsTr("Compare a source exposure");iconName:"folder";onClicked:rawComparePopup.open()}
                    }
                    RowLayout {Layout.fillWidth:true;spacing:2;enabled:controller.hasFinishComparison&&!controller.busy&&controller.displayingResult&&!controller.showingMask&&!controller.showingBackgroundModel
                        Repeater {model:[{key:"before",label:qsTr("Before")},{key:"result",label:qsTr("Result")},{key:"split",label:qsTr("Split")}]
                            delegate:IconButton {required property var modelData;objectName:"deepSkyCompare_"+modelData.key;text:modelData.label;ghost:root.comparisonMode!==modelData.key;Layout.fillWidth:true;borderColor:root.comparisonMode===modelData.key?Atelier.border:"transparent";onClicked:root.comparisonMode=modelData.key;Accessible.description:modelData.key==="before"?qsTr("Stack before finishing, displayed without stretch"):""}
                        }
                    }
                    IconButton {objectName:"deepSkyResetFamily";text:qsTr("Delete these adjustments");Layout.fillWidth:true;enabled:root.finishEditable;active:true;onClicked:root.resetFamily();Accessible.description:qsTr("Reset the selected family. Cmd+Z restores its settings.")}
                }
            }
        }
    }
    ScrollView {
        id:settingsScroll;objectName:"deepSkyInspector"
        parent:root.workflowStep===6?[family0.body,family1.body,family2.body,family3.body,family4.body][root.finishingTool]:genericSettingsSlot
        anchors.fill:parent;padding:0;clip:true;contentWidth:availableWidth
        ScrollBar.horizontal.policy:ScrollBar.AlwaysOff
        ColumnLayout {
            width:settingsScroll.availableWidth;spacing:8

                ColumnLayout {
                visible:root.workflowStep===0;Layout.fillWidth:true;Layout.minimumWidth:0
                SectionTitle {label:qsTr("Import");explanation:qsTr("Add individual files or import a folder and its subfolders. Automatic detection uses FITS metadata and file/folder names. Long offsets are classified as dark flats. Check and edit the roles in the image list.")}
                ParameterLabel {label:qsTr("Image type");explanation:qsTr("Lights contain the object. Darks correct thermal signal. Flats correct dust and uneven illumination. Bias or dark flats correct the flats. Automatic detection can be overridden for each image.")}
                DarkComboBox {id:rolePicker;objectName:"deepSkyImportRole";Layout.fillWidth:true;Layout.minimumWidth:0;model:root.roleNames;enabled:!controller.busy}
                IconButton {text:qsTr("Add files");Layout.fillWidth:true;Layout.minimumWidth:0;enabled:!controller.busy;onClicked:root.importFiles(root.selectedRole())}
                IconButton {text:qsTr("Import folder");Layout.fillWidth:true;Layout.minimumWidth:0;enabled:!controller.busy;onClicked:folderDialog.open()}
                Label {text:qsTr("Drop files into this workspace, or import a complete folder. Originals stay unchanged.");color:Atelier.secondary;wrapMode:Text.Wrap;Layout.fillWidth:true;Layout.minimumWidth:0;font.pixelSize:11}
                SectionTitle {label:qsTr("Import settings")}
                ParameterLabel {label:qsTr("Filter / group");explanation:qsTr("Auto reads the FITS filter. R, G, B, L, HA, OIII and SII are integrated separately. Color-camera data normally belongs to RGB. You can change each image group in the list.")}
                DarkComboBox {id:group;objectName:"deepSkyImportGroup";Layout.fillWidth:true;Layout.minimumWidth:0;model:["Auto","Mono","RGB","R","G","B","L","HA","OIII","SII"];enabled:!controller.busy}
                ParameterLabel {label:qsTr("ADU scale");explanation:qsTr("Leave 0 for automatic import. Override only when you know the numeric scale of a scientific FITS. Use the same convention for lights and calibration images.")}
                TextField {id:aduScale;Layout.fillWidth:true;Layout.minimumWidth:0;text:"0";enabled:!controller.busy;validator:DoubleValidator {bottom:0;top:1e12;locale:"C"}}

                }
                ColumnLayout {
                visible:root.workflowStep===1;Layout.fillWidth:true;Layout.minimumWidth:0
                SectionTitle {label:qsTr("Calibration");explanation:qsTr("Raw images need matching darks and flats. A raw dark already includes bias. Dark exposure, gain and known temperature must match. For the Siril tutorial, import Offsets_3s as Dark flats.")}
                ParameterLabel {label:qsTr("Input calibration");explanation:qsTr("Enable Lights already calibrated only when the files have already been preprocessed, for example calibrated iTelescope FITS. In that mode the imported calibration files are not applied.")}
                CheckBox {id:calibrated;objectName:"deepSkyCalibrated";text:qsTr("Lights already calibrated");checked:false;enabled:!controller.busy;Layout.fillWidth:true}
                Label {text:calibrated.checked?qsTr("Already calibrated"):qsTr("Raw images · calibration enabled");color:Atelier.accent;Layout.fillWidth:true;font.pixelSize:11}
                CheckBox {Layout.fillWidth:true;Layout.minimumWidth:0;implicitWidth:settingsScroll.availableWidth;contentItem:Text {text:parent.text;font:parent.font;color:Atelier.text;wrapMode:Text.Wrap;leftPadding:parent.indicator.width+parent.spacing;verticalAlignment:Text.AlignVCenter} id:badPixels;text:qsTr("Correct bad pixels");checked:true;enabled:!controller.busy && !calibrated.checked}
                ParameterLabel {label:qsTr("Bad pixels");explanation:qsTr("Use an imported map or detect isolated hot pixels in the master dark. The correction runs before demosaicing and preserves the Bayer pattern.")}
                IconButton {text:qsTr("Bad pixel map");Layout.fillWidth:true;Layout.minimumWidth:0;enabled:!controller.busy;onClicked:root.importFiles("BadPixels")}
                }
                Rectangle {
                    id:advancedPanel;objectName:"deepSkyAdvancedPanel";visible:root.workflowStep===1
                    Layout.fillWidth:true;Layout.minimumWidth:0;implicitHeight:advancedContents.implicitHeight+24
                    color:Atelier.surface;radius:8;border.color:Atelier.border
                    ColumnLayout {
                        id:advancedContents;anchors.left:parent.left;anchors.right:parent.right;anchors.top:parent.top;anchors.margins:12;spacing:12
                        ToolButton {
                            id:advanced;objectName:"deepSkyAdvanced";property bool expanded:false
                            Layout.fillWidth:true;implicitHeight:36;padding:0
                            contentItem:RowLayout {
                                Label {text:qsTr("Advanced stack settings");color:Atelier.text;font.pixelSize:16;font.bold:true;Layout.fillWidth:true;wrapMode:Text.Wrap}
                                Label {text:advanced.expanded?"▾":"▸";color:Atelier.muted;font.pixelSize:18}
                            }
                            background:Item {}
                            onClicked:expanded=!expanded
                        }
                        ColumnLayout {
                            visible:advanced.expanded;Layout.fillWidth:true;spacing:12
                            Label {text:qsTr("Normalization, stacking method and rejection threshold.");color:Atelier.secondary;font.pixelSize:12;wrapMode:Text.Wrap;Layout.fillWidth:true}
                            ParameterLabel {label:qsTr("Normalization");explanation:qsTr("Match stellar flux and background offsets to the reference. Disable for data normalized externally. This is global normalization, not photometric color calibration.")}
                            CheckBox {id:normalize;text:qsTr("Normalize to the reference");checked:true;enabled:!controller.busy;Layout.fillWidth:true}
                            ParameterLabel {label:qsTr("Gradient before stacking");explanation:qsTr("Optional degree-1 subtraction after calibration and debayering, before alignment. Each exposure gets its own automatic grid (20 samples per line, tolerance 1). Leave disabled for fields filled with nebulosity.")}
                            CheckBox {id:sequenceBackground;objectName:"deepSkySequenceBackground";text:qsTr("Correct each calibrated exposure");enabled:!controller.busy;Layout.fillWidth:true}
                            Rectangle {Layout.fillWidth:true;Layout.preferredHeight:1;color:Atelier.border}
                            ParameterLabel {label:qsTr("Integration");explanation:qsTr("Robust rejection requires at least three accepted lights. Lower sigma rejects more aggressively. Mean without rejection keeps all usable samples. Borders are cropped to common coverage.")}
                            DarkComboBox {id:integrationMethod;Layout.fillWidth:true;Layout.minimumWidth:0;model:[qsTr("Mean with robust rejection"),qsTr("Mean without rejection")];enabled:!controller.busy}
                            RowLayout {
                                Layout.fillWidth:true
                                ParameterLabel {label:qsTr("Rejection σ");explanation:qsTr("At each aligned pixel and channel, estimate spread with 1.4826 × MAD around the median. Reject values farther than σ times that spread, then average the survivors. This removes isolated trails and defects. Too low a threshold can reject real signal. Start at 3; at least three accepted lights are needed.")}
                                SpinBox {id:sigma;from:1;to:10;value:3;enabled:!controller.busy && integrationMethod.currentIndex===0}
                            }
                        }
                    }
                }
                ParameterLabel {visible:root.workflowStep===1;label:qsTr("Reference and alignment");explanation:qsTr("Reference selects one light per filter group. Other frames are aligned to its stars. Choose a sharp frame with many recognizable stars; this is not the frame used as the final output.")}
                Label {visible:root.workflowStep===1;text:qsTr("Check the stars, FWHM and reference in the image list before stacking.");color:Atelier.secondary;font.pixelSize:12;wrapMode:Text.Wrap;Layout.fillWidth:true}
                Rectangle {objectName:"deepSkyStackDivider";visible:root.workflowStep===1;Layout.fillWidth:true;Layout.preferredHeight:1;color:"#526174";Layout.topMargin:8;Layout.bottomMargin:8}
                IconButton {visible:root.workflowStep===1;objectName:"deepSkyIntegrate";text:qsTr("Create the stacked image");Layout.fillWidth:true;Layout.minimumWidth:0;enabled:!controller.busy && controller.frames.some(row=>row.selected && row.role==="Light");onClicked:controller.integrate(calibrated.checked,integrationMethod.currentIndex===0?sigma.value:0,normalize.checked,badPixels.checked,sequenceBackground.checked)}

            ColumnLayout {visible:root.workflowStep===6;Layout.fillWidth:true;spacing:8
                Label {text:root.editError;color:Atelier.warning;visible:text.length>0;wrapMode:Text.Wrap;Layout.fillWidth:true}
                ColumnLayout {visible:root.finishingTool===0;Layout.fillWidth:true
                CheckBox {Layout.fillWidth:true;Layout.minimumWidth:0;implicitWidth:settingsScroll.availableWidth;contentItem:Text {text:parent.text;font:parent.font;color:Atelier.text;wrapMode:Text.Wrap;leftPadding:parent.indicator.width+parent.spacing;verticalAlignment:Text.AlignVCenter} id:stretch;onClicked:{if(!checked)scnr.checked=false;root.previewAdjustments();}text:qsTr("Enable stretch");checked:true;enabled:root.finishEditable}
                IconButton {text:qsTr("Automatic stretch");Layout.fillWidth:true;enabled:root.finishEditable;onClicked:{const suggested=controller.suggestedStretch;root.setFinish(Object.assign({},root.options(),{stretch:true,black:suggested.black,white:suggested.white,midtones:suggested.midtones,asinh:false}));}}
RowLayout {Layout.fillWidth:true
 Label {text:qsTr("Method");color:Atelier.secondary;Layout.fillWidth:true}
 IconButton {text:"MTF";ghost:asinh.checked;borderColor:!asinh.checked?Atelier.border:"transparent";enabled:root.finishEditable&&!root.advancedOptions.ghs;onClicked:{asinh.checked=false;root.previewAdjustments()}}
 IconButton {text:"Asinh";ghost:!asinh.checked;borderColor:asinh.checked?Atelier.border:"transparent";enabled:root.finishEditable&&!root.advancedOptions.ghs;onClicked:{asinh.checked=true;root.previewAdjustments()}}
}
IconButton {visible:!!root.advancedOptions.ghs;text:qsTr("GHS · advanced transformations");Layout.fillWidth:true;onClicked:root.finishingTool=4}
ParameterControl {id:black;label:qsTr("Black level");textIsValue:true;text:"0";from:Math.min(0,Number(controller.linearHistogram.minimum||0));to:Math.max(1,Number(controller.linearHistogram.maximum||1));inputMinimum:-1e12;inputMaximum:1e12;decimals:4;explanation:qsTr("With stretch enabled, each channel is mapped to u = clamp((gain × pixel − black) / (white − black), 0, 1). Values at or below black become 0. Raising black darkens the sky but discards faint signal below that threshold. Check shadow clipping before applying.");enabled:root.finishEditable;onMoved:root.previewAdjustments();onGestureStarted:edits.beginEdit();onGestureEnded:edits.endEdit()}
                CheckBox {id:asinh;visible:false;enabled:root.finishEditable && stretch.checked}
                ParameterLabel {visible:asinh.checked&&!root.advancedOptions.ghs;label:qsTr("Asinh luminance");explanation:qsTr("L is either max(R,G,B), (R+G+B)/3, or 0.2126R + 0.7152G + 0.0722B. This determines the shared asinh factor. Channel ratios are preserved by that factor, but earlier level clipping or later display/export clipping can change colors. Weighted and mean modes can produce values above 1.")}
                DarkComboBox {id:asinhLuminance;visible:asinh.checked&&!root.advancedOptions.ghs;Layout.fillWidth:true;model:[qsTr("Maximum channel"),qsTr("Human weighted"),qsTr("RGB mean")];enabled:root.finishEditable;onActivated:root.previewAdjustments()}

                ParameterControl {id:asinhStrength;label:qsTr("Asinh strength");decimals:1;explanation:qsTr("From normalized RGB, compute luminance L and apply the same factor to all channels: f(L)/L, with f(L) = asinh(kL)/asinh(k). Larger k reveals fainter detail and compresses bright regions. It also reveals noise. Asinh replaces the midtones curve.");onMoved:root.previewAdjustments();onGestureStarted:edits.beginEdit();onGestureEnded:edits.endEdit();visible:asinh.checked&&!root.advancedOptions.ghs;Layout.fillWidth:true;Layout.minimumWidth:0;from:1;to:1000;value:20;enabled:root.finishEditable}

                ParameterControl {id:midtones;label:qsTr("Midtones");decimals:4;explanation:qsTr("For normalized brightness u, the curve is f(u) = (m − 1)u / ((2m − 1)u − m). At m = 0.5 it is neutral; below 0.5 it brightens midtones, above 0.5 it darkens them. The endpoints stay 0 and 1. Brightening also reveals noise; it does not recover clipped data.");onMoved:root.previewAdjustments();onGestureStarted:edits.beginEdit();onGestureEnded:edits.endEdit();visible:!asinh.checked&&!root.advancedOptions.ghs;Layout.fillWidth:true;Layout.minimumWidth:0;from:.001;to:.999;value:.15;enabled:root.finishEditable}
                ParameterControl {id:white;label:qsTr("White level");textIsValue:true;text:"1";from:Math.min(0,Number(controller.linearHistogram.minimum||0));to:Math.max(1,Number(controller.linearHistogram.maximum||1));inputMinimum:-1e12;inputMaximum:1e12;decimals:4;explanation:qsTr("With stretch enabled, white sets the value mapped to 1 after channel gain. Values at or above it merge into the same maximum: bright cores lose detail. Lowering white brightens the image but increases highlight clipping. White must remain above black.");enabled:root.finishEditable;onMoved:root.previewAdjustments();onGestureStarted:edits.beginEdit();onGestureEnded:edits.endEdit()}
}
                ColumnLayout {visible:root.finishingTool===1;Layout.fillWidth:true
                SectionTitle {label:qsTr("Background");explanation:qsTr("The optional correction fits a polynomial independently in each color channel. Bright grid samples are rejected using the tolerance. Compare with correction disabled to protect extended nebulosity. Enable sample editing to add or remove points directly on the image.")}
                CheckBox {Layout.fillWidth:true;Layout.minimumWidth:0;implicitWidth:settingsScroll.availableWidth;contentItem:Text {text:parent.text;font:parent.font;color:Atelier.text;wrapMode:Text.Wrap;leftPadding:parent.indicator.width+parent.spacing;verticalAlignment:Text.AlignVCenter} id:background;onClicked:root.previewAdjustments();text:qsTr("Correct background gradient");enabled:root.finishEditable}
                RowLayout {Layout.fillWidth:true;CheckBox {text:qsTr("Show extracted background model");checked:controller.showingBackgroundModel;enabled:!controller.busy && controller.hasBackgroundModel && rotation.currentIndex===0 && !mirrorHorizontal.checked && !mirrorVertical.checked;onClicked:controller.showingBackgroundModel=checked;Layout.fillWidth:true} HelpIcon {text:qsTr("Displays the fitted background with an automatic display stretch. Export always uses the processed image. Inspect the model for traces of real astronomical objects.")}}
                CheckBox {text:qsTr("Edit background samples on image");checked:root.editingBackgroundSamples;enabled:root.finishEditable && background.checked;onClicked:{root.editingBackgroundSamples=checked;if(checked){rotation.currentIndex=0;mirrorHorizontal.checked=false;mirrorVertical.checked=false;root.previewAdjustments();}}}
                Label {visible:root.editingBackgroundSamples;text:qsTr("Left click adds a sky sample. Right click removes one. Avoid stars and nebulosity. Rotate and mirror are temporarily unavailable while editing samples.");color:Atelier.secondary;wrapMode:Text.Wrap;Layout.fillWidth:true}
                IconButton {visible:background.checked;text:qsTr("Regenerate automatic samples");Layout.fillWidth:true;onClicked:{root.backgroundPoints=[];root.previewAdjustments()}}
                GridLayout {
                    columns:2;Layout.fillWidth:true;visible:background.checked;enabled:root.finishEditable
                    ParameterLabel {label:qsTr("Polynomial degree");explanation:qsTr("Fit a surface P(x,y) independently in R, G and B from sky samples. Degree 1 is a plane; degree 2 adds curvature. Degrees 1–4 use 3, 6, 10 or 15 coefficients per channel. Correction is image − P + the sample median. Higher degrees can mistake extended nebulosity for background.")}
                    DarkComboBox {id:backgroundDegree;objectName:"deepSkyBackgroundDegree";model:["1","2","3","4"];onActivated:root.previewAdjustments()}
                    ParameterLabel {label:qsTr("Samples per line");explanation:qsTr("An N by N grid proposes up to N² sky samples. Each sample measures local median brightness rather than a single pixel, reducing sensitivity to isolated stars. More points constrain the fit, but points on nebulae contaminate it. Remove those points on the image. This setting regenerates the automatic grid, not a manual selection.")}
                    SpinBox {id:backgroundSamples;from:2;to:50;value:20;onValueModified:root.previewAdjustments()}

                    ParameterControl {id:backgroundTolerance;label:qsTr("Grid tolerance");decimals:2;explanation:qsTr("Automatic samples are excluded when their brightness exceeds median + tolerance × MAD in any channel. MAD is the median absolute distance from the median. Lower tolerance excludes more bright regions; higher tolerance retains more points, including possible objects. Manually selected samples bypass this automatic rejection.");Layout.fillWidth:true;from:0;to:10;value:1;onMoved:root.previewAdjustments()}
                }
                }
                ColumnLayout {visible:root.finishingTool===2;Layout.fillWidth:true
                SectionTitle {label:qsTr("Color and noise")}

                ParameterControl {id:redGain;label:qsTr("Red gain");decimals:3;explanation:qsTr("Multiply the red channel before stretching. Initial gains estimate a neutral background; adjust manually for your intended colors.");onMoved:root.previewAdjustments();onGestureStarted:edits.beginEdit();onGestureEnded:edits.endEdit();Layout.fillWidth:true;Layout.minimumWidth:0;from:.1;to:3;value:1;enabled:root.finishEditable}

                ParameterControl {id:greenGain;label:qsTr("Green gain");decimals:3;explanation:qsTr("Multiply the green channel before stretching. This is not SCNR or photometric calibration.");onMoved:root.previewAdjustments();onGestureStarted:edits.beginEdit();onGestureEnded:edits.endEdit();Layout.fillWidth:true;Layout.minimumWidth:0;from:.1;to:3;value:1;enabled:root.finishEditable}

                ParameterControl {id:blueGain;label:qsTr("Blue gain");decimals:3;explanation:qsTr("Multiply the blue channel before stretching. Watch the background and stellar colors while adjusting.");onMoved:root.previewAdjustments();onGestureStarted:edits.beginEdit();onGestureEnded:edits.endEdit();Layout.fillWidth:true;Layout.minimumWidth:0;from:.1;to:3;value:1;enabled:root.finishEditable}

                ParameterControl {id:saturation;label:qsTr("Saturation");decimals:2;explanation:qsTr("Each channel moves around luminance Y: output = Y + (channel − Y) × effective saturation. 1 is neutral; above 1 increases color differences. Protection leaves Y ≤ 0.03 unchanged and fades out by Y = 0.15. Thus 0 removes color only outside protected shadows. Strong saturation may clip colors when stretch is enabled.");onMoved:root.previewAdjustments();onGestureStarted:edits.beginEdit();onGestureEnded:edits.endEdit();Layout.fillWidth:true;Layout.minimumWidth:0;from:0;to:3;value:1;enabled:root.finishEditable}
                SectionTitle {label:qsTr("Green noise reduction");explanation:qsTr("Optional SCNR on the stretched image, after background and color balance. Avoid for intentional green colors in comets, planetary nebulae or SHO palettes. This is not photometric calibration.")}
                CheckBox {id:scnr;objectName:"deepSkyScnr";text:qsTr("Remove green noise");enabled:root.finishEditable && (stretch.checked || root.operations.some(op=>op.enabled && op.settings.stretch));onClicked:root.previewAdjustments()}
                ColumnLayout {visible:scnr.checked;Layout.fillWidth:true
                    DarkComboBox {id:scnrMethod;Layout.fillWidth:true;model:[qsTr("Average neutral protection"),qsTr("Maximum neutral protection"),qsTr("Maximum mask protection"),qsTr("Additive mask protection")];onActivated:root.previewAdjustments()}

                    ParameterControl {id:scnrAmount;label:qsTr("Amount");decimals:2;explanation:qsTr("Neutral methods set G to min(G,(R+B)/2) or min(G,max(R,B)). Mask methods use G × [1 − amount × (1 − M)], where M is max(R,B) or min(1,R+B). Amount 0 is neutral; 1 applies full mask suppression. This targets green dominance, not all noise; genuine green objects can lose color.");visible:scnrMethod.currentIndex>=2;Layout.fillWidth:true;from:0;to:1;value:1;onMoved:root.previewAdjustments();onGestureStarted:edits.beginEdit();onGestureEnded:edits.endEdit()}
                    CheckBox {id:scnrPreserve;text:qsTr("Preserve CIE lightness");checked:true;onClicked:root.previewAdjustments()}
                }

                ParameterControl {id:denoise;label:qsTr("Noise smoothing");decimals:2;explanation:qsTr("Output = (1 − amount) × image + amount × GaussianBlur(image), with a Gaussian standard deviation of 0.7 pixel. At 0 nothing changes; at 1 the full blur is used. It reduces small-scale fluctuations but also softens stars and fine structures. Inspect at 100% and start low. This is not an adaptive denoiser.");onMoved:root.previewAdjustments();onGestureStarted:edits.beginEdit();onGestureEnded:edits.endEdit();Layout.fillWidth:true;Layout.minimumWidth:0;from:0;to:1;value:0;enabled:root.finishEditable}
                }
                ColumnLayout {visible:root.finishingTool===3;Layout.fillWidth:true;enabled:!root.editingBackgroundSamples
                SectionTitle {label:qsTr("Orientation");explanation:qsTr("Rotate clockwise, then mirror the finished image. The original linear integration remains unchanged. Orientation is saved with the project and can be undone.")}
                DarkComboBox {id:rotation;objectName:"deepSkyRotation";Layout.fillWidth:true;model:[qsTr("Original orientation"),"90°","180°","270°"];enabled:root.finishEditable;onActivated:root.previewAdjustments()}
                CheckBox {id:mirrorHorizontal;text:qsTr("Mirror horizontally");enabled:root.finishEditable;onClicked:root.previewAdjustments()}
                CheckBox {id:mirrorVertical;text:qsTr("Mirror vertically");enabled:root.finishEditable;onClicked:root.previewAdjustments()}
                }
                ColumnLayout {Layout.fillWidth:true;Layout.minimumWidth:0;visible:controller.groups.length>1 && root.finishingTool===2
                SectionTitle {label:qsTr("Channel composition");explanation:qsTr("Assign R/G/B or a narrowband palette. RGB uses R/G/B, SHO uses SII/HA/OIII and HOO uses HA/OIII/OIII. Optional luminance adds detail. Each chosen filter must have an integrated result.")}
                DarkComboBox {id:resultGroup;Layout.fillWidth:true;Layout.minimumWidth:0;model:controller.groups;enabled:!controller.busy && count>0;onActivated:controller.selectResult(currentText)}
                Label {text:qsTr("Combine integrated filters: RGB = R/G/B, SHO = SII/HA/OIII, HOO = HA/OIII/OIII.");color:Atelier.secondary;wrapMode:Text.Wrap;Layout.fillWidth:true;Layout.minimumWidth:0;font.pixelSize:11}
                GridLayout {
                    columns:2;Layout.fillWidth:true;Layout.minimumWidth:0;enabled:!controller.busy && controller.groups.length>0
                    Label {text:qsTr("Red");color:Atelier.secondary}
                    DarkComboBox {id:redChannel;Layout.fillWidth:true;Layout.minimumWidth:0;model:controller.groups}
                    Label {text:qsTr("Green");color:Atelier.secondary}
                    DarkComboBox {id:greenChannel;Layout.fillWidth:true;Layout.minimumWidth:0;model:controller.groups}
                    Label {text:qsTr("Blue");color:Atelier.secondary}
                    DarkComboBox {id:blueChannel;Layout.fillWidth:true;Layout.minimumWidth:0;model:controller.groups}
                    Label {text:qsTr("Luminance");color:Atelier.secondary}
                    DarkComboBox {id:luminanceChannel;Layout.fillWidth:true;Layout.minimumWidth:0;model:[qsTr("None")].concat(controller.groups)}
                }
                IconButton {text:qsTr("Combine channels");Layout.fillWidth:true;Layout.minimumWidth:0;enabled:!controller.busy && controller.groups.length>0;onClicked:controller.compose(redChannel.currentText,greenChannel.currentText,blueChannel.currentText,luminanceChannel.currentIndex>0?luminanceChannel.currentText:"")}
                }
                DeepSkyAdvancedTools {
        Layout.fillWidth:true;visible:root.finishingTool===4
        id:advancedTools;controller:root.controller;values:root.advancedOptions;editable:root.finishEditable;linearInput:root.linearRecipeInput;draftError:root.editError;draftPending:root.finishDirty;livePreview:true
        onPreviewRequested:if(root.previewAdjustments())root.applyStoredFinish()
        onEdited:settings=>{const enablingGhs=settings.ghs&&!root.advancedOptions.ghs;root.advancedOptions=settings;if(enablingGhs){stretch.checked=true;asinh.checked=false;}root.previewAdjustments();}
        onGestureStarted:edits.beginEdit()
        onGestureEnded:edits.endEdit()
    }
            }
        }
    }
    Popup {id:rawComparePopup;parent:root;width:300;x:Math.max(8,root.width-width-16);y:Math.max(8,root.height-height-120);padding:12
        background:Rectangle {color:Atelier.surface;radius:6;border.color:Atelier.border}
        ColumnLayout {width:parent.width;spacing:8
            Label {text:qsTr("Compare a source exposure");color:Atelier.text;font.weight:Font.Medium}
            DarkComboBox {
                    id:rawComparisonChoice;objectName:"deepSkyRawChoice";Layout.fillWidth:true;Layout.minimumWidth:0
                    property var lightRows:controller.frames.map((row,index)=>({name:row.name,index:index,role:row.role})).filter(row=>row.role==="Light")
                    model:lightRows.map(row=>row.name);enabled:!controller.busy && controller.hasResult
                    onActivated:controller.chooseRawComparison(lightRows[currentIndex].index)
                }
            IconButton {text:qsTr("Raw exposure (hold)");Layout.fillWidth:true;Layout.minimumWidth:0;enabled:!controller.busy && controller.hasResult && controller.hasRawComparison;onPressedChanged:root.compareRaw(pressed)}
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
        background:Rectangle {color:Atelier.surface;radius:8;border.color:completionToast.success?"#4b8065":"#b97b56"}
        contentItem:Label {text:(completionToast.success?"✓  ":"")+completionToast.message;color:Atelier.text;wrapMode:Text.Wrap;Accessible.name:text}
        enter:Transition {NumberAnimation {property:"opacity";from:0;to:1;duration:150}}
        exit:Transition {NumberAnimation {property:"opacity";from:1;to:0;duration:150}}
    }
    DarkDialog {
        id:presetDialog;objectName:"deepSkyPresetDialog";parent:Overlay.overlay;anchors.centerIn:parent;modal:true
        title:qsTr("Deep-sky presets");width:Math.min(440,root.width-32)
        ColumnLayout {
            width:parent.width;spacing:12
            Label {text:qsTr("Name this finish for your camera, filters or intent. Use a new name to duplicate it. Presets are kept locally across updates.");color:Atelier.secondary;wrapMode:Text.Wrap;Layout.fillWidth:true}
            ParameterLabel {label:qsTr("Saved presets");explanation:qsTr("A preset replaces the complete finishing recipe and all adjustment families, including advanced tools. It does not change the imported files or stacking settings. Cmd+Z restores the previous finish.")}
            DarkComboBox {id:presetChooser;objectName:"deepSkyPresetChooser";Layout.fillWidth:true;model:root.library.deepSkyPresetNames;displayText:currentIndex>=0?currentText:qsTr("Your presets");onActivated:presetName.text=currentText}
            IconButton {text:qsTr("Restore entire finish");Layout.fillWidth:true;enabled:root.finishEditable && presetChooser.currentIndex>=0;onClicked:{if(root.setFinish(root.library.deepSkyPreset(presetChooser.currentText)))presetDialog.close()}}
            Rectangle {Layout.fillWidth:true;Layout.preferredHeight:1;color:Atelier.border}
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
    AtelierMenu {
        id:exportPanel;objectName:"deepSkyExportPanel";parent:root;x:Math.max(12,root.width-width-16);y:60
        AtelierMenuItem {text:qsTr("Final image");description:qsTr("PNG, TIFF or JPEG · optional caption");enabled:!root.finishDirty&&!controller.busy&&root.canEditResult;onTriggered:root.editResult()}
        AtelierMenuItem {text:qsTr("Linear FITS");description:qsTr("Original stack · 32-bit linear data");enabled:!controller.busy&&controller.hasResult;onTriggered:{root.linearExport=true;exportDialog.nameFilters=["FITS (*.fits)"];exportDialog.open()}}
        ExportSizeHint {fixedBytes:root.controller.linearExportBytes;format:"fits";visible:root.controller.hasResult;width:exportPanel.availableWidth}
    }

    FolderDialog {id:folderDialog;title:qsTr("Import an image folder");onAccepted:controller.addFolder(selectedFolder,root.selectedRole(),group.currentText,Number(aduScale.text))}
    FileDialog {id:inputDialog;title:qsTr("Add deep-sky images");fileMode:FileDialog.OpenFiles;nameFilters:["Astro images (*.fits *.fit *.fts *.fz *.tif *.tiff *.png *.cr2 *.cr3 *.nef *.nrw *.arw *.dng *.orf *.raf *.rw2 *.pef)"];onAccepted:controller.addFiles(selectedFiles,root.importRole,group.currentText,Number(aduScale.text))}
    FileDialog {id:exportDialog;fileMode:FileDialog.SaveFile;onAccepted:{let path=selectedFile.toString();if(!/\.(fits?|fts|tiff?|jpe?g)$/i.test(path))path+=root.linearExport?".fits":(selectedNameFilter.index===1?".jpg":selectedNameFilter.index===2?".fits":".tif");controller.exportImage(path,root.linearExport);}}
    FileDialog {id:projectOpen;fileMode:FileDialog.OpenFile;nameFilters:["AstroCapture project (*.astroproject)"];onAccepted:controller.loadProject(selectedFile)}
    FileDialog {id:projectSave;fileMode:FileDialog.SaveFile;nameFilters:["AstroCapture project (*.astroproject)"];onAccepted:controller.saveProject(selectedFile.toString().endsWith(".astroproject")?selectedFile:selectedFile.toString()+".astroproject")}
    DarkDialog {id:clearConfirm;parent:Overlay.overlay;anchors.centerIn:parent;modal:true;title:qsTr("Clear this project?");standardButtons:Dialog.Ok|Dialog.Cancel;Label {text:qsTr("Export results or save the project first. Original files remain unchanged.");width:300;wrapMode:Text.Wrap}
        onAccepted:controller.clear()}
}
