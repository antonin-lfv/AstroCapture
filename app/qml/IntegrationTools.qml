import "Atelier.js" as Atelier
import QtQuick
import AstroCapture
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Dialogs

Item {
    id:root
    required property DeepSkyController controller
    property real workspaceWidth:900
    property real workspaceHeight:700
    property bool merging:false
    property string hdrMessage:""
    function mapName(kind) {return ({weights:qsTr("Retained weights"),contributors:qsTr("Retained contributors"),rejections:qsTr("Rejected samples"),recovered:qsTr("HDR recovered pixels"),clipped:qsTr("HDR still saturated")})[kind]||kind;}
    function openMaps() {maps.open();refreshMap();}
    function openHdr() {hdr.open();}
    function refreshMap() {if(mapChoice.currentIndex>=0&&mapChoice.currentIndex<controller.diagnosticMaps.length)controller.inspectDiagnostic(controller.diagnosticMaps[mapChoice.currentIndex].key,channelChoice.currentIndex);}
    Connections {target:root.controller;function onOperationFinished(success,message) {if(root.merging){root.merging=false;root.hdrMessage=message;if(success)hdr.close();}}}
    DarkDialog {
        id:maps;objectName:"integrationMapsDialog";parent:Overlay.overlay;anchors.centerIn:parent;modal:true
        title:qsTr("Integration diagnostics");width:Math.min(850,root.workspaceWidth-32);height:Math.min(740,root.workspaceHeight-32)
        contentItem:ColumnLayout {
            spacing:10
            DarkComboBox {id:mapChoice;objectName:"diagnosticMapChoice";Layout.fillWidth:true;enabled:!root.controller.busy;model:root.controller.diagnosticMaps.map(row=>({label:row.group+" · "+root.mapName(row.kind)}));textRole:"label";onActivated:root.refreshMap();explanation:qsTr("Weights sum the retained geometrical overlaps; contributors count retained frames per channel; rejections count low and high rejected samples together. These are not noise, confidence or image-quality maps. HDR maps use 1 for the indicated pixels and 0 elsewhere.")}
            DarkComboBox {id:channelChoice;objectName:"diagnosticChannelChoice";Layout.fillWidth:true;model:[qsTr("Red"),qsTr("Green"),qsTr("Blue"),qsTr("Maximum of R, G and B")];currentIndex:3;enabled:!root.controller.busy;onActivated:root.refreshMap();explanation:qsTr("Select a single channel to inspect native Bayer coverage independently. Maximum shows the largest channel value at each pixel. Display brightness is scaled from zero to the displayed maximum; FITS export keeps all three original channels and exact values.")}
            Label {text:qsTr("Display: zero is black, maximum is white. Reduced preview; FITS retains the full resolution.");color:Atelier.secondary;wrapMode:Text.Wrap;Layout.fillWidth:true;font.pixelSize:12}
            Rectangle {Layout.fillWidth:true;Layout.fillHeight:true;Layout.minimumHeight:80;color:Atelier.image
                ImagePixelsItem {id:mapImage;anchors.centerIn:parent;width:sourceWidth>0?Math.min(parent.width,parent.height*sourceWidth/sourceHeight):0;height:sourceHeight>0?width*sourceHeight/sourceWidth:0;image:root.controller.diagnosticImage;objectName:"diagnosticPreview"}
                BusyIndicator {anchors.centerIn:parent;running:root.controller.busy;visible:running}
            }
            Label {text:root.controller.diagnosticInfo.key?qsTr("%1 × %2 px · min %3 · max %4 · positive pixels %5 %").arg(root.controller.diagnosticInfo.width).arg(root.controller.diagnosticInfo.height).arg(Number(root.controller.diagnosticInfo.minimum).toLocaleString(Qt.locale(),"g",5)).arg(Number(root.controller.diagnosticInfo.maximum).toLocaleString(Qt.locale(),"g",5)).arg(Number(root.controller.diagnosticInfo.positivePercent).toLocaleString(Qt.locale(),"f",4)):"";color:Atelier.text;wrapMode:Text.Wrap;Layout.fillWidth:true}
            Label {visible:!!root.controller.hdrReport.imageKey;text:qsTr("HDR alignment · maximum residual %1 px · minimum matched stars %2").arg(Number(Math.max.apply(null,(root.controller.hdrReport.alignments||[]).map(row=>row.residual))).toLocaleString(Qt.locale(),"f",3)).arg(Math.min.apply(null,(root.controller.hdrReport.alignments||[]).filter(row=>row.matches>0).map(row=>row.matches)));color:Atelier.secondary;wrapMode:Text.Wrap;Layout.fillWidth:true;font.pixelSize:12}
            Label {text:root.controller.status;color:Atelier.secondary;wrapMode:Text.Wrap;Layout.fillWidth:true;maximumLineCount:3;elide:Text.ElideRight;font.pixelSize:12}
            Label {visible:!!root.controller.diagnosticInfo.key;text:qsTr("RGB Float32 FITS · estimated size %1 MB").arg(Number((root.controller.diagnosticInfo.width*root.controller.diagnosticInfo.height*12+8640)/1000000).toLocaleString(Qt.locale(),"f",1));color:Atelier.secondary;font.pixelSize:12;wrapMode:Text.Wrap;Layout.fillWidth:true}
            RowLayout {Layout.fillWidth:true
                IconButton {text:qsTr("Close");onClicked:maps.close()}
                Item {Layout.fillWidth:true}
                IconButton {text:root.controller.exporting?qsTr("Exporting…"):qsTr("Export map as FITS");enabled:!root.controller.busy&&mapChoice.currentIndex>=0;onClicked:mapExport.open()}
            }
        }
    }
    FileDialog {id:mapExport;title:qsTr("Export diagnostic map");fileMode:FileDialog.SaveFile;nameFilters:["FITS (*.fits)"];onAccepted:{let path=selectedFile.toString();if(!/\.(fits?|fts)$/i.test(path))path+=".fits";root.controller.exportDiagnostic(path,root.controller.diagnosticMaps[mapChoice.currentIndex].key);}}
    ListModel {id:hdrSources}
    function parseNumber(text) {try{return Number.fromLocaleString(Qt.locale(),text);}catch(e){return NaN;}}
    function hdrValid() {if(hdrSources.count<2||hdrSources.count>8)return false;for(let n=0;n<hdrSources.count;n++){const row=hdrSources.get(n);if(!Number.isFinite(row.exposure)||row.exposure<=0||!Number.isFinite(row.saturation)||row.saturation<=0)return false;}return true;}
    DarkDialog {
        id:hdr;objectName:"hdrDialog";parent:Overlay.overlay;anchors.centerIn:parent;modal:true
        title:qsTr("Linear HDR fusion");width:Math.min(760,root.workspaceWidth-32);height:Math.min(790,root.workspaceHeight-32)
        onOpened:{root.hdrMessage="";hdrSources.clear();reviewed.checked=false;hdrTransition.value=Math.round(Number((root.controller.projectOptions.hdrOptions||{}).transition||.1)*100);root.controller.frames.forEach((row,index)=>{if(row.selected&&row.role==="Light"){const prior=(root.controller.hdrReport.sources||(root.controller.projectOptions.hdrOptions||{}).sources||[]).find(s=>s.path===row.path);hdrSources.append({sourceIndex:index,name:row.name,exposure:prior?prior.exposure:Number(row.exposure||0),saturation:prior?prior.saturation:1});}});}
        contentItem:ColumnLayout {
            spacing:10
            ScrollView {Layout.fillWidth:true;Layout.fillHeight:true;clip:true;ScrollBar.vertical.policy:ScrollBar.AlwaysOff;ScrollBar.horizontal.policy:ScrollBar.AlwaysOff;contentWidth:availableWidth
                ColumnLayout {width:parent.width;spacing:12
                    Label {text:qsTr("Use 2–8 selected FITS lights from one filter group: one calibrated, debayered linear image per exposure duration. Stack each duration separately first. HDR does not calibrate raw images or undo a stretch.");color:Atelier.secondary;wrapMode:Text.Wrap;Layout.fillWidth:true}
                    Label {text:qsTr("Verify individual exposure durations: an exported stack may store total integration time in EXPTIME. Values must share the same sensor units, gain, offset, filter and binning. Unknown metadata cannot prove compatibility.");color:Atelier.warning;wrapMode:Text.Wrap;Layout.fillWidth:true}
                    Repeater {model:hdrSources
                        ColumnLayout {
                            required property int index
                            required property int sourceIndex
                            required property string name
                            required property real exposure
                            required property real saturation
                            Layout.fillWidth:true;spacing:4
                            Label {text:name;color:Atelier.text;Layout.fillWidth:true;elide:Text.ElideMiddle}
                            GridLayout {columns:2;Layout.fillWidth:true;columnSpacing:12
                                SettingLabel {text:qsTr("Individual exposure (s)");explanation:qsTr("Duration of a single sub-exposure represented by this linear image, not the sum of an integrated series. Fusion divides pixel values by this duration, then returns them to the units of the longest individual exposure. Incorrect durations bias brightness and color.");Layout.fillWidth:true}
                                SettingLabel {text:qsTr("Saturation in sample units");explanation:qsTr("Measured clipping value in this input's normalized linear samples. 1 is only a suggestion, never inferred for floating-point FITS. Inspect the source histogram or camera data. Use a conservative threshold below the clipping plateau. If one channel clips, the entire RGB sample is excluded.");Layout.fillWidth:true}
                                TextField {objectName:"hdrExposure"+index;Layout.fillWidth:true;Layout.minimumWidth:0;Component.onCompleted:text=Number(exposure).toLocaleString(Qt.locale(),"g",8);selectByMouse:true;enabled:!root.controller.busy;Accessible.name:qsTr("Individual exposure (s)")+" · "+name;onTextEdited:hdrSources.setProperty(index,"exposure",root.parseNumber(text))}
                                TextField {objectName:"hdrSaturation"+index;Layout.fillWidth:true;Layout.minimumWidth:0;Component.onCompleted:text=Number(saturation).toLocaleString(Qt.locale(),"g",8);selectByMouse:true;enabled:!root.controller.busy;Accessible.name:qsTr("Saturation in sample units")+" · "+name;onTextEdited:hdrSources.setProperty(index,"saturation",root.parseNumber(text))}
                            }
                            Rectangle {Layout.fillWidth:true;Layout.preferredHeight:1;color:Atelier.border;Layout.topMargin:4}
                        }
                    }
                    RowLayout {Layout.fillWidth:true
                        SettingLabel {text:qsTr("Transition below saturation (%)");explanation:qsTr("Below the saturation threshold, a smooth transition reduces the longest exposure's weight. 10% starts blending at 90% of the threshold. Larger values blend earlier. Below the transition, weights are proportional to exposure duration; identical RGB weights protect channel ratios. This is not inverse-noise weighting.");Layout.fillWidth:true}
                        SpinBox {id:hdrTransition;objectName:"hdrTransition";from:1;to:50;value:10;enabled:!root.controller.busy}
                    }
                    HelpCheckBox {id:reviewed;objectName:"hdrReviewed";Layout.fillWidth:true;text:qsTr("I verified calibration, linearity, durations and units");enabled:!root.controller.busy;explanation:qsTr("A FITS file does not prove that its pixels are linear or calibrated. Confirm after checking the processing history and acquisition settings. Known camera/filter/gain/offset/unit mismatches are blocked. Automatic star alignment requires at least six matches; residuals above one pixel are blocked.")}
                    Label {text:qsTr("If all usable exposures clip, the shortest available sample is retained and marked as still saturated. No lost detail is invented. The result is linear, may exceed 1, and should be finished before TIFF/PNG export. Existing results are replaced only after success.");color:Atelier.secondary;wrapMode:Text.Wrap;Layout.fillWidth:true;font.pixelSize:12}
                }
            }
            Label {text:!root.hdrValid()?qsTr("Select 2–8 FITS lights and enter positive durations and saturation values."):root.hdrMessage;color:!root.hdrValid()?Atelier.warning:Atelier.secondary;wrapMode:Text.Wrap;Layout.fillWidth:true;maximumLineCount:3;elide:Text.ElideRight}
            RowLayout {Layout.fillWidth:true
                IconButton {text:root.controller.busy?qsTr("Cancel calculation"):qsTr("Close");onClicked:{if(root.controller.busy)root.controller.cancel();else hdr.close();}}
                Item {Layout.fillWidth:true}
                BusyIndicator {running:root.merging;visible:running;Layout.preferredWidth:28;Layout.preferredHeight:28}
                IconButton {objectName:"hdrMerge";text:qsTr("Merge exposures");enabled:!root.controller.busy&&reviewed.checked&&root.hdrValid();onClicked:{const sources=[];for(let n=0;n<hdrSources.count;n++){const s=hdrSources.get(n);sources.push({index:s.sourceIndex,exposure:s.exposure,saturation:s.saturation});}root.merging=true;root.controller.mergeHdr(sources,hdrTransition.value/100,true);}}
            }
        }
    }
}
