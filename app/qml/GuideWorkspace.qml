import "Atelier.js" as Atelier
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Rectangle {
    id: root
    color: Atelier.canvas
    property bool captureActive: false
    property int topic: 0
    signal navigate(string tab)
    readonly property var lessons: [

        { title: qsTr("Start here"), lead: qsTr("One observation, three steps"),
          intro: qsTr("Capture a video, combine its sharpest frames, then finish the image. For the Moon, Sun and planets, this gives you more useful material than a single snapshot."),
          cards: [
            {title: qsTr("1 · Capture in SER"), body: qsTr("Focus carefully, keep exposures short and avoid clipping the bright areas. Record a SER video. Atmospheric turbulence changes from frame to frame; a video gives you a choice of sharper moments.")},
            {title: qsTr("2 · Analyze, select, stack"), body: qsTr("Open the SER in Processing. Analyze ranks the frames by sharpness. Start by retaining 25–50%, then compare results. Align and stack combines the selected information into one image.")},
            {title: qsTr("3 · Finish and export"), body: qsTr("Increase wavelets gradually, adjust color and compare before / after. Export a 16-bit PNG or TIFF. Keep the original SER so you can try a different selection later.")},
            {title: qsTr("Observing the Sun"), body: qsTr("Use a solar filter designed for your telescope and observation method before pointing at the Sun. Software and exposure settings do not provide eye or equipment protection.")}
          ], action: qsTr("Go to Capture"), tab: "capture" },
        { title: qsTr("File formats"), lead: qsTr("Choose a format for each stage"),
          intro: qsTr("A video contains many frames. A finished image contains the result of your work. Keep both when you want to be able to start again."),
          cards: [
            {title: qsTr("SER · the recording"), body: qsTr("An uncompressed astronomy video. Use it for capture, sharpness analysis and stacking. AstroCapture opens SER directly; it also opens ACRAW sequences, but does not import AVI.")},
            {title: qsTr("PNG / TIFF · an image to adjust"), body: qsTr("Open a PNG or TIFF directly in Processing, or drop it there. Wavelets, color, comparison and captions are available immediately. There is no frame selection or stacking for a single image. Imports accept single-image files up to 32 MP; transparency is placed on black.")},
            {title: qsTr("PNG / TIFF 16-bit · the finished result"), body: qsTr("Processing exports 16-bit RGB PNG or TIFF files, preserving the precision of the result. Mosaic exports TIFF. A 16-bit file cannot recover information absent from an 8-bit original. JPEG is accepted in Mosaic, but is not a direct Processing input.")},
            {title: qsTr("A snapshot in Capture"), body: qsTr("The photo button saves a PNG preview, a 16-bit TIFF or a raw sensor FITS according to the Capture setting. FITS opens in Deep sky. Record SER for planetary stacking or a FITS sequence for deep-sky exposures.")}
          ], action: qsTr("Go to Processing"), tab: "processing" },
        { title: qsTr("Processing"), lead: qsTr("Read the controls, judge the image"),
          intro: qsTr("For a video, select and stack first. For a PNG, TIFF or transferred mosaic, start directly with detail and color adjustments."),
          cards: [
            {title: qsTr("The quality graph"), body: qsTr("Frames are sorted from sharpest to softest, not by time. The best frame is shown at 100%. Move the blue boundary to change how many are retained. A smaller selection favors sharpness; more frames can reduce noise. The score is a guide, not a guarantee.")},
            {title: qsTr("Moon / Sun and turbulence"), body: qsTr("Choose Moon / Sun for local alignment. Start with 96 px areas; use 64 px for fine contrasted detail or 160 px for softer, noisier surfaces. Best frames per area can use different frames for each region. It takes longer and cannot restore detail missing from every frame.")},
            {title: qsTr("Wavelet levels"), body: qsTr("Levels 1–3 affect finer details; levels 4–6 affect broader structures. Start gently with levels 1 and 2, and watch for noise or bright halos. Maximum gains are 50, 20, 20, 10, 10 and 10. These values are specific to AstroCapture.")},
            {title: qsTr("Before, after and playback"), body: qsTr("Hold Before or use Compare to judge your adjustments. In video mode, Play always shows the original recording, not the stack. An image or mosaic has no video playback. Export saves the finished result.")}
          ], action: qsTr("Go to Processing"), tab: "processing" },
        { title: qsTr("Deep sky"), lead: qsTr("From individual exposures to faint detail"),
          intro: qsTr("Use the Deep sky workspace for FITS or Bayer RAW sequences. The source samples remain linear. Color preview debayers Bayer sensor data; Auto display stretch only changes their visibility. Disable these options to inspect the raw mosaic or linear brightness. Calibration always reads the original file."),
          cards: [
            {title: qsTr("Images and calibration"), body: qsTr("Choose the image type and Add files, or Import folder for a folder and its subfolders. Auto detects roles from metadata and names; verify and edit each role and filter in the image list. Add lights, darks, flats and dark flats for raw data. Raw mode is the default. Enable Lights already calibrated only for preprocessed files, such as calibrated iTelescope FITS. In the Siril tutorial, import Offsets_3s as Dark flats, not Bias. Keep the originals.")},
            {title: qsTr("Stack settings"), body: qsTr("Inspect star counts, FWHM and source previews; uncheck poor frames and choose a reference per filter. Normalization and rejection are configurable in Advanced stack settings. Start Create the stacked image from Stack settings after checking calibration, reference and advanced options. It runs calibration, alignment, normalization and integration together. Finish and export becomes available when a result exists. Hover parameter labels for help.")},
            {title: qsTr("Source or stacked result?"), body: qsTr("The image label tells you whether you are viewing a source or the stacked result. Clicking a file name previews that source without replacing the export result. Use Show stacked result to return. Stacking primarily improves noise; finishing reveals faint detail.")},
            {title: qsTr("Histogram and manual stretch"), body: qsTr("In Finish and export, the linear histogram shows data with the current color gains and the stretch curve. Select Black, Midtones or White, then drag in the graph, or edit numeric levels and the midtones slider. Zoom only changes the graph view. Watch the sampled clipping estimates; moving white can clip bright stars. Asinh offers a different stretch that preserves color ratios.")},
            {title: qsTr("Live preview and before / after"), body: qsTr("Adjustments update automatically. Before, Result and Split compare the stack before finishing with the current result; Before is displayed without stretch. The comparison follows rotations and mirrors. To compare a source exposure, open the folder button beside Comparison and hold Raw exposure. Exports use the finished stack.")},
            {title: qsTr("Finishing adjustments"), body: qsTr("Open Stretch, Background, Color, Geometry or Advanced transformations. In smaller windows, choose the family from the selector. Changes apply automatically. Delete these adjustments resets only the selected family; Cmd+Z restores the previous settings. The project gear contains presets for the entire finish. Projects preserve all finishing settings.")},
            {title: qsTr("Color, background and export"), body: qsTr("Combine multiple filters as RGB, LRGB, SHO or HOO. Adjust color gains, saturation and smoothing manually. The initial gains estimate a neutral background; they are not photometric calibration. Gradient extraction uses independent color-channel polynomials (degrees 1 to 4), a configurable grid and manually editable sky samples. Optional degree-1 extraction can run on each calibrated, debayered exposure before stacking. Avoid fitting real nebulosity. Green noise reduction offers four SCNR protection methods and optional CIE lightness preservation. Use it after stretching and color balance; intentional green colors should be preserved. Advanced transformations provides GHS, circular-PSF Richardson-Lucy, multiscale wavelet denoising, generated masks and local matched-reference photometric colors. Automatic astrometry, online catalogues and spectrophotometric calibration are not available. Export linear 32-bit FITS for further work. Classic export opens the shared editor with a choice of PNG, TIFF or JPEG and an optional caption. PNG and TIFF preserve 16-bit detail without lossy compression. The technical caption includes the accepted exposure count, total exposure time and filters. Deep-sky caption settings are saved in the project.")},
            {title: qsTr("GHS: controlled contrast"), body: qsTr("Enable GHS in Advanced transformations; it enables stretch and replaces midtones/asinh. Start with low strength, place SP near the intensity of interest, then adjust local intensity b. LP ≤ SP ≤ HP controls tangent extensions for shadows and highlights. The curve shows normalized input versus output; gray is identity. Independent RGB and shared-luminance RGB blending have different color behavior. Inverse reverses the scalar curve, not clipping, masks or color blending. Adjust the strength gradually.")},
            {title: qsTr("PSF and restoration"), body: qsTr("Work on an unstretched, unrotated recipe input. Measure central stars, review the FWHM and scatter, then explicitly use the Gaussian estimate; Moffat beta remains manual. PSF preview is display-scaled, while its linear kernel sums to 1. Richardson-Lucy restores weighted luminance with a shared RGB factor. Start with 10 iterations, damping 0.001 and stopping 0.0001 in normalized image units. Compare at 100%; rings and amplified noise mean the correction is too strong. Negative input is rejected. An explicit pedestal is available and remains in the result. It is residual-damped restoration, not blind or spatially varying deconvolution.")},
            {title: qsTr("Denoising and masks"), body: qsTr("Multiscale denoising uses a B3-spline à trous decomposition and soft thresholds from per-scale MAD, with separate luminance and chroma strengths. Start with 3 scales, threshold 3, and strengths 0.5. The noise estimate is a diagnostic, not a quality score. Prefer it before stretch and restoration. Luminance, radial and detected-star masks select where the adjustments apply: white processes, black protects, gray blends. Inspect the mask; inverted star masks protect only detected stars. Masks are regenerated from each operation input, applied after processing, and saved as parameters. They are not brush masks or imported bitmap masks.")},
            {title: qsTr("Local photometric references"), body: qsTr("Use linear broadband RGB and a CSV with header x,y,red,green,blue. Supply at least three already-matched stars, zero-based pixel coordinates from the top left, and positive reference fluxes for the intended filters. This does not identify stars or download a catalogue. Aperture photometry subtracts local annulus medians; saturated, edge and inconsistent references are rejected. Median log color ratios produce R/B gains relative to green. Inspect the accepted count, gains and residual scatter. Aperture/annulus defaults are 5/8/12 pixels and saturation 1; adapt them to your stars and data scale. This is relative color calibration, not absolute or spectrophotometric calibration. References are image-specific even when saved in a preset.")},
            {title: qsTr("Presets and undo"), body: qsTr("Finishing presets in the project gear save or restore all finishing families. Use Cmd+Z to undo. Export offers Linear FITS for the original integration and Classic export for PNG, TIFF or JPEG with an optional caption. PNG and TIFF preserve 16-bit detail without lossy compression.")},
            {title: qsTr("Apple Silicon acceleration"), body: qsTr("Large images use automatic GPU acceleration for stretch, color, smoothing and wavelets. Smaller images and unsupported operations keep the CPU. Original data and export precision stay unchanged. No extra setting is required.")},
            {title: qsTr("Capture and projects"), body: qsTr("In Capture, choose Deep sky · FITS sequence, set the camera exposure and frame count, and start. Raw sensor samples and acquisition metadata are saved in individual FITS. Mount tracking and guiding remain external. Use the gear menu to open, save or clear a project. Saving preserves file references, frame selection, stack settings and finishing controls. Reopen the project and recreate the stack to reapply the saved finish. Keep the source files in place.")}
          ], action: qsTr("Go to Deep sky"), tab: "deepSky" },
        { title: qsTr("Judge your result"), lead: qsTr("Reveal detail while preserving the image"),
          intro: qsTr("There is no universal ideal setting. Judge the whole image, then inspect it at 100%. The histogram and comparisons help you spot lost information and processing artifacts."),
          cards: [
            {title: qsTr("Background and faint detail"), body: qsTr("Keep smooth transitions and faint structures. Some fine noise is acceptable. A completely black background can hide nebulosity; heavy smoothing can erase detail or create a plastic appearance. A real nebula may extend across the field: do not force the entire background to be uniform.")},
            {title: qsTr("Highlights and histogram"), body: qsTr("Watch the clipping estimates as you adjust black and white. Avoid removing faint data or turning bright stars and object cores into featureless white areas. The histogram shape depends on the object and framing; there is no universal target shape. Detail already saturated during capture cannot be recovered by lowering brightness.")},
            {title: qsTr("Sharpness and color"), body: qsTr("Increase wavelets gradually. At 100%, check for amplified grain, bright or dark halos, rings and doubled edges around stars, planets and craters. Check that saturation preserves color differences. RGB and narrowband palettes have different goals: SHO or HOO colors do not need to resemble natural RGB colors.")},
            {title: qsTr("A practical comparison"), body: qsTr("Change one control at a time. Compare with a less processed version at similar brightness. Inspect a faint area, a bright area and a detailed area at 100%, then return to the whole image. Reduce the last adjustment slightly: if artifacts disappear while useful detail remains, keep the gentler setting. The deep-sky raw comparison uses independent stretching and original framing; it is a visual reference, not a numerical noise measurement.")},
            {title: qsTr("More contrast is not more signal"), body: qsTr("Stacking can improve signal-to-noise ratio. Stretching makes existing signal visible, and sharpening can amplify both detail and noise. No single sharpness score proves that a finished image is good. Keep the originals and the linear FITS so you can revisit your choices.")}
          ], action: qsTr("Go to Processing"), tab: "processing" },
        { title: qsTr("Mosaics"), lead: qsTr("Build a wider view of the Moon or Sun"),
          intro: qsTr("Photograph overlapping portions using the same setup and a consistent exposure. Aim for 25–40% overlap, with recognizable details shared between neighboring views."),
          cards: [
            {title: qsTr("Prepare each portion"), body: qsTr("For best results, record one SER per portion and stack each one. Export the portions as TIFF without captions and with similar, moderate sharpening. Add these TIFFs to Mosaic; PNG and JPEG also work.")},
            {title: qsTr("Select and assemble"), body: qsTr("Use the checkboxes to include or leave out images. Automatic exclusion retains the largest connected group and leaves other views visible, unchecked. At least two compatible views are needed. A smooth solar edge alone may not provide enough distinctive detail.")},
            {title: qsTr("Brightness and edges"), body: qsTr("Assembly options can balance exposure and soften the outer coverage boundary. Edge softening darkens a narrow strip and protects it from sharpening when sent to Processing. It does not fill missing areas or remove every internal seam.")},
            {title: qsTr("Finish once the mosaic is assembled"), body: qsTr("Send the result to Processing for the final wavelets and color adjustments. Add a caption and title at the end. The number of assembled images follows the mosaic into Processing. Exporting and reopening a TIFF does not restore the assembly settings or edge-protection mask.")}
          ], action: qsTr("Go to Mosaic"), tab: "mosaic" },
        { title: qsTr("Export and History"), lead: qsTr("Keep your originals and find your results"),
          intro: qsTr("Capture files and processed exports have separate destination folders in Settings. These choices are saved locally across application updates."),
          cards: [
            {title: qsTr("Save a result"), body: qsTr("Processing exports PNG or TIFF; Mosaic exports TIFF. An imported source image is protected from overwriting; choose a different file name. Captions are optional, and their title appears above the caption text. Once exported, the caption is part of the image.")},
            {title: qsTr("What appears in History?"), body: qsTr("With automatic history enabled, successful exports from Mosaic and Processing are added, including a mosaic finished in Processing. Capture snapshots are also remembered. Moving between tabs alone does not save an image.")},
            {title: qsTr("Paths, not duplicate files"), body: qsTr("History remembers file locations. Saving again to the same path updates one entry; a new path creates another. You can import existing images, open them or reveal them in Finder. Removing an entry does not delete the image; missing paths are cleaned automatically.")},
            {title: qsTr("Keep enough to start again"), body: qsTr("Keep the SER recordings and the uncaptioned image portions separately from your final exports. A TIFF preserves the image, not an editable project with its frame selection, wavelet settings or assembly history.")}
          ], action: qsTr("Go to History"), tab: "history" }
        ,{ title:qsTr("Studio navigation"),lead:qsTr("One workspace for each task"),
          intro:qsTr("Home offers Capture, Processing and Deep sky, recent saved projects and SER videos, and work still open in this session."),
          cards:[
            {title:qsTr("Move between workspaces"),body:qsTr("The top tabs stay visible. Switching tabs preserves loaded images, framing and controls, and calculations continue in the background. Click the activity label to return to a running task. During recording, Home, Capture and Guide remain available; other workspaces are locked.")},
            {title:qsTr("Return to a project"),body:qsTr("Resume returns to the current session without loading files again. Open reloads a saved deep-sky project; recreate its stack to restore the saved finish. Keep its source files in place. Settings is a workspace in the top navigation. The deep-sky gear manages the project and finishing presets.")},
            {title:qsTr("Deep-sky tools on the right"),body:qsTr("Images, Stack and Finish form the bar above the image. Stack contains calibration and integration settings. Finish unlocks after stacking. Its right inspector contains the input histogram and adjustment families; the image stays on the left with zoom below it. The floppy button opens final-image or linear FITS export. The project gear manages projects and finishing presets. In Images and Stack, the folder button toggles the source list.")}
          ],action:qsTr("Go to Home"),tab:"home"}
        ,{title:qsTr("From Siril"),lead:qsTr("Familiar controls, explicit parameters"),
          intro:qsTr("Keep the same workflow: calibrate, register, stack, then finish. The linear FITS keeps the integration; the final image contains your finishing adjustments."),
          cards:[
            {title:qsTr("Resume the calculated results"),body:qsTr("Home offers Restore session after restarting. It restores calculated images, settings, captions and A/B versions. Saving the session before quitting waits for the latest changes. Keep your source files and project as well.")},
            {title:qsTr("Crop and geometry"),body:qsTr("In Finish, choose Geometry. In Processing, choose Crop, rotate and resize. Draw a rectangle, move it or adjust its handles. X and Y start at zero in the top-left corner. Crop and quarter-turns keep pixel values; free rotation and scaling use the selected interpolation.")},
            {title:qsTr("Local masks"),body:qsTr("Advanced transformations offers automatic, painted and imported masks. White applies the treatment, black protects the input, gray blends. Red shows protected areas. Choose the mask scope. Cmd+Z removes a brush stroke while the editor is open. Imported masks must match the recipe input dimensions.")},
            {title:qsTr("Check the parameter units"),body:qsTr("GHS, PSF, Richardson–Lucy, multiscale denoising and SCNR expose their parameters and help. Values and algorithms are not interchangeable with every Siril tool. Photometry currently uses a CSV of identified reference stars; automatic plate solving and catalog-based PCC/SPCC are still to come.")}
          ],action:qsTr("Go to Deep sky"),tab:"deepSky"}
    ]
    readonly property var lesson: lessons[topic]
    ColumnLayout {
        anchors.top:parent.top;anchors.bottom:parent.bottom;anchors.horizontalCenter:parent.horizontalCenter;anchors.topMargin:20;anchors.bottomMargin:20;width:Math.min(1040,parent.width-40);spacing:16
        Label { text: qsTr("Guide"); color: Atelier.text; font.pixelSize: 20; font.weight:Font.Medium }
        Label { text: qsTr("From the first recording to the finished image, at your own pace."); color: Atelier.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        Flow {
            Layout.fillWidth: true; spacing: 8
            Repeater {
                model: root.lessons
                IconButton {
                    required property int index; required property var modelData
                    text: modelData.title; borderColor: root.topic===index ? Atelier.accent : Atelier.border
                    onClicked: { root.topic=index; lessonScroll.contentItem.contentY=0; }
                }
            }
        }
        ScrollView {
            id: lessonScroll
            objectName: "guideLessonScroll"
            Layout.fillWidth: true; Layout.fillHeight: true
            contentWidth: availableWidth; clip: true
            ScrollBar.vertical.policy: ScrollBar.AsNeeded
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            ColumnLayout {
                width: lessonScroll.availableWidth; spacing: 14
                Label { text: root.lesson.lead; font.pixelSize: 20; color: Atelier.text; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                Label { text: root.lesson.intro; color: Atelier.secondary; wrapMode: Text.WordWrap; Layout.fillWidth: true; bottomPadding: 8 }
                Repeater {
                    model: root.lesson.cards
                    Rectangle {
                        required property var modelData
                        Layout.fillWidth: true; implicitHeight: cardText.implicitHeight+32
                        color: Atelier.surface; radius: 4; border.color: Atelier.border
                        ColumnLayout {
                            id: cardText
                            anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 16; spacing: 8
                            Label { text: modelData.title; font.pixelSize: 16; color: Atelier.text; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                            Label { text: modelData.body; font.pixelSize: 13; color: Atelier.secondary; wrapMode: Text.WordWrap; Layout.fillWidth: true; lineHeight: 1.2 }
                        }
                    }
                }
                IconButton { text: root.lesson.action; enabled: !root.captureActive || root.lesson.tab==="capture"; onClicked: root.navigate(root.lesson.tab) }
                Item { Layout.preferredHeight: 8 }
            }
        }
    }
}
