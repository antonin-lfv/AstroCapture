import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Dialogs
import "Atelier.js" as Atelier
ColumnLayout {
    id:tools
    required property var controller
    property var values:({})
    property bool editable:false
    property bool linearInput:true
    property int exportLayer:1
    signal edited(var settings)
    signal gestureStarted()
    signal gestureEnded()
    signal viewRequested(int layer)
    spacing:8;Layout.fillWidth:true
    function setValue(key,value){const s=Object.assign({},values);s[key]=value;edited(s);}
    component Parameter:ParameterControl {
        id:p
        required property string key
        property real fallback:1
        enabled:tools.editable&&tools.values.starSeparation===true
        value:tools.values[key]===undefined?fallback:Number(tools.values[key])
        onMoved:tools.setValue(key,value)
        onGestureStarted:tools.gestureStarted()
        onGestureEnded:tools.gestureEnded()
        Connections {target:tools;function onValuesChanged(){p.value=tools.values[p.key]===undefined?p.fallback:Number(tools.values[p.key]);}}
    }
    HelpCheckBox {objectName:"enableStarSeparation";text:qsTr("Separate stars and background");Layout.fillWidth:true;checked:tools.values.starSeparation||false;enabled:tools.editable&&tools.linearInput&&Number(tools.values.maskType||0)===0;onClicked:tools.setValue("starSeparation",checked);explanation:qsTr("Fits circular Gaussian/Moffat stars over a local sky plane on linear pixels, after color calibration and gradient correction. Extended structures and rejected stars stay in the background. This PSF estimate is incomplete for saturated, blended or irregular stars. Inspect both layers. Disable masks; scientific measurements must precede separation.")}
    Label {visible:tools.values.starSeparation===true;Layout.fillWidth:true;wrapMode:Text.Wrap;color:Atelier.secondary;font.pixelSize:11;text:qsTr("Common stretch stays in the Stretch family. Neutral layer controls reproduce that result exactly. Final image exports the recombined result.")}
    RowLayout {Layout.fillWidth:true
        IconButton {text:qsTr("Combined");Layout.fillWidth:true;ghost:tools.controller.starLayerView!==0;enabled:tools.controller.hasStarLayers&&!tools.controller.busy;onClicked:tools.viewRequested(0)}
        IconButton {objectName:"viewStarsLayer";text:qsTr("Stars");Layout.fillWidth:true;ghost:tools.controller.starLayerView!==1;enabled:tools.controller.hasStarLayers&&!tools.controller.busy;onClicked:tools.viewRequested(1)}
        IconButton {objectName:"viewStarBackgroundLayer";text:qsTr("Background");Layout.fillWidth:true;ghost:tools.controller.starLayerView!==2;enabled:tools.controller.hasStarLayers&&!tools.controller.busy;onClicked:tools.viewRequested(2)}
    }
    Label {objectName:"starLayerDiagnostics";Layout.fillWidth:true;wrapMode:Text.Wrap;visible:tools.controller.hasStarLayers;color:Atelier.secondary;font.pixelSize:11;text:qsTr("Fitted %1 / %2 · rejected %3 (saturated %4)\nMean normalized fit RMS %5").arg(tools.controller.advancedReport.starsAccepted||0).arg(tools.controller.advancedReport.starsDetected||0).arg(tools.controller.advancedReport.starsRejected||0).arg(tools.controller.advancedReport.starsSaturated||0).arg(Number(tools.controller.advancedReport.starsFitRms||0).toFixed(4))}
    SettingLabel {text:qsTr("Stars");explanation:qsTr("Adjust only the stellar layer. It is the difference between the common finished image and the common finished PSF residual, so its appearance follows the common stretch. Signed Float32 values are preserved; preview and integer exports clip the display range.");color:Atelier.text;Layout.fillWidth:true}
    Parameter {key:"starsGain";label:qsTr("Stellar brightness / reduction");from:0;to:3;fallback:1;decimals:3;explanation:qsTr("1 preserves stellar intensity; below 1 reduces the fitted stellar contribution, 0 removes it. This reduces brightness, not stellar radius, and leaves rejected stars in the background. Values above 1 brighten stars and can exceed the display range.")}
    Parameter {key:"starsMidtones";label:qsTr("Stellar midtones");from:.001;to:.999;fallback:.5;decimals:4;explanation:qsTr("0.5 is neutral. Lower values brighten the intermediate stellar signal; higher values dim it. The curve acts only between 0 and 1 after stellar gain, retaining signed and high-range values outside that interval.")}
    Parameter {key:"starsSaturation";label:qsTr("Stellar saturation");from:0;to:3;fallback:1;decimals:3;explanation:qsTr("1 preserves stellar colors; 0 makes the stellar contribution neutral in weighted luminance. Higher values strengthen color differences and may create out-of-range channel values.")}
    SettingLabel {text:qsTr("Background and extended structures");explanation:qsTr("The background is the original image minus accepted stellar PSF models. It retains nebulae, galaxies, noise and rejected stars. Check it at 100% before increasing brightness or reducing noise.");color:Atelier.text;Layout.fillWidth:true}
    Parameter {key:"starBackgroundGain";label:qsTr("Background brightness");from:0;to:3;fallback:1;decimals:3;explanation:qsTr("Multiplies the finished residual layer independently of the stellar contribution. 1 is neutral. Brightening also reveals noise and remaining stars; no clipped or missing structure is recovered.")}
    Parameter {key:"starBackgroundSaturation";label:qsTr("Background saturation");from:0;to:3;fallback:1;decimals:3;explanation:qsTr("Changes color differences in the finished residual only. 1 is neutral and 0 is monochrome. It does not replace PCC/SPCC, which must measure original linear stars before separation.")}
    Parameter {key:"starBackgroundDenoise";label:qsTr("Background denoising");from:0;to:1;fallback:0;decimals:3;explanation:qsTr("Applies the existing denoising filter to the finished residual only. 0 disables it; larger values smooth more. Fine nebular structures can be lost, so compare at 100%.")}
    InspectorFamily {title:qsTr("Extraction settings");expanded:false;onActivated:expanded=!expanded
        DarkComboBox {Layout.fillWidth:true;model:[qsTr("Gaussian PSF"),qsTr("Moffat PSF")];currentIndex:Number(tools.values.starPsfProfile||0);enabled:tools.editable;onActivated:index=>tools.setValue("starPsfProfile",index);explanation:qsTr("Gaussian fits compact stellar cores. Moffat has stronger wings, controlled by beta. Neither models diffraction spikes or saturation. Choose the closest profile and inspect residuals.")}
        Parameter {key:"starFwhm";label:qsTr("PSF FWHM · pixels");from:.5;to:12;fallback:3;decimals:3;explanation:qsTr("Typical stellar width at half maximum in original image pixels. Fits explore 0.65–1.5 times this width. Use the measured PSF in Advanced transformations as a guide. Excessive width can include compact nebular knots.")}
        Parameter {key:"starBeta";label:qsTr("Moffat beta");from:1.5;to:10;fallback:3;decimals:3;visible:Number(tools.values.starPsfProfile||0)===1;explanation:qsTr("Lower beta gives stronger extended wings for the same FWHM. Higher beta approaches a Gaussian-like core. This value is used only by the Moffat profile.")}
        Parameter {key:"starLimit";label:qsTr("Maximum detected stars");from:1;to:5000;fallback:2000;decimals:0;stepSize:1;explanation:qsTr("Caps candidate detection, brightest first, to control processing cost. The detector downsamples large images; faint or closely spaced stars can remain in the residual even below the cap.")}
        Parameter {key:"starFitTolerance";label:qsTr("Fit tolerance");from:.01;to:.5;fallback:.15;decimals:3;explanation:qsTr("Maximum local fit RMS divided by stellar peak amplitude. Smaller values keep only cleaner PSF fits; larger values accept imperfect stars and risk subtracting compact structures. Rejected candidates remain in the background.")}
        Parameter {key:"starSaturationLimit";label:qsTr("Saturation limit · image units");from:.01;to:10;inputMaximum:100;fallback:.98;decimals:4;explanation:qsTr("Reject a star when any core channel reaches this threshold in the prepared linear image, including calibrated RGB gains. Saturated cores cannot be reconstructed reliably. Check the units and actual sensor saturation.")}
    }
    RowLayout {Layout.fillWidth:true
        IconButton {text:qsTr("Export stars");Layout.fillWidth:true;enabled:tools.controller.hasStarLayers&&!tools.controller.busy;onClicked:{tools.exportLayer=1;layerFile.open()}}
        IconButton {text:qsTr("Export background");Layout.fillWidth:true;enabled:tools.controller.hasStarLayers&&!tools.controller.busy;onClicked:{tools.exportLayer=2;layerFile.open()}}
        HelpIcon {text:qsTr("Exports the finished layer as signed Float32 FITS, with current geometry and WCS/SIP. Adding stars and background reproduces the combined result within floating-point rounding. The main Export command always exports the combined image.")}
    }
    readonly property bool dialogOpen:layerFile.visible
    FileDialog {id:layerFile;fileMode:FileDialog.SaveFile;defaultSuffix:"fits";title:tools.exportLayer===1?qsTr("Export stellar layer"):qsTr("Export residual background");nameFilters:[qsTr("Floating-point FITS (*.fits)")];onAccepted:tools.controller.exportStarLayer(selectedFile,tools.exportLayer)}
}
