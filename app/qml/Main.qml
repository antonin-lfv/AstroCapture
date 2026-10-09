import QtQuick
// The native macOS style ignores background/contentItem customization, so
// controls we style ourselves (e.g. IconButton) silently keep their
// OS-appearance-dependent look, which can render near-invisible against our
// always-dark background. Basic is a non-native style that fully honors it.
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Dialogs

ApplicationWindow {
    id: window

    width: 1280
    height: 820
    minimumWidth: 900
    minimumHeight: 640
    visible: true
    title: "AstroCapture" + (Qt.application.version ? " · " + Qt.application.version : "")
    color: "#0b0d10"
    palette.window: "#0b0d10"
    palette.windowText: "#eef3fa"
    palette.base: "#1c2026"
    palette.text: "#eef3fa"
    palette.placeholderText: "#9aa7b8"
    palette.button: "#1c2026"
    palette.buttonText: "#eef3fa"
    palette.highlight: "#6ca5f8"
    palette.highlightedText: "#0b0d10"

    property bool quitApproved: false
    property bool quitSavePending: false
    property string quitExportTarget: ""
    property bool quitExportUsesCaption: false
    readonly property bool quitBusy: previewController.recording || deepSkyController.busy || processingController.busy || mosaicController.busy || captionController.busy
    readonly property bool hasOpenWork: previewController.recording || deepSkyController.frames.length>0 || processingController.loaded || mosaicController.files.length>0 || quitBusy
    onClosing: close => {
        if(quitApproved || !hasOpenWork)return;
        close.accepted=false;
        quitConfirmation.open();
    }
    QuitConfirmation {
        id:quitConfirmation
        hasProject:deepSkyController.frames.length>0
        hasProcessing:processingController.hasResult
        busy:window.quitBusy
        recording:previewController.recording
        preferredTarget:window.currentTab
        exportTargets: {
            let targets=[];
            if(deepSkyController.hasResult)targets.push({id:"deepSky",name:qsTr("Deep sky"),ready:!deepSkyWorkspace.finishDirty});
            if(processingController.hasResult)targets.push({id:"processing",name:qsTr("Processing"),ready:!processingWorkspace.adjustmentsDirty});
            if(mosaicController.hasResult)targets.push({id:"mosaic",name:qsTr("Mosaic"),ready:true});
            return targets;
        }
        onQuitRequested:{window.quitApproved=true;close();window.close();}
        onSaveProjectRequested: {
            if(!deepSkyWorkspace.saveOptions()){saveStatus=deepSkyWorkspace.editError;return;}
            saveStatus="";close();quitProjectSave.open();
        }
        onExportRequested: target => {
            window.quitExportTarget=target;
            const controller=target==="deepSky"?deepSkyController:target==="processing"?processingController:mosaicController;
            const suggestion=appLibrary.suggestedExport(controller.exportUrl);
            quitTiffSave.currentFolder=appLibrary.exportFolderFor(suggestion);
            quitTiffSave.selectedFile=suggestion.toString().replace(/\.[^/.]+$/, ".tif");
            saveStatus="";close();quitTiffSave.open();
        }
        onSaveSettingsRequested: {
            window.quitExportTarget="";saveStatus="";close();window.currentTab="processing";
            window.quitSavePending=true;processingWorkspace.saveCurrentPreset();
        }
    }
    FileDialog {
        id:quitProjectSave;title:qsTr("Save project before quitting")
        fileMode:FileDialog.SaveFile;nameFilters:[qsTr("AstroCapture project (*.astroproject)")];defaultSuffix:"astroproject"
        onAccepted: {
            let path=selectedFile.toString();if(!path.endsWith(".astroproject"))path+=".astroproject";
            deepSkyController.saveProject(path);quitConfirmation.saveStatus=deepSkyController.status;quitConfirmation.open();
        }
        onRejected:quitConfirmation.open()
    }
    FileDialog {
        id:quitTiffSave;title:qsTr("Save finished TIFF before quitting")
        fileMode:FileDialog.SaveFile;nameFilters:[qsTr("16-bit TIFF image (*.tif *.tiff)")];defaultSuffix:"tif"
        onAccepted: {
            let path=selectedFile.toString();if(!/\.tiff?$/i.test(path))path+=".tif";
            window.quitSavePending=true;
            window.quitExportUsesCaption=false;
            if(window.quitExportTarget==="deepSky") {
                const caption=deepSkyWorkspace.captionOptions;
                if(caption.enabled) {
                    if(!captionController.openDocument(deepSkyController.processingDocument())) {
                        window.quitSavePending=false;
                        quitConfirmation.saveStatus=qsTr("Cannot prepare this image for captioned export. Return to Deep sky to export the image.");
                        quitConfirmation.open();return;
                    }
                    window.quitExportUsesCaption=true;captionController.exportAnnotation=caption;captionController.exportImage(path);
                } else deepSkyController.exportImage(path,false);
            }
            else if(window.quitExportTarget==="processing")processingController.exportImage(path);
            else mosaicController.exportImage(path);
        }
        onRejected:quitConfirmation.open()
    }
    Timer {
        interval:150;repeat:true;running:window.quitSavePending
        onTriggered: {
            if(window.quitBusy || processingWorkspace.dialogOpen || deepSkyWorkspace.dialogOpen || mosaicWorkspace.dialogOpen || sharedCaptionEditor.dialogOpen || quitTiffSave.visible || quitProjectSave.visible)return;
            window.quitSavePending=false;
            if(window.quitExportTarget.length>0)quitConfirmation.saveStatus=window.quitExportTarget==="deepSky"?(window.quitExportUsesCaption?captionController.status:deepSkyController.status):window.quitExportTarget==="processing"?processingController.status:mosaicController.status;
            window.quitExportTarget="";quitConfirmation.open();
        }
    }

    property string currentTab: "home"
    readonly property bool textEditing: activeFocusItem instanceof TextInput || activeFocusItem instanceof TextEdit
    readonly property bool processingMode: currentTab === "processing"
    AppLibrary { id: appLibrary; objectName: "appLibrary" }
    ProcessingController { id: processingController }
    ProcessingController { id: captionController }
    CaptionEditor {
        id:sharedCaptionEditor;controller:captionController;library:appLibrary
        previewContent:Component {ProcessingImageItem {controller:captionController}}
        hostWidth:deepSkyWorkspace.width;hostHeight:deepSkyWorkspace.height
    }
    Connections {target:captionController
        function onAnnotationChanged(){deepSkyWorkspace.captionOptions=captionController.exportAnnotation;}
        function onImageExported(url){appLibrary.recordExport(url);}
    }
    MosaicController { id: mosaicController }
    DeepSkyController { id: deepSkyController }
    readonly property string activityTab: previewController.recording?"capture":deepSkyController.busy?"deepSky":mosaicController.busy?"mosaic":processingController.busy && !processingController.seeking?"processing":""
    readonly property string activityText: previewController.recording?qsTr("Recording…"):deepSkyController.busy?qsTr("Deep sky · %1%").arg(deepSkyController.progress):mosaicController.busy?qsTr("Mosaic · processing…"):processingController.busy && !processingController.seeking?qsTr("Processing · %1%").arg(processingController.progress):""
    function navigate(tab) {
        if(sharedCaptionEditor.dialogOpen)return;
        if(previewController.recording && ["home","capture","guide"].indexOf(tab)<0)return;
        currentTab=tab;
    }
    header: StudioNavigation {
        currentTab:window.currentTab;recording:previewController.recording
        activityText:window.activityText;activityTab:window.activityTab
        onNavigate:tab=>window.navigate(tab)
    }
    footer:Rectangle {
        height:30;color:"#10141b"
        Rectangle {anchors.top:parent.top;width:parent.width;height:1;color:"#29313d"}
        RowLayout {
            anchors.fill:parent;anchors.leftMargin:20;anchors.rightMargin:16;spacing:8
            Rectangle {Layout.preferredWidth:6;Layout.preferredHeight:6;radius:3;color:window.activityTab.length>0?"#91b8ff":"#8cbfa1"}
            Label {text:window.activityText||qsTr("Ready");color:"#a2aec0";font.pixelSize:12;Layout.fillWidth:true;elide:Text.ElideRight}
            Label {text:previewController.connected?previewController.cameraName:qsTr("Camera not connected");color:"#a2aec0";font.pixelSize:12;elide:Text.ElideRight;Layout.maximumWidth:window.width*.3}
        }
    }
    HomeWorkspace {
        id:homeWorkspace;anchors.fill:parent;visible:window.currentTab==="home"
        library:appLibrary;recording:previewController.recording
        deepSkyOpen:deepSkyController.frames.length>0;deepSkyCount:deepSkyController.frames.length
        processingOpen:processingController.loaded;processingName:processingController.sourceName
        projectBusy:deepSkyController.busy;processingBusy:processingWorkspace.controlsBusy
        onNavigate:tab=>window.navigate(tab)
        onChooseProject:{window.navigate("deepSky");deepSkyWorkspace.chooseProject();}
        onOpenProject:source=>{window.navigate("deepSky");deepSkyController.loadProject(source);}
        onOpenVideo:source=>{window.navigate("processing");processingWorkspace.openFile(source);}
    }
    Connections {
        target:deepSkyController
        function onProjectOpened(url){appLibrary.recordProject(url);}
        function onProjectSaved(url){appLibrary.recordProject(url);}
    }
    ProcessingWorkspace {
        id: processingWorkspace
        anchors.fill: parent
        visible: window.processingMode
        controller: processingController
        library: appLibrary
        lastRecording: previewController.recordingPath
        shortcutsAllowed: !window.textEditing && !settingsPanel.opened
    }

    DeepSkyWorkspace {
        id: deepSkyWorkspace
        anchors.fill: parent
        visible: window.currentTab === "deepSky"
        shortcutsAllowed: !window.textEditing && !settingsPanel.opened
        controller: deepSkyController
        library: appLibrary
        canEditResult: !captionController.busy
        onEditResult: {
            const caption=deepSkyWorkspace.captionOptions;
            if(captionController.openDocument(deepSkyController.processingDocument())) {
                captionController.exportAnnotation=caption;
                sharedCaptionEditor.open();
            }
        }
    }
    Connections { target: deepSkyController; function onImageExported(url) { if (/\.(png|tiff?|jpe?g)$/i.test(url.toString())) appLibrary.recordExport(url); } }

    MosaicWorkspace {
        id: mosaicWorkspace
        anchors.fill: parent
        visible: window.currentTab === "mosaic"
        controller: mosaicController
        library: appLibrary
        canEditResult: !processingController.busy && !processingWorkspace.adjustmentsDirty && !processingWorkspace.playing
        onEditResult: {
            if (processingController.openDocument(mosaicController.processingDocument())) window.currentTab = "processing";
        }
    }
    Connections { target: mosaicController; function onImageExported(url) { appLibrary.recordExport(url); } }
    GuideWorkspace {
        anchors.fill: parent
        visible: window.currentTab === "guide"
        captureActive: previewController.recording
        onNavigate: tab => { window.currentTab = tab; }
    }
    GlobalSettingsWorkspace {
        anchors.fill: parent
        visible: window.currentTab === "settings"
        library: appLibrary
        captureController: previewController
    }
    HistoryWorkspace {
        anchors.fill: parent
        visible: window.currentTab === "history"
        library: appLibrary
    }
    Connections {
        target: processingController
        function onImageExported(url) { appLibrary.recordExport(url); }
    }
    Connections {
        target: previewController
        function onStillImageSaved(url) { appLibrary.rememberImage(url); }
    }

    DropArea {
        id: deepSkyDrop
        anchors.fill:parent;z:99
        enabled:window.currentTab !== "deepSky" && !previewController.recording && !deepSkyController.busy && !deepSkyWorkspace.dialogOpen && !sharedCaptionEditor.dialogOpen && !processingWorkspace.dialogOpen && !mosaicWorkspace.dialogOpen
        function accepts(event) {return event.hasUrls && event.urls.length>0 && event.urls.every(url => /^file:/i.test(url.toString()) && /\.(fits?|fts|fz|cr2|cr3|nef|nrw|arw|dng|orf|raf|rw2|pef)$/i.test(url.toString()));}
        onEntered:drag=>{drag.accepted=accepts(drag);}
        onDropped:drop=>{if(!accepts(drop))return;window.currentTab="deepSky";deepSkyController.addFiles(drop.urls);drop.acceptProposedAction();}
        Rectangle {anchors.fill:parent;anchors.margins:8;visible:parent.containsDrag;color:"#e6111c2c";border.color:"#6ca5f8";radius:8
            Label {anchors.centerIn:parent;text:qsTr("Drop to open in Deep sky");color:"#e8edf5";font.pixelSize:24}
        }
    }

    DropArea {
        id: serDrop
        anchors.fill: parent
        z: 100
        enabled: !previewController.recording && !processingController.busy && !processingWorkspace.dialogOpen && !mosaicWorkspace.dialogOpen && !deepSkyWorkspace.dialogOpen && !sharedCaptionEditor.dialogOpen
        function accepts(event) {
            return event.hasUrls && event.urls.length === 1
                && event.urls[0].toString().startsWith("file:")
                && /\.(ser|acraw)$/i.test(event.urls[0].toString());
        }
        onEntered: (drag) => { drag.accepted = accepts(drag); }
        onDropped: (drop) => {
            if (!enabled || !accepts(drop)) { drop.accepted = false; return; }
            window.currentTab = "processing";
            processingWorkspace.openFile(drop.urls[0]);
            drop.acceptProposedAction();
        }
        Rectangle {
            anchors.fill: parent
            anchors.margins: 8
            visible: serDrop.containsDrag
            color: "#e6111c2c"
            border.color: "#6ca5f8"
            border.width: 2
            radius: 8
            Label {
                anchors.centerIn: parent
                text: qsTr("Drop to open in Processing")
                color: "#e8edf5"
                font.pixelSize: 24
            }
        }
    }

    Shortcut {
        sequence: StandardKey.Open
        enabled: !previewController.recording && !processingController.busy && !window.textEditing && !settingsPanel.opened && !processingWorkspace.dialogOpen && !mosaicWorkspace.dialogOpen && !deepSkyWorkspace.dialogOpen && !sharedCaptionEditor.dialogOpen
        onActivated: { window.currentTab = "processing"; processingWorkspace.chooseSource(); }
    }
    Repeater {
        model: ["home", "capture", "processing", "deepSky", "mosaic", "history", "guide", "settings"]
        delegate: Item {
            required property string modelData
            required property int index
            Shortcut {
                sequence: "Ctrl+" + (index + 1)
                enabled: !previewController.recording && !window.textEditing && !settingsPanel.opened && !processingWorkspace.dialogOpen && !mosaicWorkspace.dialogOpen && !deepSkyWorkspace.dialogOpen && !sharedCaptionEditor.dialogOpen
                onActivated: window.navigate(modelData)
            }
        }
    }
    // Frame counters can run into the tens of thousands during long
    // sequences; abbreviate past 1000 (1k, 10.1k, 1.2M) so the sidebar
    // stays readable instead of growing a wide raw integer.
    function formatCount(n) {
        if (n >= 1000000) {
            const v = n / 1000000;
            return (Number.isInteger(v) ? v.toFixed(0) : v.toFixed(1)) + "M";
        }
        if (n >= 1000) {
            const v = n / 1000;
            return (Number.isInteger(v) ? v.toFixed(0) : v.toFixed(1)) + "k";
        }
        return n.toString();
    }

    PreviewController {
        id: previewController
    }

    // Gate B: the mock backend is the only enumerable camera today, so
    // "Rescan" both discovers and connects it in one step (AGENT.md 10.6,
    // USER_FLOWS Flow 1). Real discovery/selection lands with the first
    // hardware backend (Gate C).
    //
    // Layout: preview (+ histogram) fills the left side; a fixed-width
    // sidebar on the right holds every control, grouped into labeled
    // sections — the FireCapture/SharpCap-style split used by most
    // planetary capture software, instead of stacking everything in a
    // strip below the preview.
    RowLayout {
        anchors.fill: parent
        visible: previewController.connected && window.currentTab === "capture"
        spacing: 0

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 8

            Item {
                id: previewArea
                Layout.fillWidth: true
                Layout.fillHeight: true

                LivePreviewItem {
                    id: livePreview
                    anchors.fill: parent
                    controller: previewController
                }

                // Center crosshair, aligned to the actual displayed image
                // rect (not previewArea's full, possibly letterboxed
                // bounds) so it stays centered on the frame regardless of
                // aspect ratio.
                Item {
                    x: livePreview.imageRect.x
                    y: livePreview.imageRect.y
                    width: livePreview.imageRect.width
                    height: livePreview.imageRect.height
                    visible: previewController.connected

                    Rectangle {
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.verticalCenter: parent.verticalCenter
                        width: 1
                        height: 18
                        color: "#ffffff90"
                    }
                    Rectangle {
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.verticalCenter: parent.verticalCenter
                        width: 18
                        height: 1
                        color: "#ffffff90"
                    }
                }

                // ROI drag-select: click and drag on the preview to crop
                // to that sub-region — replaces typing X/Y/Width/Height by
                // hand. Coordinates are mapped through whatever's
                // currently displayed (full sensor or an existing ROI),
                // so a drag inside an already-cropped view narrows it
                // further instead of resetting relative to the full
                // sensor. Double-click resets to the full sensor.
                MouseArea {
                    id: roiSelect
                    anchors.fill: parent
                    enabled: previewController.roiSupported && !previewController.recording

                    onDoubleClicked: previewController.resetRoi()

                    property real startFracX: 0
                    property real startFracY: 0

                    function fractionAt(mouse) {
                        const rect = livePreview.imageRect;
                        if (rect.width <= 0 || rect.height <= 0) {
                            return null;
                        }
                        const fx = (mouse.x - rect.x) / rect.width;
                        const fy = (mouse.y - rect.y) / rect.height;
                        return Qt.point(Math.max(0, Math.min(1, fx)), Math.max(0, Math.min(1, fy)));
                    }

                    onPressed: (mouse) => {
                        const frac = fractionAt(mouse);
                        if (!frac) {
                            mouse.accepted = false;
                            return;
                        }
                        startFracX = frac.x;
                        startFracY = frac.y;
                        roiSelectionBox.updateFromFractions(startFracX, startFracY, startFracX, startFracY);
                        roiSelectionBox.visible = true;
                    }

                    onPositionChanged: (mouse) => {
                        if (!roiSelectionBox.visible) return;
                        const frac = fractionAt(mouse);
                        if (!frac) return;
                        roiSelectionBox.updateFromFractions(startFracX, startFracY, frac.x, frac.y);
                    }

                    onReleased: (mouse) => {
                        if (!roiSelectionBox.visible) return;
                        roiSelectionBox.visible = false;
                        const frac = fractionAt(mouse);
                        if (!frac) return;

                        // The currently displayed frame is roiWidth x
                        // roiHeight sensor pixels, offset at roiX/roiY —
                        // map the drag's fractional positions through
                        // that to get absolute sensor pixels.
                        const baseX = previewController.roiX;
                        const baseY = previewController.roiY;
                        const baseW = previewController.roiWidth;
                        const baseH = previewController.roiHeight;

                        const x1 = baseX + startFracX * baseW;
                        const y1 = baseY + startFracY * baseH;
                        const x2 = baseX + frac.x * baseW;
                        const y2 = baseY + frac.y * baseH;

                        let x = Math.min(x1, x2);
                        let y = Math.min(y1, y2);
                        let w = Math.abs(x2 - x1);
                        let h = Math.abs(y2 - y1);

                        // Round to even (Bayer phase) and require a
                        // minimum drag size — an accidental click without
                        // a real drag shouldn't crop to a near-zero ROI.
                        x = Math.floor(x / 2) * 2;
                        y = Math.floor(y / 2) * 2;
                        w = Math.round(w / 2) * 2;
                        h = Math.round(h / 2) * 2;
                        if (w < 16 || h < 16) {
                            return;
                        }

                        previewController.setRoi(x, y, w, h);
                    }
                }

                Rectangle {
                    id: roiSelectionBox
                    visible: false
                    color: "#4d6e93ff"
                    border.color: "#6e93ff"
                    border.width: 1

                    function updateFromFractions(fx1, fy1, fx2, fy2) {
                        const rect = livePreview.imageRect;
                        const px1 = rect.x + Math.min(fx1, fx2) * rect.width;
                        const py1 = rect.y + Math.min(fy1, fy2) * rect.height;
                        const px2 = rect.x + Math.max(fx1, fx2) * rect.width;
                        const py2 = rect.y + Math.max(fy1, fy2) * rect.height;
                        roiSelectionBox.x = px1;
                        roiSelectionBox.y = py1;
                        roiSelectionBox.width = px2 - px1;
                        roiSelectionBox.height = py2 - py1;
                    }
                }
            }

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 90
                Layout.leftMargin: 12
                Layout.rightMargin: 12
                Layout.bottomMargin: 12

                HistogramItem {
                    id: histogramItem
                    anchors.fill: parent
                    controller: previewController
                }

                // Gate D backlog 22.4: min/max/mean of the current frame,
                // overlaid on the histogram rather than adding a separate
                // sidebar row.
                Text {
                    visible: histogramItem.hasStats
                    anchors.top: parent.top
                    anchors.right: parent.right
                    anchors.margins: 4
                    text: qsTr("min ") + histogramItem.minValue + qsTr("  max ") + histogramItem.maxValue + qsTr("  mean ") +
                          histogramItem.meanValue.toFixed(1)
                    color: "#9aa0a6"
                    font.pixelSize: 10
                }
            }
        }

        Rectangle {
            Layout.fillWidth: false
            Layout.fillHeight: true
            Layout.preferredWidth: 1
            color: "#242830"
        }

        Rectangle {
            Layout.preferredWidth: 300
            Layout.fillHeight: true
            color: "#101318"

            ScrollView {
                ScrollBar.vertical.policy: ScrollBar.AlwaysOff
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                id: sidebarScroll
                anchors.fill: parent
                anchors.margins: 16
                clip: true

                ColumnLayout {
                    // A few pixels narrower than availableWidth, not the
                    // full width: the scrollbar is a transient overlay
                    // (appears over content on hover/scroll rather than
                    // reserving its own space), so without this gutter it
                    // covers the last few pixels of right-aligned text.
                    width: sidebarScroll.availableWidth - 8
                    spacing: 16

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Text {
                            text: previewController.cameraName
                            color: "#e8eaed"
                            font.pixelSize: 14
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }

                        IconButton {
                            iconName: "gear"
                            text:qsTr("Capture settings")
                            borderColor: "#242830"
                            onClicked: settingsPanel.open()


                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10

                        Text {
                            text: formatCount(previewController.frameCount) + qsTr(" frames")
                            color: "#9aa0a6"
                            font.pixelSize: 11
                        }

                        Text {
                            text: previewController.fps.toFixed(1) + qsTr(" fps")
                            color: "#9aa0a6"
                            font.pixelSize: 11
                        }

                        HelpIcon {
                            text: qsTr("This is acquisition, not recording: frames received from the camera since ") +
                                  qsTr("connecting. Nothing is written to disk unless you press Record — this can run ") +
                                  qsTr("for hours with no files created and constant memory use (only the latest ") +
                                  qsTr("frame is ever kept).")
                        }

                        Item { Layout.fillWidth: true }

                        Text {
                            text: qsTr("skipped ") + previewController.previewSkippedFrames
                            color: "#5f6368"
                            font.pixelSize: 11
                        }
                    }

                    IconButton {
                        Layout.fillWidth: true
                        text: qsTr("Disconnect")
                        borderColor: "#242830"
                        onClicked: previewController.disconnectCamera()

                        helpText: qsTr("Disconnect this camera so Rescan can pick a different one.")
                    }

                    Rectangle { Layout.fillWidth: true; height: 1; color: "#242830" }

                    SidebarSection {
                        title: qsTr("Capture")

                        TextField {
                            id: targetNameField
                            Layout.fillWidth: true
                            placeholderText: qsTr("Target (optional)")
                            text: previewController.targetName
                            color: "#e8eaed"
                            placeholderTextColor: "#9aa0a6"
                            selectedTextColor: "#e8eaed"
                            selectionColor: "#2a3d6b"
                            font.pixelSize: 12
                            onEditingFinished: previewController.targetName = text
                            background: Rectangle {
                                implicitHeight: 28
                                radius: 6
                                color: "#1c2026"
                                border.color: "#3a4048"
                                border.width: 1
                            }
                        }

                        DarkComboBox {
                            Layout.fillWidth: true
                            model: [qsTr("Planetary · SER"), qsTr("Deep sky · FITS sequence"), qsTr("Raw sequence · ACRAW")]
                            currentIndex: previewController.videoFormat === "FITS" ? 1 : previewController.videoFormat === "ACRAW" ? 2 : 0
                            enabled: !previewController.recording
                            onActivated: previewController.videoFormat = currentIndex === 1 ? "FITS" : currentIndex === 2 ? "ACRAW" : "SER"
                        }
                        RowLayout {
                            visible: previewController.videoFormat === "FITS"
                            Layout.fillWidth: true
                            DarkComboBox {Layout.fillWidth: true; model:["Light","Dark","Flat","Bias","DarkFlat"]; enabled:!previewController.recording; onActivated:previewController.captureType=currentText}
                            DarkComboBox {Layout.fillWidth: true;model:["", "R", "G", "B", "L", "HA", "OIII", "SII"];enabled:!previewController.recording;onActivated:previewController.captureFilter=currentText}
                        }
                        Text {
                            visible: previewController.videoFormat === "FITS"
                            text: qsTr("Set exposure and frame count, then start. Raw pixels are saved before demosaicing. Camera limits still apply; mount guiding is external.")
                            color: "#9aa0a6"; font.pixelSize:11; wrapMode:Text.Wrap; Layout.fillWidth:true
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            IconButton {
                                Layout.fillWidth: true
                                iconName: "camera"
                                text: qsTr("Capture")
                                onClicked: previewController.captureStillImage()

                                helpText: qsTr("Shortcut: C")
                            }

                            IconButton {
                                Layout.fillWidth: true
                                iconName: previewController.recording ? "stop" : "camera"
                                text: previewController.recording ? qsTr("Stop") : qsTr("Record")
                                enabled: previewController.recording || !processingController.busy
                                active: previewController.recording
                                activeBorderColor: "#e5484d"
                                onClicked: previewController.recording ? previewController.stopRecording()
                                                                         : previewController.startRecording()

                                helpText: qsTr("Shortcut: R")
                            }
                        }

                        Text {
                            Layout.fillWidth: true
                            visible: previewController.lastCaptureStatus.length > 0
                            text: previewController.lastCaptureStatus
                            color: "#9aa0a6"
                            font.pixelSize: 11
                            wrapMode: Text.Wrap
                        }
                    }

                    Rectangle { Layout.fillWidth: true; height: 1; color: "#242830" }

                    SidebarSection {
                        title: qsTr("Recording")
                        helpText: "Auto-stops recording once either target is reached (whichever comes first). " +
                                  "0 means unlimited/disabled for that field — set both to 0 to only stop manually."

                        ColumnLayout {
                            Layout.fillWidth: true
                            visible: !previewController.recording
                            spacing: 6

                            Text {
                                text: qsTr("Stop recording after")
                                color: "#9aa0a6"
                                font.pixelSize: 12
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 6
                                DarkSpinBox {
                                    id: frameTargetSpin
                                    // Fixed rather than fillWidth: the
                                    // "frames"/"sec" labels differ in
                                    // length, which made the two spin
                                    // boxes different widths when each
                                    // filled "row minus its own label".
                                    Layout.preferredWidth: sidebarScroll.availableWidth * 0.66
                                    from: 0
                                    to: 1000000
                                    value: previewController.recordingTargetFrameCount
                                    onValueModified: previewController.recordingTargetFrameCount = value
                                }
                                Text { text: qsTr("frames"); color: "#9aa0a6"; font.pixelSize: 11 }
                            }

                            Text { text: qsTr("or"); color: "#5f6368"; font.pixelSize: 11 }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 6
                                DarkSpinBox {
                                    id: durationTargetSpin
                                    Layout.preferredWidth: sidebarScroll.availableWidth * 0.66
                                    from: 0
                                    to: 36000
                                    value: previewController.recordingTargetDurationSeconds
                                    onValueModified: previewController.recordingTargetDurationSeconds = value
                                }
                                Text { text: qsTr("sec"); color: "#9aa0a6"; font.pixelSize: 11 }
                            }

                            Text {
                                text: qsTr("0 = unlimited")
                                color: "#5f6368"
                                font.pixelSize: 10
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            visible: previewController.recording
                            spacing: 6

                            RowLayout {
                                spacing: 8
                                Rectangle { width: 8; height: 8; radius: 4; color: "#e5484d" }
                                Text {
                                    text: previewController.recordingElapsedSeconds.toFixed(1) + qsTr(" s")
                                    color: "#e8eaed"
                                    font.pixelSize: 13
                                    font.weight: Font.DemiBold
                                }
                            }

                            Text {
                                text: qsTr("Recorded: ") + formatCount(previewController.recordingFrameCount)
                                color: "#9aa0a6"
                                font.pixelSize: 12
                            }

                            // Never silent (ROADMAP.md Gate D: "no silent
                            // dropped frames") — shown even at zero so a
                            // sudden increase is noticeable, red once it
                            // happens.
                            Text {
                                text: qsTr("Dropped: ") + previewController.recordingDroppedFrames
                                color: previewController.recordingDroppedFrames > 0 ? "#e5484d" : "#9aa0a6"
                                font.pixelSize: 12
                            }

                            // Gate D backlog 22.4: write fps falling behind
                            // capture fps is what recordingDroppedFrames
                            // rising actually means — shown side by side so
                            // the cause is visible, not just the count.
                            Text {
                                property real writeFps: previewController.recordingElapsedSeconds > 0
                                                         ? previewController.recordingFrameCount /
                                                           previewController.recordingElapsedSeconds
                                                         : 0
                                text: qsTr("Write: ") + writeFps.toFixed(1) + qsTr(" fps  (capture: ") +
                                      previewController.fps.toFixed(1) + " fps)"
                                color: writeFps < previewController.fps * 0.9 ? "#e5484d" : "#9aa0a6"
                                font.pixelSize: 12
                            }
                        }

                        Text {
                            Layout.fillWidth: true
                            visible: !previewController.recording && previewController.recordingStatus.length > 0
                            text: previewController.recordingStatus
                            color: "#9aa0a6"
                            font.pixelSize: 11
                            wrapMode: Text.Wrap
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        height: 1
                        color: "#242830"
                        visible: previewController.exposureSupported || previewController.gainSupported ||
                                 previewController.extraControls.length > 0
                    }

                    // Capability-driven: the whole section (and each
                    // control within it) is hidden for backends that don't
                    // expose it.
                    SidebarSection {
                        title: qsTr("Camera Controls")
                        visible: previewController.exposureSupported || previewController.gainSupported ||
                                 previewController.extraControls.length > 0

                        ColumnLayout {
                            Layout.fillWidth: true
                            visible: previewController.exposureSupported
                            spacing: 4

                            RowLayout {
                                Layout.fillWidth: true
                                Text { text: qsTr("Exposure"); color: "#9aa0a6"; font.pixelSize: 12; Layout.fillWidth: true }
                                Text {
                                    text: (previewController.exposureMicroseconds / 1000).toFixed(1) + qsTr(" ms")
                                    color: "#e8eaed"
                                    font.pixelSize: 12
                                }
                            }

                            Slider {
                                id: exposureSlider
                                Layout.fillWidth: true
                                // Logarithmic: the min/max range spans 100us
                                // to tens of seconds on real cameras (five
                                // orders of magnitude), so a linear slider
                                // put nearly the whole useful sub-100ms
                                // planetary-imaging range into a couple of
                                // pixels of travel — log space gives every
                                // decade equal room instead.
                                from: Math.log10(previewController.exposureMinMicroseconds)
                                to: Math.log10(previewController.exposureMaxMicroseconds)
                                value: Math.log10(Math.max(previewController.exposureMicroseconds,
                                                            previewController.exposureMinMicroseconds))
                                onMoved: previewController.exposureMicroseconds = Math.min(
                                             previewController.exposureMaxMicroseconds,
                                             Math.max(previewController.exposureMinMicroseconds, Math.pow(10, value)))
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            visible: previewController.gainSupported
                            spacing: 4

                            RowLayout {
                                Layout.fillWidth: true
                                Text { text: qsTr("Gain"); color: "#9aa0a6"; font.pixelSize: 12; Layout.fillWidth: true }
                                Text {
                                    text: previewController.gainValue
                                    color: "#e8eaed"
                                    font.pixelSize: 12
                                }
                            }

                            Slider {
                                id: gainSlider
                                Layout.fillWidth: true
                                from: previewController.gainMin
                                to: previewController.gainMax
                                stepSize: 1
                                value: previewController.gainValue
                                onMoved: previewController.gainValue = Math.round(value)
                            }
                        }

                        // Gate D backlog 22.3: gamma/white balance/
                        // brightness/frame rate limit, whichever the
                        // connected backend actually reports as supported.
                        Repeater {
                            model: previewController.extraControls

                            delegate: ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 4

                                RowLayout {
                                    Layout.fillWidth: true
                                    Text {
                                        text: qsTranslate("CameraControls", modelData.label)
                                        color: "#9aa0a6"
                                        font.pixelSize: 12
                                        Layout.fillWidth: true
                                    }
                                    Text {
                                        // extraSlider.value, not
                                        // modelData.value: the controller
                                        // deliberately doesn't re-notify
                                        // extraControls per drag update
                                        // (see setExtraControlValue's
                                        // comment) since that would rebuild
                                        // this whole Repeater mid-drag, so
                                        // this must track the slider's own
                                        // live value instead to stay
                                        // current while dragging.
                                        text: (modelData.isFloat ? extraSlider.value.toFixed(2)
                                                                  : Math.round(extraSlider.value)) +
                                              (modelData.unit.length > 0 ? " " + modelData.unit : "")
                                        color: "#e8eaed"
                                        font.pixelSize: 12
                                    }
                                }

                                Slider {
                                    id: extraSlider
                                    Layout.fillWidth: true
                                    from: modelData.min
                                    to: modelData.max
                                    stepSize: modelData.step
                                    value: modelData.value
                                    onMoved: previewController.setExtraControlValue(modelData.key, value)
                                }
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        height: 1
                        color: "#242830"
                        visible: previewController.roiSupported
                    }

                    // ROI/crop (ROADMAP.md Gate D backlog 22.1): read out a
                    // sub-rectangle of the sensor instead of the full
                    // frame. Selected by click-and-drag directly on the
                    // preview (previewArea's roiSelect MouseArea below)
                    // rather than numeric fields — much more direct than
                    // typing coordinates for what's fundamentally a
                    // "draw a box around what I want" action.
                    SidebarSection {
                        title: qsTr("Sensor ROI")
                        helpText: "Reads out only a sub-rectangle of the sensor instead of the full frame, which " +
                                  "can raise achievable fps. Click and drag on the preview to select the region " +
                                  "— dragging again inside an existing selection narrows it further. " +
                                  "Double-click, or \"Full frame\", resets it."
                        visible: previewController.roiSupported
                        enabled: !previewController.recording

                        Text {
                            Layout.fillWidth: true
                            text: previewController.roiWidth + " × " + previewController.roiHeight + qsTr(" of ") +
                                  previewController.sensorWidth + " × " + previewController.sensorHeight
                            color: "#9aa0a6"
                            font.pixelSize: 11
                        }

                        IconButton {
                            Layout.fillWidth: true
                            iconName: "close"
                            text: qsTr("Full frame")
                            visible: previewController.roiWidth < previewController.sensorWidth ||
                                     previewController.roiHeight < previewController.sensorHeight
                            onClicked: previewController.resetRoi()
                        }
                    }

                    Item { Layout.fillHeight: true }
                }
            }
        }
    }

    ColumnLayout {
        anchors.centerIn: parent
        visible: !previewController.connected && window.currentTab === "capture"
        spacing: 20
        width: Math.min(420, window.width - 80)

        Image {
            Layout.alignment: Qt.AlignHCenter
            source: "assets/logo.png"
            sourceSize.width: 112
            sourceSize.height: 112
            fillMode: Image.PreserveAspectFit
            smooth: true
        }

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("No camera connected")
            color: "#e8eaed"
            font.pixelSize: 22
            font.weight: Font.DemiBold
        }

        Text {
            Layout.alignment: Qt.AlignHCenter
            Layout.fillWidth: true
            text: previewController.errorMessage.length > 0
                  ? previewController.errorMessage
                  : qsTr("Connect a supported planetary camera to start a live preview.")
            color: previewController.errorMessage.length > 0 ? "#e8776a" : "#9aa0a6"
            font.pixelSize: 14
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
        }

        IconButton {
            id: rescanButton
            Layout.alignment: Qt.AlignHCenter
            iconName: "refresh"
            text: qsTr("Rescan for cameras")
            onClicked: previewController.rescan()

            helpText: qsTr("Looks for a connected camera. Offers the mock test camera if none is found.")
        }

        // Only populated when Rescan finds more than one real camera —
        // with zero or one, it connects directly and this stays empty.
        ColumnLayout {
            Layout.alignment: Qt.AlignHCenter
            Layout.fillWidth: true
            visible: previewController.availableCameras.length > 0
            spacing: 8

            Text {
                Layout.alignment: Qt.AlignHCenter
                text: qsTr("Multiple cameras found — choose one:")
                color: "#9aa0a6"
                font.pixelSize: 12
            }

            Repeater {
                model: previewController.availableCameras

                delegate: IconButton {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.fillWidth: true
                    iconName: "camera"
                    text: modelData.displayName
                    onClicked: previewController.connectToCamera(modelData.id)
                }
            }
        }

        Text {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 24
            text: qsTr("Development build")
            color: "#5f6368"
            font.pixelSize: 11
        }
    }

    IconButton {
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: 12
        visible: !previewController.connected && window.currentTab === "capture"
        iconName: "gear"
        text:qsTr("Capture settings")
        borderColor: "#242830"
        onClicked: settingsPanel.open()


    }

    SettingsPanel {
        id: settingsPanel
        controller: previewController
    }

    // rescan() no longer connects the mock camera silently when no real
    // camera is found — that made a genuine real-camera failure look
    // identical to "nothing plugged in" (see
    // docs/development/NEXIMAGE5_HARDWARE_NOTES.md's bring-up notes).
    // It emits mockCameraOffered() instead and waits for this to confirm.
    Popup {
        id: mockConfirm
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        focus: true
        padding: 16
        // Popup auto-sizes from contentItem.implicitWidth, which a fixed
        // width: on a plain (non-contentItem) child ColumnLayout doesn't
        // feed — the Popup was staying at its default size while the
        // ColumnLayout rendered wider than it, so the buttons visually
        // spilled outside the dark modal background. Setting width here
        // (and having the ColumnLayout below fill it via parent.width,
        // same as SettingsPanel.qml's root Popup) makes the Popup's size
        // authoritative instead.
        width: 332

        background: Rectangle {
            color: "#15181d"
            radius: 10
            border.color: "#3a4048"
            border.width: 1
        }

        Connections {
            target: previewController
            function onMockCameraOffered() {
                mockConfirm.open();
            }
        }

        ColumnLayout {
            width: parent.width
            spacing: 14

            Text {
                text: qsTr("No real camera found.")
                color: "#e8eaed"
                font.pixelSize: 14
                font.weight: Font.DemiBold
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }

            Text {
                text: qsTr("Use the mock test camera instead?")
                color: "#9aa0a6"
                font.pixelSize: 12
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Item { Layout.fillWidth: true }

                IconButton {
                    text: qsTr("Cancel")
                    onClicked: mockConfirm.close()
                }

                IconButton {
                    text: qsTr("Use mock camera")
                    onClicked: {
                        previewController.connectMock();
                        mockConfirm.close();
                    }
                }
            }
        }
    }

    // Gate D backlog 22.7: capture sessions are often run with the mouse
    // busy on framing/focus. Disabled while a text field has focus or the
    // settings popup is open, so typing "c"/"r" (e.g. into a target name)
    // doesn't accidentally trigger them.
    Shortcut {
        sequence: "C"
        enabled: window.currentTab === "capture" && !processingController.busy && previewController.connected && !targetNameField.activeFocus && !settingsPanel.opened
        onActivated: previewController.captureStillImage()
    }

    Shortcut {
        sequence: "R"
        enabled: window.currentTab === "capture" && !processingController.busy && previewController.connected && !targetNameField.activeFocus && !settingsPanel.opened
        onActivated: previewController.recording ? previewController.stopRecording()
                                                   : previewController.startRecording()
    }

}
