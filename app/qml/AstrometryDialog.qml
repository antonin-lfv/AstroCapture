import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Dialogs
import "Atelier.js" as Atelier

Popup {
    id: dialog
    objectName: "astrometryDialog"
    required property var controller
    property var values: ({})
    property var prepared: null
    property bool showMatches: false
    property int stageIndex: 0
    readonly property bool dialogOpen: opened || localCatalog.visible || equipmentResponse.visible
    property real hostWidth: 1100
    property real hostHeight: 800
    signal calibrationRequested(var settings)
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(600, hostWidth - 32)
    height: Math.min(800, hostHeight - 32, panel.implicitHeight + topPadding + bottomPadding)
    padding: 20
    modal: true
    focus: true
    closePolicy: controller.busy ? Popup.NoAutoClose : Popup.CloseOnEscape
    background: Rectangle {color:Atelier.surface;radius:10;border.color:Atelier.border}
    function refreshHints() {
        const s=controller.astrometry;
        ra.text=s.ra===undefined?"":Number(s.ra).toFixed(7);
        dec.text=s.dec===undefined?"":Number(s.dec).toFixed(7);
        sampling.text=s.scale===undefined?"":Number(s.scale).toFixed(5);
    }
    onOpened: {
        refreshHints();stageIndex=controller.astrometry.verified===true?1:0;
        method.currentIndex=String(values.photometryMethod||"").startsWith("SPCC")?1:0;
        white.currentIndex=String(values.photometryWhite||"").includes("(AB)")?1:0;
        response.currentIndex=method.currentIndex===0&&String(values.photometryProfile||"").includes("SHA256")?1:0;
        referenceLimit.text=String(controller.astrometry.referenceLimit||(method.currentIndex===1?60:150));
    }
    Connections {
        target: dialog.controller
        function onCatalogCalibrationReady(settings) {dialog.prepared=settings;}
        function onOperationFinished(success,message) {
            if(success&&dialog.prepared!==null){const settings=Object.assign({},dialog.values,dialog.prepared);dialog.prepared=null;dialog.calibrationRequested(settings);}
            else if(!success)dialog.prepared=null;
            if(success&&dialog.controller.astrometry.verified)dialog.refreshHints();
        }
    }
    component Note: Label {Layout.fillWidth:true;wrapMode:Text.Wrap;color:Atelier.secondary;font.pixelSize:12}
    component SectionTitle: RowLayout {
        property string label
        property string explanation
        Layout.fillWidth:true
        Label {text:parent.label;color:Atelier.text;font.weight:Font.Medium;wrapMode:Text.Wrap;Layout.fillWidth:true}
        HelpIcon {text:parent.explanation}
    }
    component Input: RowLayout {
        id: row
        required property string label
        required property string help
        property alias text: field.text
        property real minimum: -1e9
        property real maximum: 1e9
        signal accepted(real value)
        Layout.fillWidth:true
        Label {text:row.label;Layout.fillWidth:true;wrapMode:Text.Wrap;color:Atelier.text}
        HelpIcon {text:row.help}
        TextField {id:field;objectName:row.objectName+"Field";Layout.preferredWidth:132;selectByMouse:true;color:Atelier.text;font.pixelSize:13;background:Rectangle {color:Atelier.field;radius:4;border.color:field.activeFocus?Atelier.accent:Atelier.border} enabled:!dialog.controller.busy;validator:DoubleValidator {bottom:row.minimum;top:row.maximum;locale:"C"} Accessible.name:row.label;onEditingFinished:if(acceptableInput)row.accepted(Number(text))}
    }
    ColumnLayout {
        id: panel
        anchors.fill:parent
        spacing:12
        RowLayout {Layout.fillWidth:true
            Label {text:qsTr("Astrometry and color calibration");color:Atelier.text;font.pixelSize:16;font.weight:Font.DemiBold;Layout.fillWidth:true;wrapMode:Text.Wrap}
            IconButton {text:qsTr("Close");enabled:!dialog.controller.busy;onClicked:dialog.close()}
        }
        RowLayout {Layout.fillWidth:true;spacing:8
            IconButton {text:qsTr("1 · WCS");Layout.fillWidth:true;primary:dialog.stageIndex===0;onClicked:dialog.stageIndex=0}
            IconButton {text:qsTr("2 · PCC / SPCC");Layout.fillWidth:true;primary:dialog.stageIndex===1;onClicked:dialog.stageIndex=1}
        }
        Flickable {
            id: scroll
            Layout.fillWidth:true
            Layout.fillHeight:true
            clip:true
            contentWidth:width
            contentHeight:body.implicitHeight
            implicitHeight:body.implicitHeight
            boundsBehavior:Flickable.StopAtBounds
            ColumnLayout {
                id:body
                width:scroll.width
                spacing:12
                ColumnLayout {Layout.fillWidth:true;visible:dialog.stageIndex===0;spacing:12
                SectionTitle {label:qsTr("1 · Solve the field (WCS)");explanation:qsTr("Identify stars without changing pixels. The approximate centre and sampling constrain the search; rotation and mirroring are found automatically. This is a targeted TAN solver, not a blind full-sky solver.")}
                Input {id:ra;objectName:"astrometryRa";label:qsTr("Right ascension · degrees");minimum:0;maximum:359.99999999;help:qsTr("Approximate ICRS field centre. Convert RA in hours to degrees by multiplying by 15. Example: M42 is near 83.82°. FITS hints are filled when available.")}
                Input {id:dec;objectName:"astrometryDec";label:qsTr("Declination · degrees");minimum:-90;maximum:90;help:qsTr("Approximate ICRS declination, with its sign. M42 is near −5.39°. Keep the target within the search field.")}
                Input {id:sampling;objectName:"astrometrySampling";label:qsTr("Sampling · arcsec/pixel");minimum:.01;maximum:100;help:qsTr("Use 206.265 × pixel size in µm / effective focal length in mm. Include binning in pixel size. A seed between half and twice the actual sampling can be refined.")}
                Input {id:limit;objectName:"astrometryMagnitude";text:"16";label:qsTr("Limiting magnitude · Gaia G");minimum:5;maximum:20;help:qsTr("Include stars brighter than this Gaia G magnitude. Increase for sparse fields, reduce for dense fields. The catalogue is capped at 5000 stars.")}
                RowLayout {Layout.fillWidth:true
                    DarkComboBox {id:catalogMode;objectName:"astrometryCatalogMode";Layout.fillWidth:true;model:[qsTr("Gaia DR3 · online"),qsTr("Local Gaia CSV")];enabled:!dialog.controller.busy;explanation:qsTr("Online queries send field coordinates and sampling to ESA, never your image. Catalogues and XP spectra are cached locally. CSV works without internet; export Gaia positions, magnitudes, temperatures and proper motions.")}
                    IconButton {text:qsTr("Import CSV");enabled:!dialog.controller.busy;onClicked:localCatalog.open()}
                }
                Note {visible:catalogMode.currentIndex===1;text:dialog.controller.astrometry.localName||qsTr("Import a Gaia catalogue before solving.")}
                IconButton {objectName:"astrometrySolve";text:qsTr("Solve and verify WCS");Layout.fillWidth:true;busy:dialog.controller.busy&&dialog.prepared===null;enabled:dialog.controller.hasResult&&!dialog.controller.busy&&ra.text.length>0&&dec.text.length>0&&sampling.text.length>0&&(catalogMode.currentIndex===0||!!dialog.controller.astrometry.localName);onClicked:dialog.controller.solveAstrometry({ra:Number(ra.text),dec:Number(dec.text),scale:Number(sampling.text),magnitude:Number(limit.text),local:catalogMode.currentIndex===1})}
                Note {objectName:"astrometryDiagnostics";visible:dialog.controller.astrometry.verified===true;text:{const s=dialog.controller.astrometry;return qsTr("Verified · %1 stars · RMS %2 px (%3″) · field coverage %4%").arg(s.matches||0).arg(Number(s.rms||0).toFixed(3)).arg(Number((s.rms||0)*(s.scale||0)).toFixed(3)).arg((Number(s.coverage||0)*100).toFixed(0));}}
                RowLayout {visible:dialog.controller.astrometry.verified===true;Layout.fillWidth:true
                    CheckBox {text:qsTr("Mark matched stars on the image");checked:dialog.showMatches;enabled:!dialog.controller.busy;onClicked:dialog.showMatches=checked;Layout.fillWidth:true}
                    HelpIcon {text:qsTr("Close this dialog to inspect the catalogue markers at 100%. Markers are hidden when geometry or comparison changes pixel coordinates. Verify stars across the whole field before trusting calibration.")}
                }
                }
                ColumnLayout {Layout.fillWidth:true;visible:dialog.stageIndex===1;spacing:12
                SectionTitle {label:qsTr("2 · Calibrate broadband RGB");explanation:qsTr("Calibration measures identified stars on linear pixels before stretching. The final preview may remain stretched. Narrowband palettes and duplicated mono channels are unsuitable for this broadband calibration.")}
                DarkComboBox {id:method;objectName:"catalogCalibrationMethod"
                    onActivated: index => {referenceLimit.text=String(index===1?60:150)}
                    Layout.fillWidth:true;model:[qsTr("PCC · stellar temperature model"),qsTr("SPCC · Gaia XP spectra")];enabled:!dialog.controller.busy;explanation:qsTr("PCC approximates stellar continua using Gaia temperatures; extinction and absorption lines limit its accuracy. SPCC integrates measured Gaia XP spectra through the actual equipment response. Both use robust aperture photometry and reject outliers.")}
                DarkComboBox {id:response;objectName:"catalogCalibrationResponse";Layout.fillWidth:true;model:method.currentIndex===1?[qsTr("My equipment · measured RGB response")]:[qsTr("Bessell R/V/B · PCC reference"),qsTr("My equipment · measured RGB response")];enabled:!dialog.controller.busy;explanation:qsTr("For SPCC import photon response QE × RGB filter × any additional transmission, per channel. CSV: wavelength_nm,red,green,blue; increasing 336–1020 nm, values 0–1 and zero endpoints. Do not normalize channels independently. The Bessell reference is a model, not your camera.")}
                IconButton {text:qsTr("Import equipment response");Layout.fillWidth:true;enabled:!dialog.controller.busy;onClicked:equipmentResponse.open()}
                Note {visible:!!dialog.controller.astrometry.responseName;text:dialog.controller.astrometry.responseName||""}
                DarkComboBox {id:white;objectName:"catalogCalibrationWhite";Layout.fillWidth:true;model:[qsTr("White reference · Sun (CALSPEC)"),qsTr("White reference · constant Fν (AB)")];enabled:!dialog.controller.busy;explanation:qsTr("The Sun uses the measured CALSPEC solar spectrum as neutral white. Constant Fν uses an AB reference (Fλ proportional to 1/λ²). These choices define the desired white, not the sky background.")}
                InspectorFamily {title:qsTr("Photometry settings");expanded:false;onActivated:expanded=!expanded
                    Input {id:referenceLimit;objectName:"catalogCalibrationLimit";label:qsTr("Maximum reference stars");text:method.currentIndex===1?"60":"150";minimum:8;maximum:150;help:qsTr("More stars improve the robustness of the color fit but take longer to download. Default: 60 for SPCC, 150 for PCC. At least eight stars must remain after photometric rejection.")}
                    Input {label:qsTr("Aperture radius · pixels");text:String(dialog.values.photometryAperture||5);minimum:1;maximum:Math.min(30,Number(dialog.values.photometryInner||8)-.01);help:qsTr("Include the stellar flux while avoiding neighbours. The sky annulus must start beyond this aperture. Default: 5 pixels.");onAccepted:value=>dialog.calibrationRequested(Object.assign({},dialog.values,{photometryAperture:value}))}
                    Input {label:qsTr("Sky annulus · inner radius");text:String(dialog.values.photometryInner||8);minimum:Number(dialog.values.photometryAperture||5)+.01;maximum:Number(dialog.values.photometryOuter||12)-.01;help:qsTr("Start the sky ring beyond the stellar wings. Nebulosity and neighbours bias the local background estimate.");onAccepted:value=>dialog.calibrationRequested(Object.assign({},dialog.values,{photometryInner:value}))}
                    Input {label:qsTr("Sky annulus · outer radius");text:String(dialog.values.photometryOuter||12);minimum:Number(dialog.values.photometryInner||8)+.01;maximum:80;help:qsTr("The whole sky ring must fit in the image. Increase for more sky samples, keeping it local to the star.");onAccepted:value=>dialog.calibrationRequested(Object.assign({},dialog.values,{photometryOuter:value}))}
                    Input {label:qsTr("Saturation · image units");text:String(dialog.values.photometrySaturation||1);minimum:.000001;maximum:10;help:qsTr("Reject a star if any aperture channel reaches this threshold. Use the saturation value in the imported linear image units. Saturated sensor cores cannot be recovered.");onAccepted:value=>dialog.calibrationRequested(Object.assign({},dialog.values,{photometrySaturation:value}))}
                }
                Note {text:qsTr("Correct gradients first. Calibration replaces automatic RGB gains with 1; manual adjustments remain available. Photometry settings are shared with the reference-star tool.")}
                Note {visible:!!dialog.values.photometryMethod;text:qsTr("Active calibration: %1 · %2").arg(dialog.values.photometryMethod||"").arg(dialog.values.photometryWhite||"")}
                Note {visible:dialog.controller.astrometry.verified!==true;text:qsTr("First solve the field in the WCS tab. Calibration requires identified stars.")}
                Note {objectName:"catalogCalibrationDiagnostics";visible:dialog.values.photometry===true&&dialog.controller.advancedReport.photometryAccepted!==undefined;text:{const r=dialog.controller.advancedReport,g=r.photometryGains||[1,1,1];return qsTr("Used %1 · rejected %2 · scatter %3 mag\nRGB gains %4 / %5 / %6\nRejections: edges %7 · saturation %8 · weak signal %9 · outliers %10").arg(r.photometryAccepted||0).arg(r.photometryRejected||0).arg(Number(r.photometryScatter||0).toFixed(4)).arg(Number(g[0]).toFixed(4)).arg(Number(g[1]).toFixed(4)).arg(Number(g[2]).toFixed(4)).arg(r.photometryEdgeRejected||0).arg(r.photometrySaturatedRejected||0).arg(r.photometrySignalRejected||0).arg(r.photometryOutlierRejected||0);}}
                }
            }
        }
        Rectangle {Layout.fillWidth:true;Layout.preferredHeight:1;color:Atelier.border}
        Note {objectName:"astrometryStatus";text:dialog.controller.status}
        RowLayout {Layout.fillWidth:true
            IconButton {text:qsTr("Cancel calculation");visible:dialog.controller.busy;onClicked:dialog.controller.cancel()}
            IconButton {visible:dialog.stageIndex===0;text:qsTr("Continue to colors");Layout.fillWidth:true;enabled:!dialog.controller.busy&&dialog.controller.astrometry.verified===true;onClicked:dialog.stageIndex=1}
            IconButton {visible:dialog.stageIndex===1;objectName:"catalogCalibrationApply";text:method.currentIndex?qsTr("Calibrate with SPCC"):qsTr("Calibrate with PCC");Layout.fillWidth:true;busy:dialog.controller.busy;enabled:dialog.controller.hasResult&&!dialog.controller.busy&&dialog.controller.astrometry.verified===true&&((method.currentIndex===0&&response.currentIndex===0)||!!dialog.controller.astrometry.responseName)&&Number(dialog.values.maskType||0)===0;onClicked:dialog.controller.prepareCatalogCalibration(method.currentIndex,{customResponse:method.currentIndex===1||response.currentIndex===1,white:white.currentIndex,maxStars:Number(referenceLimit.text)})}
        }
    }
    FileDialog {id:localCatalog;title:qsTr("Gaia DR3 catalogue");nameFilters:[qsTr("CSV catalogue (*.csv)")];onAccepted:{dialog.controller.importAstrometryCatalog(selectedFile);catalogMode.currentIndex=1;}}
    FileDialog {id:equipmentResponse;title:qsTr("Measured equipment response");nameFilters:[qsTr("RGB response (*.csv)")];onAccepted:{dialog.controller.importColorResponse(selectedFile);response.currentIndex=method.currentIndex===0?1:0;}}
}
