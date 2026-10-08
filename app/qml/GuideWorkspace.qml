import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Rectangle {
    id: root
    color: "#0b0d10"
    property bool captureActive: false
    property int topic: 0
    signal navigate(string tab)
    readonly property var lessons: [
        { title: qsTr("Start here"), lead: qsTr("One observation, three steps"),
          intro: qsTr("Capture a video, combine its sharpest frames, then finish the image. For the Moon, Sun and planets, this gives you more useful material than a single snapshot."),
          cards: [
            {title: qsTr("1 · Capture in SER"), body: qsTr("Focus carefully, keep exposures short and avoid clipping the bright areas. Record a SER video. Atmospheric turbulence changes from frame to frame; a video gives you a choice of sharper moments.")},
            {title: qsTr("2 · Analyze, select, stack"), body: qsTr("Open the SER in Processing. Analyze ranks the frames by sharpness. Start by retaining 25–50%, then compare results. Align and stack combines the selected information into one image.")},
            {title: qsTr("3 · Finish and export"), body: qsTr("Increase wavelets gradually, adjust color and compare before / after. Export a 16-bit TIFF. Keep the original SER so you can try a different selection later.")},
            {title: qsTr("Observing the Sun"), body: qsTr("Use a solar filter designed for your telescope and observation method before pointing at the Sun. Software and exposure settings do not provide eye or equipment protection.")}
          ], action: qsTr("Go to Capture"), tab: "capture" },
        { title: qsTr("File formats"), lead: qsTr("Choose a format for each stage"),
          intro: qsTr("A video contains many frames. A finished image contains the result of your work. Keep both when you want to be able to start again."),
          cards: [
            {title: qsTr("SER · the recording"), body: qsTr("An uncompressed astronomy video. Use it for capture, sharpness analysis and stacking. AstroCapture opens SER directly; it also opens ACRAW sequences, but does not import AVI.")},
            {title: qsTr("PNG / TIFF · an image to adjust"), body: qsTr("Open a PNG or TIFF directly in Processing, or drop it there. Wavelets, color, comparison and captions are available immediately. There is no frame selection or stacking for a single image. Imports accept single-image files up to 32 MP; transparency is placed on black.")},
            {title: qsTr("TIFF 16-bit · the finished result"), body: qsTr("Processing and Mosaic export 16-bit RGB TIFF files, preserving the precision of the result. A 16-bit file cannot recover information absent from an 8-bit original. JPEG is accepted in Mosaic, but is not a direct Processing input.")},
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
          intro: qsTr("Use the Deep sky workspace for FITS or Bayer RAW sequences. The source samples remain linear; an automatic display stretch makes faint data visible without changing the originals."),
          cards: [
            {title: qsTr("Images and calibration"), body: qsTr("Choose the image type and Add files, or Import folder for a folder and its subfolders. Auto detects roles from metadata and names; verify and edit each role and filter in the image list. Add lights, darks, flats and dark flats for raw data. Raw mode is the default. Enable Lights already calibrated only for preprocessed files, such as calibrated iTelescope FITS. In the Siril tutorial, import Offsets_3s as Dark flats, not Bias. Keep the originals.")},
            {title: qsTr("Stack settings"), body: qsTr("Inspect star counts, FWHM and source previews; uncheck poor frames and choose a reference per filter. Normalization and rejection are configurable in Advanced stack settings. Start Create the stacked image from Stack settings after checking calibration, reference and advanced options. It runs calibration, alignment, normalization and integration together. Finish and export becomes available when a result exists. Hover parameter labels for help.")},
            {title: qsTr("Source or stacked result?"), body: qsTr("The image label tells you whether you are viewing a source or the stacked result. Clicking a file name previews that source without replacing the export result. Use Show stacked result to return. Stacking primarily improves noise; finishing reveals faint detail.")},
            {title: qsTr("Histogram and manual stretch"), body: qsTr("In Finish and export, the linear histogram shows data with the current color gains and the stretch curve. Select Black, Midtones or White, then drag in the graph, or edit numeric levels and the midtones slider. Zoom only changes the graph view. Watch the sampled clipping estimates; moving white can clip bright stars. Asinh offers a different stretch that preserves color ratios.")},
            {title: qsTr("Live preview and before / after"), body: qsTr("Live adjustment preview updates after you move a control and keeps the latest request. Disable it to use Apply adjustments instead. Choose a light frame in the comparison list and hold Raw exposure to compare that original against the finished stack. The raw preview has its own automatic stretch and original framing; exports still use the finished result. Automatic starting point and Reset to linear remain available. Every finish starts from the linear integration, without accumulating stretches.")},
            {title: qsTr("Color, background and export"), body: qsTr("Combine multiple filters as RGB, LRGB, SHO or HOO. Adjust color gains, saturation and smoothing manually. The initial gains estimate a neutral background; they are not photometric calibration. Planar gradient correction is optional and may affect extended nebulosity. Photometric calibration, GHS and deconvolution are not available yet. Export linear 32-bit FITS for further work, or the finished 16-bit TIFF or JPEG. Image finale offers TIFF or JPEG. Details and caption transfers the finished result to Processing with the accepted exposure count, total exposure time and filters for the technical caption.")},
            {title: qsTr("Presets, undo and versions A/B"), body: qsTr("In Finish and export, load or manage your named finishing presets. Save with a new name to duplicate a preset; Replace updates an existing name. Presets contain stretch, levels, color, background and smoothing, but no calibration files or channel composition. Check the histogram because absolute levels depend on the data. Undo and Redo operate separately in A and B; a slider drag is one change. Switch versions to compare two finishes on the same linear stack and framing. Enlarge preview temporarily hides histograms and the image list; Show histograms restores them. Copy duplicates settings into the other version. The final export uses the selected, applied version. With live preview off, apply pending settings before export. Projects save both versions and the selected version, but not undo histories. A new stack or channel composition resets both versions and their histories.")},
            {title: qsTr("Apple Silicon acceleration"), body: qsTr("Large images use automatic GPU acceleration for stretch, color, smoothing and wavelets. Smaller images and unsupported operations keep the CPU. Original data and export precision stay unchanged. No extra setting is required.")},
            {title: qsTr("Capture and projects"), body: qsTr("In Capture, choose Deep sky · FITS sequence, set the camera exposure and frame count, and start. Raw sensor samples and acquisition metadata are saved in individual FITS. Mount tracking and guiding remain external. Use the gear menu to open, save or clear a project. Saving preserves file references, frame selection, stack settings and finishing controls. Reopen the project and recreate the stack to reapply the saved finish, even with live preview disabled. Keep the source files in place.")}
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
            {title: qsTr("Save a result"), body: qsTr("Export from Processing or Mosaic creates a TIFF. An imported source image is protected from overwriting; choose a different file name. Captions are optional, and their title appears above the caption text. Once exported, the caption is part of the image.")},
            {title: qsTr("What appears in History?"), body: qsTr("With automatic history enabled, successful exports from Mosaic and Processing are added, including a mosaic finished in Processing. Capture snapshots are also remembered. Moving between tabs alone does not save an image.")},
            {title: qsTr("Paths, not duplicate files"), body: qsTr("History remembers file locations. Saving again to the same path updates one entry; a new path creates another. You can import existing images, open them or reveal them in Finder. Removing an entry does not delete the image; missing paths are cleaned automatically.")},
            {title: qsTr("Keep enough to start again"), body: qsTr("Keep the SER recordings and the uncaptioned image portions separately from your final exports. A TIFF preserves the image, not an editable project with its frame selection, wavelet settings or assembly history.")}
          ], action: qsTr("Go to History"), tab: "history" }
    ]
    readonly property var lesson: lessons[topic]
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 24; spacing: 16
        Label { text: qsTr("Guide"); color: "#eef3fa"; font.pixelSize: 26 }
        Label { text: qsTr("From the first recording to the finished image, at your own pace."); color: "#9aa7b8"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        Flow {
            Layout.fillWidth: true; spacing: 8
            Repeater {
                model: root.lessons
                IconButton {
                    required property int index; required property var modelData
                    text: modelData.title; borderColor: root.topic===index ? "#6ca5f8" : "#39465a"
                    onClicked: { root.topic=index; lessonScroll.contentItem.contentY=0; }
                }
            }
        }
        ScrollView {
            id: lessonScroll
            objectName: "guideLessonScroll"
            Layout.fillWidth: true; Layout.fillHeight: true
            contentWidth: availableWidth; clip: true
            ScrollBar.vertical.policy: ScrollBar.AlwaysOff
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            ColumnLayout {
                width: lessonScroll.availableWidth; spacing: 14
                Label { text: root.lesson.lead; font.pixelSize: 22; color: "#eef3fa"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                Label { text: root.lesson.intro; color: "#b5c3d4"; wrapMode: Text.WordWrap; Layout.fillWidth: true; bottomPadding: 8 }
                Repeater {
                    model: root.lesson.cards
                    Rectangle {
                        required property var modelData
                        Layout.fillWidth: true; implicitHeight: cardText.implicitHeight+32
                        color: "#141a22"; radius: 10; border.color: "#26303c"
                        ColumnLayout {
                            id: cardText
                            anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 16; spacing: 8
                            Label { text: modelData.title; font.pixelSize: 16; color: "#e3ecf7"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                            Label { text: modelData.body; font.pixelSize: 13; color: "#b5c3d4"; wrapMode: Text.WordWrap; Layout.fillWidth: true; lineHeight: 1.2 }
                        }
                    }
                }
                IconButton { text: root.lesson.action; enabled: !root.captureActive || root.lesson.tab==="capture"; onClicked: root.navigate(root.lesson.tab) }
                Item { Layout.preferredHeight: 8 }
            }
        }
    }
}
