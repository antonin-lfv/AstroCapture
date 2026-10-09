import "Atelier.js" as Atelier
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Dialogs

Control {
    id: tools
    objectName: "deepSkyAdvancedTools"
    required property var controller
    property var values: ({})
    property string draftError: ""
    property bool draftPending: false
    property bool livePreview: true
    signal previewRequested
    property bool editable: false
    property bool linearInput: true
    property bool pendingMaskImport:false
    property bool pendingMaskEditor:false
    property int toolIndex: 0
    signal edited(var settings)
    signal gestureStarted
    signal gestureEnded
    readonly property bool catalogOpen: catalog.visible || maskEditor.visible || maskImport.visible
    implicitHeight: contentItem.implicitHeight
    padding: 0
    palette.windowText: Atelier.text
    palette.text: Atelier.text
    palette.base: Atelier.field
    palette.button: Atelier.field
    palette.buttonText: Atelier.text
    palette.highlight: Atelier.accent
    background: Item {}
    function change(key, value) {
        const copy = Object.assign({}, values);
        copy[key] = value;
        edited(copy);
    }
    MaskEditor {id:maskEditor;onAccepted:(values)=>tools.edited(values)}
    FileDialog {id:maskImport;title:qsTr("Import a mask");nameFilters:[qsTr("Mask images (*.fits *.fit *.fts *.png *.tif *.tiff)")];onAccepted:{tools.pendingMaskImport=true;tools.controller.loadMaskFile(selectedFile);}}
    function openMaskEditor(){if(values.maskType===5&&controller.importedMaskPath!==values.maskFile){pendingMaskEditor=true;controller.loadMaskFile("file://"+encodeURI(values.maskFile));}else maskEditor.edit(controller.maskSourceImage,values,controller.importedMaskImage);}
    Connections {target:tools.controller
        function onMaskFileReady(path){if(tools.pendingMaskImport){tools.pendingMaskImport=false;tools.edited(Object.assign({},tools.values,{maskType:5,maskFile:path,maskStrokes:[]}));}if(tools.pendingMaskEditor){tools.pendingMaskEditor=false;maskEditor.edit(tools.controller.maskSourceImage,tools.values,tools.controller.importedMaskImage);}}
        function onOperationFinished(success,message){if(!success){tools.pendingMaskImport=false;tools.pendingMaskEditor=false;}}
    }
    component Explanation: Label {
        color: Atelier.secondary
        wrapMode: Text.Wrap
        Layout.fillWidth: true
        font.pixelSize: 12
    }
    component NumberSetting: ColumnLayout {
        id: setting
        required property string label
        required property string key
        required property string explanation
        property real minimum: 0
        property real maximum: 1
        property bool integer: false
        Layout.fillWidth: true
        spacing: 2
        RowLayout {
            Layout.fillWidth: true
            Label {
                text: setting.label
                color: Atelier.text
                Layout.fillWidth: true
                wrapMode: Text.Wrap
            }
            HelpIcon {text:setting.explanation}
            TextField {
                Layout.preferredWidth: 115
                text: Number(tools.values[setting.key] === undefined ? 0 : tools.values[setting.key]).toPrecision(7)
                validator: DoubleValidator {
                    bottom: setting.minimum
                    top: setting.maximum
                    locale: "C"
                }
                enabled: tools.editable
                onEditingFinished: {
                    const v = Number(text);
                    if (Number.isFinite(v) && v >= setting.minimum && v <= setting.maximum)
                        tools.change(setting.key, setting.integer ? Math.round(v) : v);
                }
            }
        }
        Slider {
            Layout.fillWidth: true
            from: setting.minimum
            to: setting.maximum
            stepSize: setting.integer ? 1 : 0
            value: tools.values[setting.key] === undefined ? 0 : tools.values[setting.key]
            enabled: tools.editable
            onMoved: tools.change(setting.key, setting.integer ? Math.round(value) : value)
            onPressedChanged: {
                if (pressed)
                    tools.gestureStarted();
                else
                    tools.gestureEnded();
            }
        }
    }
    component Toggle: CheckBox {
        id: toggle
        required property string key
        checked: tools.values[key] || false
        enabled: tools.editable
        onClicked: tools.change(key, checked)
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        contentItem: Text {
            text: toggle.text; font: toggle.font; color: toggle.enabled ? Atelier.text : Atelier.muted
            leftPadding: toggle.indicator.width + toggle.spacing
            wrapMode: Text.Wrap; verticalAlignment: Text.AlignVCenter
        }
    }
    contentItem: ColumnLayout {
        spacing: 12
        RowLayout {
            visible:false
            Layout.fillWidth: true
            Label {
                text: qsTr("Advanced transformations")
                color: Atelier.text
                font.pixelSize: 20
                font.bold: true
                Layout.fillWidth: true
            }
            IconButton {
                text: "×"
                visible:false
                Accessible.name: qsTr("Close")
            }
        }
        DarkComboBox {
            objectName: "deepSkyAdvancedToolChooser"
            Layout.fillWidth: true
            model: [qsTr("GHS stretch"), qsTr("PSF and deconvolution"), qsTr("Multiscale denoising"), qsTr("Masks"), qsTr("Photometric colors")]
            currentIndex: tools.toolIndex
            onActivated: tools.toolIndex = currentIndex
        }
        RowLayout {
            Label {text: qsTr("Processing order")
            color: "#8eadd0"
            }
            HelpIcon {
                text: qsTr("Changes are applied automatically. A mask blends the adjustments; orientation is applied afterwards.") + "\n\n" + qsTr("Background → explicit pedestal → photometric colors → multiscale denoising → Richardson–Lucy → channel gains and stretch → saturation and smoothing → SCNR → mask blending → orientation. Scientific measurements cannot follow an earlier stretch or orientation.")
            }
        }
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Atelier.border
        }
        ColumnLayout {
            id: advancedScroll
            Layout.fillWidth: true
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 12
                ColumnLayout {
                    visible: tools.toolIndex === 0
                    Layout.fillWidth: true
                    RowLayout {
                    Layout.fillWidth: true
                    Toggle {
                        objectName: "deepSkyGhsEnabled"
                        key: "ghs"
                        text: qsTr("Use generalized hyperbolic stretch")
                    }
                    HelpIcon {
                        text: qsTr("GHS replaces the midtones or asinh curve when stretching is enabled. Zero strength is neutral. Work in small increments; the histogram curve uses the same calculation as the image.")
                    }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Label {text:qsTr("Stretch curve");color:Atelier.secondary;Layout.fillWidth:true}
                        HelpIcon {text:qsTr("Curve: normalized input 0–1 horizontally, output 0–1 vertically. Gray is identity. Shared-luminance color blending and masks can change the final per-channel result.")}
                    }
                    Canvas {
                        objectName: "deepSkyGhsCurveCanvas"
                        Layout.fillWidth: true
                        Layout.preferredHeight: 140
                        property var curve: tools.controller.ghsCurve(tools.values)
                        onCurveChanged: requestPaint()
                        onWidthChanged: requestPaint()
                        onPaint: {
                            const ctx = getContext("2d");
                            ctx.fillStyle = "#0b1017";
                            ctx.fillRect(0, 0, width, height);
                            ctx.strokeStyle = "#3c4a59";
                            ctx.beginPath();
                            ctx.moveTo(0, height);
                            ctx.lineTo(width, 0);
                            ctx.stroke();
                            if (curve.length !== 513)
                                return;
                            ctx.strokeStyle = "#71d7b1";
                            ctx.beginPath();
                            for (let i = 0; i < curve.length; i++) {
                                const x = i * width / 512, y = height * (1 - curve[i]);
                                if (i === 0)
                                    ctx.moveTo(x, y);
                                else
                                    ctx.lineTo(x, y);
                            }
                            ctx.stroke();
                        }
                    }
                    NumberSetting {
                        key: "ghsStrength"
                        label: qsTr("Stretch strength · ln(D + 1)")
                        maximum: 20
                        explanation: qsTr("The algorithm uses D = exp(strength) − 1. Increasing strength redistributes contrast around the symmetry point. It reveals existing faint detail and noise; it does not add signal.")
                    }
                    NumberSetting {
                        key: "ghsLocal"
                        label: qsTr("Local intensity · b")
                        minimum: -5
                        maximum: 15
                        explanation: qsTr("Positive b concentrates contrast near the symmetry point. Negative b distributes it more broadly. The published GHS formula changes continuously between logarithmic, integral, exponential and hyperbolic forms.")
                    }
                    NumberSetting {
                        key: "ghsSymmetry"
                        label: qsTr("Symmetry point · SP")
                        minimum: Number(tools.values.ghsShadows || 0)
                        maximum: tools.values.ghsHighlights === undefined ? 1 : tools.values.ghsHighlights
                        explanation: qsTr("SP is the input brightness receiving the greatest contrast. Place it near the histogram feature you want to expand. It must lie between the shadow and highlight protection thresholds.")
                    }
                    NumberSetting {
                        key: "ghsShadows"
                        label: qsTr("Shadow protection · LP")
                        maximum: Number(tools.values.ghsSymmetry || 0)
                        explanation: qsTr("Below LP the curve follows a tangent line, preserving continuity and reserving contrast for shadows. It does not freeze those pixels. LP must be at or below SP.")
                    }
                    NumberSetting {
                        key: "ghsHighlights"
                        label: qsTr("Highlight protection · HP")
                        minimum: Number(tools.values.ghsSymmetry || 0)
                        explanation: qsTr("Above HP the curve follows a tangent line to reserve contrast for highlights and star cores. It does not reconstruct saturated cores. HP must be at or above SP.")
                    }
                    DarkComboBox {
                        Layout.fillWidth: true
                        model: [qsTr("Independent RGB channels"), qsTr("Shared luminance · RGB blend protection")]
                        currentIndex: tools.values.ghsMode || 0
                        enabled: tools.editable
                        onActivated: tools.change("ghsMode", currentIndex)
                    }
                    Explanation {
                        text: qsTr("Shared luminance multiplies RGB by one factor. If a channel would exceed 1, RGB blending mixes in the independent-channel result to stay bounded. This can change color ratios near highlights.")
                    }
                    Toggle {
                        key: "ghsInverse"
                        text: qsTr("Inverse curve")
                    }
                    Explanation {
                        text: qsTr("Inverse reverses the scalar curve. It cannot recover clipped values or undo a mask or shared-luminance color blending. Use Undo to restore the exact previous settings.")
                    }
                }
                ColumnLayout {
                    visible: tools.toolIndex === 1
                    Layout.fillWidth: true
                    Explanation {
                        text: tools.linearInput ? qsTr("Use on linear data before stretching. The circular PSF describes a spatially constant blur; inspect stars at 100% for dark rings or amplified noise.") : qsTr("Earlier transformations changed brightness or orientation. Deconvolution and stellar PSF measurement are blocked; return to a linear, unrotated recipe input.")
                        color: tools.linearInput ? Atelier.secondary : "#edb06c"
                    }
                    Toggle {
                        objectName: "deepSkyDeconvolutionEnabled"
                        key: "deconvolution"
                        text: qsTr("Richardson–Lucy deconvolution")
                        enabled: tools.editable && tools.linearInput
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        IconButton {
                            objectName: "deepSkyEstimatePsf"
                            text: qsTr("Measure stellar PSF")
                            Layout.fillWidth: true
                            enabled: tools.editable && tools.linearInput && !tools.controller.busy
                            onClicked: tools.controller.estimatePsf()
                        }
                        IconButton {
                            text: qsTr("Use estimate")
                            enabled: tools.editable && tools.controller.advancedReport.estimatedFwhm !== undefined
                            onClicked: {
                                const copy = Object.assign({}, tools.values, {
                                    psfFwhm: tools.controller.advancedReport.estimatedFwhm,
                                    psfProfile: 0
                                });
                                tools.edited(copy);
                            }
                        }
                    }
                    Explanation {
                        text: tools.controller.advancedReport.estimatedFwhm === undefined ? qsTr("Estimation uses at least three central, unsaturated stars and Gaussian second moments. It is not a Moffat fit or blind PSF estimation.") : qsTr("Estimated FWHM %1 px · scatter %2 px · %3 stars").arg(Number(tools.controller.advancedReport.estimatedFwhm).toFixed(3)).arg(Number(tools.controller.advancedReport.estimatedScatter).toFixed(3)).arg(tools.controller.advancedReport.estimatedStars)
                    }
                    DarkComboBox {
                        Layout.fillWidth: true
                        model: [qsTr("Circular Gaussian PSF"), qsTr("Circular Moffat PSF")]
                        currentIndex: tools.values.psfProfile || 0
                        enabled: tools.editable
                        onActivated: tools.change("psfProfile", currentIndex)
                    }
                    NumberSetting {
                        key: "psfFwhm"
                        label: qsTr("PSF width · FWHM in pixels")
                        minimum: .5
                        maximum: 12
                        explanation: qsTr("Full width at half maximum of the blur. Gaussian σ = FWHM / 2.35482. Too wide a PSF over-corrects stars; too narrow leaves blur. The finite kernel is normalized to sum to 1.")
                    }
                    NumberSetting {
                        visible: tools.values.psfProfile === 1
                        key: "psfBeta"
                        label: qsTr("Moffat wings · β")
                        minimum: 1.5
                        maximum: 10
                        explanation: qsTr("The profile is (1 + r²/α²)^(−β). Lower β gives stronger wings. α is derived from FWHM; β is manual and is not estimated from stars.")
                    }
                    Canvas {
                        objectName: "deepSkyPsfCanvas"
                        Layout.fillWidth: true
                        Layout.preferredHeight: 110
                        property var kernel: tools.controller.psfKernel
                        onKernelChanged: requestPaint()
                        onWidthChanged: requestPaint()
                        onPaint: {
                            const ctx = getContext("2d");
                            ctx.fillStyle = "#0b1017";
                            ctx.fillRect(0, 0, width, height);
                            const n = Math.round(Math.sqrt(kernel.length));
                            if (!n)
                                return;
                            let max = 0;
                            for (let i = 0; i < kernel.length; i++)
                                max = Math.max(max, kernel[i]);
                            const side = Math.min(height, width), left = (width - side) / 2;
                            for (let y = 0; y < n; y++)
                                for (let x = 0; x < n; x++) {
                                    const v = Math.round(255 * Math.sqrt(kernel[y * n + x] / max));
                                    ctx.fillStyle = "rgb(" + v + "," + v + "," + v + ")";
                                    ctx.fillRect(left + x * side / n, y * side / n, side / n + 1, side / n + 1);
                                }
                        }
                    }
                    Explanation {
                        text: qsTr("PSF preview uses square-root display scaling; actual kernel values are normalized linear weights.")
                    }
                    NumberSetting {
                        key: "rlIterations"
                        label: qsTr("Maximum iterations")
                        minimum: 1
                        maximum: 100
                        integer: true
                        explanation: qsTr("Each iteration compares the observed image with the current estimate blurred by the PSF, then applies a multiplicative correction. More iterations can sharpen detail but amplify noise and rings.")
                    }
                    NumberSetting {
                        key: "pedestal"
                        label: qsTr("Explicit pedestal · image units")
                        minimum: -1
                        maximum: 1
                        explanation: qsTr("Add this constant after background correction and before scientific tools. If calibration left negative values, a positive pedestal can make Richardson–Lucy input non-negative. The offset is retained in the result and saved; it is not silently removed.")
                    }
                    NumberSetting {
                        key: "rlDamping"
                        label: qsTr("Residual damping · image units")
                        explanation: qsTr("The residual correction is weighted by r²/(r² + damping²). Small residuals are suppressed. 0 gives undamped Richardson–Lucy. This is residual damping, not total-variation regularization.")
                    }
                    NumberSetting {
                        key: "rlStopping"
                        label: qsTr("Early stopping · relative change")
                        maximum: .1
                        explanation: qsTr("Stop when sum(abs(new − old))/sum(abs(old)) falls below this threshold. 0 disables early stopping. The actual iteration count and final change are reported.")
                    }
                    NumberSetting {
                        key: "rlBlend"
                        label: qsTr("Restoration amount")
                        explanation: qsTr("0 preserves the input, 1 uses the restored luminance. A shared luminance factor scales RGB to preserve ratios. Finite non-negative input is required; no hidden pedestal is added.")
                    }
                    Explanation {
                        visible: tools.controller.advancedReport.rlIterations !== undefined
                        text: qsTr("Executed %1 iterations · relative change %2").arg(tools.controller.advancedReport.rlIterations).arg(Number(tools.controller.advancedReport.rlChange || 0).toPrecision(4))
                    }
                }
                ColumnLayout {
                    visible: tools.toolIndex === 2
                    Layout.fillWidth: true
                    Toggle {
                        objectName: "deepSkyAdvancedDenoiseEnabled"
                        key: "advancedDenoise"
                        text: qsTr("Multiscale wavelet denoising")
                    }
                    Explanation {
                        text: qsTr("A B3-spline à trous decomposition separates fine to coarse detail. At each scale, soft thresholding shrinks small coefficients using a robust MAD noise estimate. Luminance and chroma have separate strengths. This is not NL-Bayes or an AI model.")
                    }
                    NumberSetting {
                        key: "noiseScales"
                        label: qsTr("Detail scales")
                        minimum: 1
                        maximum: 5
                        integer: true
                        explanation: qsTr("Filter spacing doubles at each scale: 1, 2, 4, 8, 16 pixels. More scales affect larger structures. Broad diffuse signal can be weakened; compare at 100%.")
                    }
                    NumberSetting {
                        key: "noiseThreshold"
                        label: qsTr("Threshold · robust σ")
                        maximum: 8
                        explanation: qsTr("For detail coefficient d, soft thresholding gives sign(d) × max(abs(d) − threshold × σ, 0). σ = 1.4826 × MAD of that scale. 0 disables shrinkage. Stars and nebulosity can bias the noise estimate.")
                    }
                    NumberSetting {
                        key: "noiseAmount"
                        label: qsTr("Luminance reduction")
                        explanation: qsTr("Blend original and thresholded luminance details at every selected scale. 0 keeps luminance unchanged; 1 uses full thresholding. The coarse residual remains unchanged.")
                    }
                    NumberSetting {
                        key: "noiseChroma"
                        label: qsTr("Chroma reduction")
                        explanation: qsTr("Shrink R − Y and B − Y details separately, then reconstruct green so weighted luminance stays Y. This targets fine color mottling. 0 keeps chroma detail unchanged.")
                    }
                    Explanation {
                        visible: tools.controller.advancedReport.noiseBefore !== undefined
                        text: qsTr("Estimated adjacent-pixel luminance noise: %1 → %2. This is a diagnostic, not a measure of image quality.").arg(Number(tools.controller.advancedReport.noiseBefore || 0).toPrecision(4)).arg(Number(tools.controller.advancedReport.noiseAfter || 0).toPrecision(4))
                    }
                }
                ColumnLayout {
                    visible: tools.toolIndex === 3
                    Layout.fillWidth: true
                    DarkComboBox {
                        id:maskTypeChoice;objectName: "deepSkyMaskType"
                        Layout.fillWidth: true
                        model: [qsTr("No mask"), qsTr("Luminance range"), qsTr("Radial region"), qsTr("Detected stars"),qsTr("Painted mask"),qsTr("Imported mask")]
                        currentIndex: tools.values.maskType || 0
                        enabled: tools.editable && !tools.values.photometry
                        onActivated:{if(currentIndex===5&&!tools.values.maskFile){currentIndex=Qt.binding(()=>tools.values.maskType||0);maskImport.open();}else tools.change("maskType",currentIndex);}
                    }
                    Explanation {
                        text: qsTr("White applies the adjustments, black preserves its input, gray blends them: output = input + mask × (processed − input). The mask uses the recipe input before geometry and is applied once after the selected treatment. Photometric calibration is global and cannot be masked.")
                    }
                    RowLayout {Layout.fillWidth:true;visible:tools.values.maskType===4||tools.values.maskType===5
                        IconButton {objectName:"deepSkyPaintMask";text:qsTr("Paint mask");Layout.fillWidth:true;enabled:tools.editable&&!tools.controller.busy;onClicked:tools.openMaskEditor()}
                        IconButton {text:qsTr("Import mask");enabled:tools.editable&&!tools.controller.busy;onClicked:maskImport.open()}
                    }
                    Label {visible:tools.values.maskType===5;text:tools.values.maskFile||qsTr("Choose a mask image first.");color:Atelier.secondary;elide:Text.ElideMiddle;Layout.fillWidth:true}
                    DarkComboBox {visible:tools.values.maskType!==0;Layout.fillWidth:true;model:[qsTr("Mask all finishing adjustments"),qsTr("Mask multiscale denoising only"),qsTr("Mask deconvolution only"),qsTr("Mask stretch and color only")];currentIndex:tools.values.maskScope||0;Accessible.name:qsTr("Mask scope");onActivated:tools.change("maskScope",currentIndex)}
                    ColumnLayout {
                        visible: tools.values.maskType === 1
                        Layout.fillWidth: true
                        NumberSetting {
                            key: "maskLow"
                            label: qsTr("Lower luminance bound")
                            maximum: Number(tools.values.maskHigh || 1) - .000001
                            explanation: qsTr("Select weighted RGB luminance above this bound. Values use input image units, not the automatically stretched preview. Transition width is controlled by feathering.")
                        }
                        NumberSetting {
                            key: "maskHigh"
                            label: qsTr("Upper luminance bound")
                            minimum: Number(tools.values.maskLow || 0) + .000001
                            explanation: qsTr("Select luminance below this bound. Bright stars can be excluded by lowering it. The bound must stay above the lower bound.")
                        }
                    }
                    ColumnLayout {
                        visible: tools.values.maskType === 2
                        Layout.fillWidth: true
                        NumberSetting {
                            key: "maskX"
                            label: qsTr("Center X · fraction of width")
                            explanation: qsTr("0 is the left edge, 1 the right edge. Coordinates refer to the recipe input before the rotation or mirrors.")
                        }
                        NumberSetting {
                            key: "maskY"
                            label: qsTr("Center Y · fraction of height")
                            explanation: qsTr("0 is the top edge, 1 the bottom edge. Move the center to isolate an object or a background region.")
                        }
                        NumberSetting {
                            key: "maskRadius"
                            label: qsTr("Radius · fraction of shorter side")
                            minimum: .001
                            maximum: 2
                            explanation: qsTr("Circular selection in image pixels. Radius and feathering are expressed relative to the shorter image side, so the circle remains circular on rectangular images.")
                        }
                    }
                    NumberSetting {
                        visible: tools.values.maskType === 3
                        key: "maskStarRadius"
                        label: qsTr("Star radius · FWHM multiplier")
                        minimum: .5
                        maximum: 6
                        explanation: qsTr("Build circular regions around up to 150 detected stars, with radius = measured FWHM × multiplier. Faint or crowded stars may be missed. Invert to protect detected stars while processing the rest.")
                    }
                    NumberSetting {
                        visible: tools.values.maskType > 0 && tools.values.maskType < 4
                        key: "maskFeather"
                        label: qsTr("Feathering")
                        minimum: .001
                        explanation: qsTr("Smoothstep t²(3 − 2t) softens mask edges. For luminance it is an intensity width; for radial regions a fraction of the shorter side; for stars a fraction of each star radius.")
                    }
                    NumberSetting {visible:tools.values.maskType>=4;key:"maskBlurRadius";label:qsTr("Edge blur σ (px)");maximum:100;explanation:qsTr("Gaussian edge smoothing in image pixels. Zero keeps the painted or imported edges.")}
                    Toggle {
                        visible: tools.values.maskType !== 0
                        key: "maskInvert"
                        text: qsTr("Invert mask")
                    }
                    CheckBox {
                        objectName: "deepSkyShowMask"
                        text: qsTr("Show mask on the image")
                        checked: tools.controller.showingMask
                        enabled: !tools.controller.busy && tools.controller.hasMask
                        onClicked: tools.controller.showingMask = checked
                    }
                    Explanation {
                        visible: tools.controller.advancedReport.maskCoverage !== undefined
                        text: qsTr("Mean mask weight: %1%. Soft edges contribute partial weight; this is not a count of fully selected pixels.").arg(Number(tools.controller.advancedReport.maskCoverage || 0).toFixed(2))
                    }
                }
                ColumnLayout {
                    visible: tools.toolIndex === 4
                    Layout.fillWidth: true
                    Explanation {
                        text: qsTr("Local matched-reference color calibration, for linear broadband RGB. No plate solving or automatic catalogue lookup is performed. Reference positions must match this image; reference RGB fluxes must represent the intended filter response. Magnitudes cannot be entered directly.")
                    }
                    Toggle {
                        objectName: "deepSkyPhotometryEnabled"
                        key: "photometry"
                        text: qsTr("Calibrate colors from reference stars")
                        enabled: tools.editable && tools.linearInput && Number(tools.values.maskType || 0) === 0 && (tools.values.photometryReferences || []).length >= 3
                    }
                    IconButton {
                        text: qsTr("Import matched reference CSV")
                        Layout.fillWidth: true
                        enabled: tools.editable
                        onClicked: catalog.open()
                    }
                    Explanation {
                        text: qsTr("CSV header: x,y,red,green,blue. Coordinates are zero-based pixels from the top left. At least three stars are required; imported references are saved inside the project and preset. Current references: %1.").arg((tools.values.photometryReferences || []).length)
                    }
                    NumberSetting {
                        key: "photometryAperture"
                        label: qsTr("Stellar aperture · pixels")
                        minimum: 1
                        maximum: Math.min(30, Number(tools.values.photometryInner || 8) - .01)
                        explanation: qsTr("Sum RGB within this radius, then subtract aperture pixel count × median sky from the annulus. The aperture must contain the stellar flux and avoid nearby stars.")
                    }
                    NumberSetting {
                        key: "photometryInner"
                        label: qsTr("Sky annulus · inner radius")
                        minimum: Number(tools.values.photometryAperture || 5) + .01
                        maximum: Number(tools.values.photometryOuter || 12) - .01
                        explanation: qsTr("Start the sky ring beyond the star's wings. A ring contaminated by nebulosity or neighboring stars biases the measured stellar flux.")
                    }
                    NumberSetting {
                        key: "photometryOuter"
                        label: qsTr("Sky annulus · outer radius")
                        minimum: Number(tools.values.photometryInner || 8) + .01
                        maximum: 80
                        explanation: qsTr("Outer limit of the sky sample ring. Stars whose entire ring does not fit in the image are rejected. Keep the ring local while providing enough sky samples.")
                    }
                    NumberSetting {
                        key: "photometrySaturation"
                        label: qsTr("Saturation threshold · image units")
                        minimum: .000001
                        maximum: 10
                        explanation: qsTr("Reject a reference when any aperture channel reaches this value. Set it from the actual sensor/import scale. Already saturated sensor cores cannot be repaired by calibration.")
                    }
                    Explanation {
                        text: qsTr("For each usable star, compare reference R/G and B/G with measured ratios. Robust median log-ratios give red and blue gains; green stays 1. Outliers beyond 3 robust σ are rejected. This calibrates relative colors, not absolute magnitudes or spectral response.")
                    }
                    Explanation {
                        visible: tools.controller.advancedReport.photometryAccepted !== undefined
                        text: {
                            const r = tools.controller.advancedReport, g = r.photometryGains || [1, 1, 1];
                            return qsTr("References used %1 · rejected %2 · residual scatter %3 mag · RGB gains %4 / %5 / %6").arg(r.photometryAccepted || 0).arg(r.photometryRejected || 0).arg(Number(r.photometryScatter || 0).toFixed(4)).arg(Number(g[0]).toFixed(4)).arg(Number(g[1]).toFixed(4)).arg(Number(g[2]).toFixed(4));
                        }
                    }
                }
            }
        }
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Atelier.border
        }
        Explanation {
            visible: tools.controller.advancedReport.outputMinimum !== undefined
            text: qsTr("Applied output range %1…%2 · channels below 0: %3% · above 1: %4%. Floating FITS preserves these values; display and TIFF/JPEG can clip them.").arg(Number(tools.controller.advancedReport.outputMinimum||0).toPrecision(4)).arg(Number(tools.controller.advancedReport.outputMaximum||0).toPrecision(4)).arg(Number(tools.controller.advancedReport.outputBelowZero||0).toFixed(3)).arg(Number(tools.controller.advancedReport.outputAboveOne||0).toFixed(3))
        }
        Explanation {
            visible: tools.draftError.length > 0 || tools.draftPending
            text: tools.draftError.length > 0 ? tools.draftError : qsTr("Updating the image. Diagnostics describe the last completed result.")
            color: "#edb06c"
        }
        IconButton {
            visible: !tools.livePreview
            Layout.fillWidth: true
            text: qsTr("Update preview")
            enabled: tools.editable
            onClicked: tools.previewRequested()
        }
        RowLayout {
            Layout.fillWidth: true
            BusyIndicator {
                running: tools.controller.busy
                visible: running
                Layout.preferredWidth: 28
                Layout.preferredHeight: 28
            }
            Label {
                text: tools.controller.busy ? tools.controller.stage : tools.controller.status
                color: Atelier.secondary
                wrapMode: Text.Wrap
                Layout.fillWidth: true
                font.pixelSize: 11
            }
            IconButton {
                visible:tools.controller.busy
                text: qsTr("Cancel calculation")
                onClicked: if (tools.controller.busy)
                    tools.controller.cancel()

            }
        }
    }
    FileDialog {
        id: catalog
        title: qsTr("Matched photometric reference stars")
        nameFilters: [qsTr("CSV catalogue (*.csv)")]
        onAccepted: {
            const references = tools.controller.importPhotometryCatalog(selectedFile);
            if (references.length >= 3)
                tools.change("photometryReferences", references);
        }
    }
}
