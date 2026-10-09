import "Atelier.js" as Atelier
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Dialogs

Rectangle {
    id: root
    required property ProcessingController controller
    required property AppLibrary library
    property string lastRecording: ""
    property bool surfaceMode: false
    property int surfacePatchSize: 96
    property bool localSelection: true
    property bool playing: false
    property bool adjustmentsDirty: false
    property int adjustmentRevision: 0
    property int submittedRevision: 0
    readonly property bool processingJob: controller.busy && !controller.seeking
    // Keep controls in one state for the whole gesture/playback, including gaps between reads.
    readonly property bool controlsBusy: controller.busy || playing || timeline.pressed

    property bool restoringControls: false
    property real panelWidth: library.processingPanelWidth
    property bool compareImages: false
    property bool holdBefore: false
    property real compareSplit: 0.5
    property bool shortcutsAllowed: true
    readonly property bool canEdit: controller.hasResult && !playing && (!controller.busy || controller.adjusting)
    readonly property bool dialogOpen: processingMenu.opened || presetPopup.opened || annotationPopup.dialogOpen || openDialog.visible
    readonly property bool localShortcuts: visible && shortcutsAllowed && !processingMenu.opened && !presetPopup.opened && !annotationPopup.dialogOpen && !openDialog.visible
    ProcessingEdits { id: edits; objectName: "processingEdits" }
    Connections {
        target: edits
        function onValuesChanged() { if (!root.restoringControls && root.controller.hasResult) root.scheduleAdjustments(); }
    }
    function setAnnotation(key, value) {
        let options = Object.assign({}, controller.exportAnnotation);
        options[key] = value;
        controller.exportAnnotation = options;
    }
    function scheduleAdjustments() {
        adjustmentsDirty = true;
        ++adjustmentRevision;
        adjustmentTimer.restart();
    }
    function saveCurrentPreset() {playing=false;edits.endEdit();presetPopup.open();}
    function chooseSource() {
        playing = false;
        openDialog.currentFolder = library.lastVideoDirectory;
        openDialog.open();
    }
    function openFile(url) {
        playing = false;
        adjustmentTimer.stop();
        controller.open(url);
    }
    function togglePlayback() {
        if (!controller.loaded || adjustmentsDirty || (controller.busy && !controller.seeking)) return;
        playing = !playing;
        if (playing) controller.seek(controller.frameIndex);
    }
    function stepFrame(delta) {
        if (playing || adjustmentsDirty || !controller.loaded || (controller.busy && !controller.seeking)) return;
        controller.seek(Math.max(0, Math.min(controller.frameCount - 1, controller.frameIndex + delta)));
    }
    color: Atelier.canvas
    DropArea {
        anchors.fill: parent; z: 100
        enabled: root.visible && !root.controlsBusy && !root.dialogOpen
        function accepts(event) {
            return event.hasUrls && event.urls.length===1 && /^file:/i.test(event.urls[0].toString())
                && /\.(png|tiff?)$/i.test(event.urls[0].toString());
        }
        onEntered: drag => { drag.accepted=accepts(drag); }
        onDropped: drop => { if(!accepts(drop))return; root.openFile(drop.urls[0]); drop.acceptProposedAction(); }
        Rectangle {
            anchors.fill: parent; anchors.margins: 8; radius: 8
            visible: parent.containsDrag; color: "#e6111c2c"; border.color: Atelier.accent; border.width: 2
            Label { anchors.centerIn: parent; text: qsTr("Drop to adjust this image"); color: Atelier.text; font.pixelSize: 24 }
        }
    }
    function resetControls() {
        restoringControls = true;
        adjustmentTimer.stop();
        edits.resetSession();
        restoringControls = false;
        adjustmentsDirty = false;
        compareImages = false; holdBefore = false;
    }
    readonly property var sessionState: ({values:edits.values, compare:compareImages, split:compareSplit, keep:keep.value,
        surface:surfaceMode, local:localSelection, patch:surfacePatchSize, fit:imageView.fitMode, zoom:imageView.zoom, panX:imageView.panX, panY:imageView.panY})
    function restoreSessionState(state) {
        restoringControls=true;playing=false;adjustmentTimer.stop();edits.resetSession();
        edits.applyValues(state.values || controller.appliedSettings);
        surfaceMode=!!state.surface;localSelection=state.local!==false;surfacePatchSize=state.patch===64||state.patch===160?state.patch:96;
        compareImages=state.compare || false;compareSplit=state.split===undefined?.5:state.split;
        if(state.keep!==undefined)keep.value=state.keep;
        imageView.fitMode=state.fit!==false;imageView.zoom=state.zoom || 1;imageView.panX=state.panX || 0;imageView.panY=state.panY || 0;
        adjustmentsDirty=false;restoringControls=false;imageView.constrainPan();
    }
    function applyAdjustments() {
        const values = edits.values;
        let gains = [];
        for (let i = 1; i <= 6; ++i) gains.push(values["level" + i]);
        submittedRevision = adjustmentRevision;
        controller.apply(gains, values.denoise, values.red, values.green, values.blue, values.gamma);
    }
    Shortcut { sequence: "Space"; enabled: root.localShortcuts && root.controller.loaded; onActivated: root.togglePlayback() }
    Shortcut { sequence: "Left"; enabled: root.localShortcuts && root.controller.loaded; onActivated: root.stepFrame(-1) }
    Shortcut { sequence: "Right"; enabled: root.localShortcuts && root.controller.loaded; onActivated: root.stepFrame(1) }
    Shortcut { sequences: [StandardKey.Undo]; enabled: root.localShortcuts && root.canEdit && edits.canUndo; onActivated: edits.undo() }
    Shortcut { sequences: [StandardKey.Redo]; enabled: root.localShortcuts && root.canEdit && edits.canRedo; onActivated: edits.redo() }
    onVisibleChanged: if (!visible) playing = false
    Connections {
        target: root.controller
        function onChanged() { if (!root.controller.loaded) root.playing = false; }
        function onSourceOpened() { root.resetControls(); imageView.fit(); root.library.recordVideo(root.controller.sourceUrl); keep.value = Math.max(1, Math.round(root.controller.frameCount * 0.5)); }
        function onFailed() { root.playing = false; }
        function onStackReady() { root.resetControls(); imageView.fit(); }
        function onAdjustmentsApplied() {
            if (root.submittedRevision === root.adjustmentRevision) root.adjustmentsDirty = false;
        }
    }
    Timer {
        id: adjustmentTimer
        interval: 180
        onTriggered: {
            if (root.controller.hasResult && (!root.controller.busy || root.controller.adjusting))
                root.applyAdjustments();
        }
    }
    Timer {
        interval: 100
        repeat: true
        running: root.playing && root.visible
        onTriggered: {
            if (!root.controller.busy && root.controller.loaded)
                root.controller.seek((root.controller.frameIndex + 1) % root.controller.frameCount);
        }
    }
    FileDialog {
        id: openDialog
        title: qsTr("Open a video or image")
        nameFilters: [qsTr("Videos and images (*.ser *.acraw *.SER *.png *.PNG *.tif *.TIF *.tiff *.TIFF)")]
        onAccepted: root.openFile(selectedFile)
    }
    Popup {
        id: presetPopup
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(420, root.width - 32)
        padding: 18
        modal: true
        focus: true
        background: Rectangle { color: Atelier.surface; radius: 10; border.color: Atelier.border }
        ColumnLayout {
            width: parent.width
            spacing: 12
            Label { text: qsTr("Processing presets"); color: Atelier.text; font.pixelSize: 18 }
            Label { text: qsTr("Save your wavelets, denoise, color and gamma settings. Loading a preset can be undone."); color: Atelier.secondary; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            DarkComboBox {id:presetChooser;objectName:"processingPresetChooser";Layout.fillWidth:true;model:root.library.processingPresetNames;displayText:currentIndex>=0?currentText:qsTr("Processing presets");onActivated:presetName.text=currentText}
            IconButton {text:qsTr("Load");Layout.fillWidth:true;enabled:presetChooser.currentIndex>=0 && root.canEdit;onClicked:{edits.applyValues(root.library.processingPreset(presetChooser.currentText));presetPopup.close()}}
            TextField { id: presetName; objectName: "processingPresetName"; Layout.fillWidth: true; placeholderText: qsTr("Preset name"); maximumLength: 80; color: Atelier.text; background: Rectangle { color: Atelier.field; radius: 4; border.color: Atelier.border } }
            Label { visible: root.library.processingPresetNames.indexOf(presetName.text.trim()) >= 0; text: qsTr("This name already exists. Replace will overwrite its saved settings."); color: "#edb06c"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            RowLayout {
                Layout.fillWidth: true
                IconButton { text: qsTr("Cancel"); onClicked: presetPopup.close() }
                IconButton { text: qsTr("Delete"); enabled: root.library.processingPresetNames.indexOf(presetName.text.trim()) >= 0; onClicked: { root.library.removeProcessingPreset(presetName.text.trim()); presetPopup.close(); } }
                Item { Layout.fillWidth: true }
                IconButton {
                    text: root.library.processingPresetNames.indexOf(presetName.text.trim()) >= 0 ? qsTr("Replace") : qsTr("Save")
                    enabled: presetName.text.trim().length > 0
                    onClicked: if (root.library.saveProcessingPreset(presetName.text, edits.values, true)) { presetChooser.currentIndex = root.library.processingPresetNames.indexOf(presetName.text.trim()); presetPopup.close(); }
                }
            }
        }
    }
    component CaptionSwitch: Switch {
        id: control
        indicator: Rectangle {
            implicitWidth: 38; implicitHeight: 22
            x: control.leftPadding; y: (control.height - height) / 2
            radius: 11; color: control.checked ? Atelier.accent : Atelier.border
            Rectangle { width: 16; height: 16; radius: 8; y: 3; x: control.checked ? 19 : 3; color: Atelier.text }
        }
        background: Rectangle { color: "transparent"; radius: 6; border.color: control.visualFocus ? Atelier.text : "transparent" }
    }
    CaptionEditor {
        id: annotationPopup
        controller: root.controller
        library:root.library
        previewContent:Component {ProcessingImageItem {controller:root.controller}}
        hostWidth: root.width; hostHeight: root.height
        onPreviewRequested: root.controller.showResult(false)
    }
    component Caption: Label {
        color: Atelier.muted
        wrapMode: Text.WordWrap
        font.pixelSize: 12
        Layout.fillWidth: true
    }
    component SectionTitle: Label {
        color: Atelier.text
        font.pixelSize: 15
        font.weight: Font.DemiBold
        Layout.topMargin: 14
    }
    component ProcessingButton: IconButton {
        id: jobButton
        property bool working: false
        // Always reserve the indicator's space, including when idle.
        rightPadding: 36
        Item {
            id: spinner
            width: 16; height: 16
            anchors.right: parent.right
            anchors.rightMargin: 10
            anchors.verticalCenter: parent.verticalCenter
            opacity: jobButton.working ? 1 : 0
            Canvas {
                anchors.fill: parent
                onPaint: {
                    const ctx = getContext("2d");
                    ctx.reset();
                    ctx.strokeStyle = Atelier.accent;
                    ctx.lineWidth = 2;
                    ctx.lineCap = "round";
                    ctx.beginPath();
                    ctx.arc(8, 8, 6, 0, Math.PI * 1.5);
                    ctx.stroke();
                }
            }
            RotationAnimator {
                target: spinner
                from: 0; to: 360
                duration: 900
                loops: Animation.Infinite
                running: jobButton.working && jobButton.visible
            }
        }
    }
    component DarkSlider: Slider {
        id: control
        implicitHeight: 28
        background: Rectangle {
            x: control.leftPadding
            y: control.topPadding + control.availableHeight / 2 - height / 2
            width: control.availableWidth
            height: 4
            radius: 2
            color: Atelier.border
            Rectangle { width: control.visualPosition * parent.width; height: parent.height; radius: 2; color: control.enabled ? Atelier.accent : "#445164" }
        }
        handle: Rectangle {
            x: control.leftPadding + control.visualPosition * (control.availableWidth - width)
            y: control.topPadding + control.availableHeight / 2 - height / 2
            implicitWidth: 14; implicitHeight: 14
            radius: 7
            color: control.enabled ? Atelier.accent : "#566170"
        }
    }
    component Adjustment: RowLayout {
        id: adjustment
        property string settingKey: ""
        property string label: ""
        property string help: ""
        property real minimum: 0.5
        property real maximum: 2
        Layout.fillWidth: true
        Label { text: adjustment.label; color: Atelier.secondary; Layout.preferredWidth: 72; font.pixelSize: 12 }
        HelpIcon { text: adjustment.help; visible: adjustment.help.length > 0 }
        DarkSlider {
            Layout.fillWidth: true
            from: adjustment.minimum; to: adjustment.maximum
            value: edits.values[adjustment.settingKey] === undefined ? 0 : edits.values[adjustment.settingKey]
            onPressedChanged: pressed ? edits.beginEdit() : edits.endEdit()
            onMoved: edits.setValue(adjustment.settingKey, value)
        }
        Label { text: Number(edits.values[adjustment.settingKey] || 0).toFixed(adjustment.maximum < 1 ? 3 : 2); color: Atelier.secondary; Layout.preferredWidth: 40; font.pixelSize: 11 }
    }
    AtelierMenu {
        id:processingMenu;parent:root;x:Math.max(12,root.width-width-16);y:60
        AtelierMenuItem {text:qsTr("Processing presets");onTriggered:root.saveCurrentPreset()}
    }
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12
        RowLayout {
            Layout.fillWidth:true;Layout.preferredHeight:44
            ColumnLayout {Layout.fillWidth:true;spacing:2
                Label {text:root.controller.sourceName.replace(/ · \d+ ×.*$/, "") || qsTr("Processing");color:Atelier.text;font.pixelSize:16;font.weight:Font.DemiBold;Layout.fillWidth:true;elide:Text.ElideRight}
                Label {visible:root.controller.imageWidth>0;text:root.controller.imageWidth+" × "+root.controller.imageHeight+" px";color:Atelier.secondary;font.pixelSize:11}
            }
            IconAction {iconName:"gear";text:qsTr("Processing options");enabled:root.controller.hasResult;onClicked:processingMenu.open()}
            IconButton {iconName:"export";text:qsTr("Export");primary:true;enabled:root.controller.hasResult&&!root.controlsBusy&&!root.adjustmentsDirty;onClicked:annotationPopup.open()}
        }
        RowLayout {
            Layout.fillWidth: true
            IconButton {
                text: qsTr("Open image or video"); iconName:"folder"
                enabled: !root.controlsBusy
                onClicked: root.chooseSource()
            }
            IconButton {
                text: qsTr("Last recording")
                enabled: !root.controlsBusy && root.lastRecording.toLowerCase().endsWith(".ser")
                onClicked: { root.playing = false; adjustmentTimer.stop(); root.controller.openPath(root.lastRecording); }
            }
            IconButton {
                text: qsTr("Recent files")
                enabled: !root.controlsBusy && root.library.recentVideos.length > 0
                onClicked: recentMenu.popup()
                Menu {
                    id: recentMenu
                    objectName: "processingRecentMenu"
                    width: Math.min(420, root.width - 40)
                    popupType: Popup.Item
                    padding: 6
                    palette.window: Atelier.surface
                    palette.text: Atelier.text
                    palette.highlight: Atelier.hover
                    palette.highlightedText: "#ffffff"
                    background: Rectangle { color: Atelier.surface; radius: 8; border.color: Atelier.border }
                    Instantiator {
                        model: root.library.recentVideos
                        delegate: MenuItem {
                            id: recentEntry
                            required property var modelData
                            objectName: "processingRecentEntry"
                            implicitHeight: 36
                            hoverEnabled: true
                            leftPadding: 12
                            rightPadding: 12
                            text: modelData.name + (modelData.exists ? "" : qsTr(" · unavailable"))
                            enabled: modelData.exists
                            contentItem: Text {
                                text: recentEntry.text
                                color: recentEntry.enabled ? Atelier.text : Atelier.muted
                                font.pixelSize: 12
                                verticalAlignment: Text.AlignVCenter
                                elide: Text.ElideMiddle
                            }
                            background: Rectangle {
                                radius: 4
                                color: recentEntry.enabled && (recentEntry.highlighted || recentEntry.hovered) ? Atelier.hover : Atelier.surface
                            }
                            onTriggered: root.openFile(modelData.url)
                            DarkToolTip { visible: recentEntry.hovered; text: recentEntry.modelData.path }
                        }
                        onObjectAdded: (index, object) => recentMenu.insertItem(index, object)
                        onObjectRemoved: (index, object) => recentMenu.removeItem(object)
                    }
                }
            }
            Item {Layout.fillWidth:true}
        }
        RowLayout {
            Layout.fillWidth: true
            Layout.minimumHeight: 20
            Layout.preferredHeight: 20
            Layout.maximumHeight: 20
            Label {
                text: root.controller.stillImage ? qsTr("Still image · adjust details, color and caption") : root.controller.loaded ? (root.surfaceMode && root.localSelection ? qsTr("Per area: ") : qsTr("Selected: ")) + keep.value + " / " + root.controller.frameCount + qsTr(" frames (") + (100 * keep.value / root.controller.frameCount).toFixed(1) + "%)" : qsTr("Open a video to stack, or an image to adjust.")
                color: Atelier.accent
                font.pixelSize: 12
                font.weight: Font.DemiBold
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
            Label {
                text: root.controller.stackSummary
                color: Atelier.secondary
                font.pixelSize: 12
                elide: Text.ElideRight
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignRight
                HoverHandler { id: stackSummaryHover }
                ToolTip.visible: stackSummaryHover.hovered && truncated
                ToolTip.text: text
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 12
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                ImageViewport {
                    id: imageView
                    objectName: "processingViewport"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    sourceWidth: root.controller.imageWidth
                    sourceHeight: root.controller.imageHeight
                    ProcessingImageItem {
                        objectName:"processingPreviewImage"
                        parent: imageView.imageContainer
                        anchors.fill: parent
                        controller: root.controller
                        showBefore: root.holdBefore
                        compare: root.compareImages
                        split: root.compareSplit
                    }
                    Item {
                        objectName: "comparisonDivider"
                        parent: imageView.imageContainer
                        z: 1
                        height: parent.height
                        width: 24 / imageView.effectiveZoom
                        x: parent.width * root.compareSplit - width / 2
                        visible: root.compareImages && root.controller.showingResult && !root.holdBefore
                        Rectangle { anchors.horizontalCenter: parent.horizontalCenter; width: 1 / imageView.effectiveZoom; height: parent.height; color: Atelier.accent }
                        Rectangle { anchors.centerIn: parent; width: parent.width; height: 32 / imageView.effectiveZoom; radius: width / 3; color: Atelier.accent; opacity: 0.85 }
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.SplitHCursor
                            onPositionChanged: (mouse) => {
                                if (pressed) root.compareSplit = Math.max(0, Math.min(1, mapToItem(imageView.imageContainer, mouse.x, mouse.y).x / imageView.imageContainer.width));
                            }
                        }
                    }
                    ColumnLayout {
                        anchors.centerIn: parent
                        width: Math.min(360, parent.width - 36)
                        visible: !root.controller.loaded && !root.controller.hasResult
                        Label { text: qsTr("From capture to final image"); color: Atelier.text; font.pixelSize: 22; Layout.fillWidth: true; wrapMode: Text.WordWrap; horizontalAlignment: Text.AlignHCenter }
                        Caption { text: qsTr("Open or drop a SER, PNG or TIFF here. Analyze and stack a video, or adjust an image directly."); horizontalAlignment: Text.AlignHCenter }
                        Caption { text: qsTr("Your original files stay untouched."); horizontalAlignment: Text.AlignHCenter }
                        IconButton {text:qsTr("Open image or video");iconName:"folder";Layout.alignment:Qt.AlignHCenter;onClicked:root.chooseSource()}
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Label { text: root.controller.imageLabel; color: Atelier.secondary; font.pixelSize: 12; Layout.fillWidth: true; elide: Text.ElideRight }
                    IconButton {
                        text: root.controller.stillImage ? qsTr("Show result") : qsTr("Show stacked result")
                        visible: root.controller.hasResult
                        enabled: !root.controlsBusy && !root.adjustmentsDirty
                        onClicked: { root.playing = false; root.controller.showResult(false); }
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    enabled: root.controller.loaded
                    visible: root.controller.loaded && !root.controller.stillImage
                    IconButton {
                        text: root.playing ? qsTr("Pause source") : qsTr("Play source")
                        Layout.preferredWidth: playbackText.width + 50
                        Layout.minimumWidth: playbackText.width + 50
                        Layout.maximumWidth: playbackText.width + 50
                        TextMetrics { id: playbackText; text: qsTr("Pause source"); font.pixelSize: 12 }
                        enabled: !root.adjustmentsDirty && (root.playing || !root.controlsBusy)
                        onClicked: root.togglePlayback()
                        helpText: qsTr("Original SER video, up to 10 frames/s. The stack is a single still image.")
                    }
                    DarkSlider {
                        id: timeline
                        Layout.fillWidth: true
                        from: 0; to: Math.max(1, root.controller.frameCount - 1)
                        stepSize: 1
                        live: true
                        enabled: (!root.controller.busy || root.controller.seeking) && !root.playing && !root.adjustmentsDirty && root.controller.frameCount > 1
                        onMoved: root.controller.seek(Math.round(value))
                        Binding {
                            target: timeline; property: "value"
                            value: root.controller.frameIndex
                            when: !timeline.pressed && !root.controller.seeking
                            restoreMode: Binding.RestoreNone
                        }
                    }
                    Label {
                        id: frameCounter
                        text: (root.controller.loaded ? root.controller.frameIndex + 1 : 0) + " / " + root.controller.frameCount
                        color: Atelier.secondary
                        font.pixelSize: 12
                        font.family: "monospace"
                        horizontalAlignment: Text.AlignRight
                        Layout.preferredWidth: frameCountText.width
                        Layout.minimumWidth: frameCountText.width
                        Layout.maximumWidth: frameCountText.width
                        TextMetrics {
                            id: frameCountText
                            font: frameCounter.font
                            text: root.controller.frameCount + " / " + root.controller.frameCount
                        }
                    }
                }
            }
            Rectangle {
                visible:root.controller.loaded || root.controller.hasResult
                Layout.preferredWidth: 7
                Layout.fillHeight: true
                color: resizeHandle.containsMouse || resizeHandle.pressed ? Atelier.accent : Atelier.border
                MouseArea {
                    id: resizeHandle
                    objectName: "processingResizeHandle"
                    anchors.fill: parent
                    anchors.margins: -4
                    hoverEnabled: true
                    cursorShape: Qt.SplitHCursor
                    property real startX: 0
                    property real startWidth: 0
                    onPressed: (mouse) => { startX = mapToItem(root, mouse.x, mouse.y).x; startWidth = root.panelWidth; }
                    onPositionChanged: (mouse) => {
                        if (pressed) root.panelWidth = Math.max(260, Math.min(600, root.width * 0.55, startWidth - (mapToItem(root, mouse.x, mouse.y).x - startX)));
                    }
                    onReleased: root.library.processingPanelWidth = Math.round(root.panelWidth)
                }
            }
            ScrollView {
                objectName:"processingControls"
                visible:root.controller.loaded || root.controller.hasResult
                ScrollBar.vertical.policy: ScrollBar.AsNeeded
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                Layout.preferredWidth: Math.min(root.panelWidth, root.width * 0.55)
                Layout.fillHeight: true
                clip: true
                contentWidth: availableWidth
                ColumnLayout {
                    width: parent.width
                    spacing: 8
                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: qsTr("Processing controls"); color: Atelier.secondary; Layout.fillWidth: true; elide: Text.ElideRight }
                        IconButton { text: root.library.compactProcessing ? qsTr("Show explanations") : qsTr("Hide explanations"); onClicked: root.library.compactProcessing = !root.library.compactProcessing }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        visible: !root.controller.stillImage
                    SectionTitle { text: qsTr("1   Select the best frames"); Layout.topMargin: 0 }
                    Caption { visible: !root.library.compactProcessing; text: qsTr("Sharpness ranking helps reject blurred frames. Inspect the result: noise and clouds can also affect the score.") }
                    ProcessingButton {
                        text: root.controller.ranked ? qsTr("Analyze again") : qsTr("Analyze video")
                        working: root.controller.analyzing
                        Layout.fillWidth: true
                        enabled: root.controller.loaded && !root.controlsBusy && !root.adjustmentsDirty
                        onClicked: root.controller.analyze()
                    }
                    Canvas {
                        id: qualityGraph
                        Layout.fillWidth: true
                        Layout.preferredHeight: 65
                        visible: root.controller.ranked
                        property var points: root.controller.qualities
                        onPointsChanged: requestPaint()
                        onWidthChanged: requestPaint()
                        onPaint: {
                            let ctx = getContext("2d"); ctx.reset();
                            ctx.fillStyle = "#121820"; ctx.fillRect(0, 0, width, height);
                            if (!points.length) return;
                            ctx.strokeStyle = Atelier.accent; ctx.lineWidth = 2; ctx.beginPath();
                            for (let i = 0; i < points.length; ++i) {
                                let x = i * width / Math.max(1, points.length - 1);
                                let y = height - 5 - points[i] * (height - 10);
                                if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
                            }
                            ctx.stroke();
                            ctx.fillStyle = "rgba(108, 165, 248, 0.15)"; ctx.fillRect(0, 0, width * keep.value / Math.max(1, root.controller.frameCount), height);
                        }
                        Rectangle {
                            x: Math.min(parent.width - width, Math.max(0, parent.width * keep.value / Math.max(1, root.controller.frameCount) - width / 2))
                            width: 2; height: parent.height; color: "#b7d5ff"
                            Rectangle { width: 10; height: 20; radius: 4; anchors.centerIn: parent; color: Atelier.accent }
                        }
                        MouseArea {
                            objectName: "qualitySelectionHandle"
                            anchors.fill: parent
                            enabled: !root.controlsBusy && root.controller.ranked
                            cursorShape: Qt.SizeHorCursor
                            function selectAt(x) { keep.value = Math.max(1, Math.min(root.controller.frameCount, Math.round(x / width * root.controller.frameCount))); }
                            onPressed: mouse => selectAt(mouse.x)
                            onPositionChanged: mouse => { if (pressed) selectAt(mouse.x); }
                            Accessible.role: Accessible.Slider
                            Accessible.name: qsTr("Frames to keep")
                        }
                        Connections { target: keep; function onValueChanged() { qualityGraph.requestPaint(); } }
                    }
                    Caption {
                        visible: root.controller.ranked && !root.library.compactProcessing
                        text: root.surfaceMode && root.localSelection ? qsTr("Global sharpness ranking, best = 100%. The blue boundary sets a count per area; each area selects its own frames.") : qsTr("Left → right: sharpest → softest (sorted, not time). Height: relative sharpness, best = 100%. Shaded area: frames kept.")
                    }
                    Caption { visible: root.controller.ranked; text: qsTr("Drag the blue boundary to choose how many of the best frames to keep.") }
                    RowLayout {
                        Caption { text: qsTr("Frames to keep") }
                        DarkSpinBox {
                            id: keep
                            objectName: "framesToKeep"
                            Layout.preferredWidth: 108
                            from: 1; to: Math.max(1, root.controller.frameCount)
                            value: 1
                            editable: true
                            enabled: !root.controlsBusy
                        }
                    }
                    RowLayout {
                        Repeater {
                            model: [10, 25, 50, 75]
                            delegate: IconButton {
                                required property int modelData
                                text: modelData + "%"
                                borderColor: root.controller.loaded && keep.value === Math.max(1, Math.round(root.controller.frameCount * modelData / 100)) ? Atelier.accent : Atelier.border
                                Layout.fillWidth: true
                                enabled: root.controller.loaded && !root.controlsBusy
                                onClicked: keep.value = Math.max(1, Math.round(root.controller.frameCount * modelData / 100))
                            }
                        }
                    }
                    Caption {
                        visible: !root.library.compactProcessing && !(root.surfaceMode && root.localSelection)
                        text: root.controller.loaded ? qsTr("Keeping ") + Math.round(100 * keep.value / root.controller.frameCount) + qsTr("% of the video. Start at 25–50%: fewer frames favor sharpness; more frames reduce noise but can soften detail.") : qsTr("Analyze first to rank the frames by sharpness.")
                    }
                    SectionTitle { text: qsTr("2   Align and stack") }
                    RowLayout {
                        enabled: !root.controlsBusy
                        IconButton { text: qsTr("Planet"); Layout.fillWidth: true; borderColor: !root.surfaceMode ? Atelier.accent : Atelier.border; onClicked: root.surfaceMode = false }
                        IconButton { text: qsTr("Moon / Sun"); Layout.fillWidth: true; borderColor: root.surfaceMode ? Atelier.accent : Atelier.border; onClicked: root.surfaceMode = true }
                    }
                    Caption { visible: root.surfaceMode; text: qsTr("Local alignment follows surface details to reduce turbulence distortions. Unreliable areas use global alignment. It cannot recover detail lost to blur.") }
                    RowLayout {
                        visible: root.surfaceMode
                        enabled: !root.controlsBusy
                        Repeater {
                            model: [64, 96, 160]
                            delegate: IconButton {
                                required property int modelData
                                text: modelData + " px"
                                Layout.fillWidth: true
                                borderColor: root.surfacePatchSize === modelData ? Atelier.accent : Atelier.border
                                onClicked: root.surfacePatchSize = modelData
                            }
                        }
                    }
                    Caption { visible: root.surfaceMode; text: qsTr("Alignment areas: 64 px for fine, contrasted detail; 96 px to start; 160 px for softer or noisier surfaces. Use short, well-exposed recordings with steady focus.") }
                    RowLayout {
                        visible: root.surfaceMode; enabled: !root.controlsBusy
                        Caption { text: qsTr("Best frames per area") }
                        CaptionSwitch {
                            checked: root.localSelection
                            Accessible.name: qsTr("Best frames per area")
                            onToggled: root.localSelection = checked
                        }
                    }
                    Caption {
                        visible: root.surfaceMode && root.localSelection
                        text: qsTr("Each area chooses its best frames from the entire video. The blue boundary sets the count per area; the curve remains a global overview. This takes longer than global selection.")
                    }

                    Caption { visible: !root.library.compactProcessing; text: qsTr("Aligns the selected frames and averages them into ONE still image. Unreliable matches are skipped; shared borders are cropped. Play source always shows the original video.") }
                    ProcessingButton {
                        text: root.surfaceMode && root.localSelection ? qsTr("Stack up to %1 frames per area").arg(keep.value) : qsTr("Align & stack ") + keep.value + qsTr(" frames")
                        working: root.controller.stacking
                        Layout.fillWidth: true
                        enabled: root.controller.ranked && !root.controlsBusy && !root.adjustmentsDirty
                        onClicked: root.controller.stack(keep.value, root.surfaceMode, root.surfacePatchSize, root.localSelection)
                    }
                    }
                    SectionTitle { text: root.controller.stillImage ? qsTr("Reveal detail") : qsTr("3   Reveal detail") }
                    Caption { visible: !root.library.compactProcessing; text: qsTr("Adjustments update automatically after a short pause. Levels 1–2: fine details · 3–4: medium structures · 5–6: broad contrast. Each scale doubles in size. Hover the help icon for an explanation.") }
                    ColumnLayout {
                        enabled: root.controller.hasResult && (!root.controlsBusy || root.controller.adjusting)
                        Layout.fillWidth: true
                        Repeater {
                            id: waveletRows
                            model: 6
                            delegate: Adjustment {
                                required property int index
                                settingKey: "level" + (index + 1)
                                label: qsTr("Level ") + (index + 1)
                                help: [
                                    qsTr("Finest detail: very small edges and texture. Also amplifies fine noise; use gently."),
                                    qsTr("Fine detail: narrow edges and small structures, at a broader scale than Level 1."),
                                    qsTr("Medium detail: intermediate structures and transitions."),
                                    qsTr("Broader detail: larger structures and contrast transitions."),
                                    qsTr("Large-scale detail: broad bands and large features."),
                                    qsTr("Coarsest detail: the broadest structures and contrast variations. Large gains can create halos.")
                                ][index] + qsTr(" Each level is about twice as broad as the previous one. These are overlapping detail scales, not fixed astronomical features. Zero adds no detail at this level.")
                                minimum: 0; maximum: index === 0 ? 50 : index < 3 ? 20 : 10
                            }
                        }
                        Adjustment { settingKey: "denoise"; label: qsTr("Denoise"); minimum: 0; maximum: 0.05 }
                        Adjustment { settingKey: "red"; label: qsTr("Red") }
                        Adjustment { settingKey: "green"; label: qsTr("Green") }
                        Adjustment { settingKey: "blue"; label: qsTr("Blue") }
                        Adjustment { settingKey: "gamma"; label: qsTr("Gamma") }
                        RowLayout {
                            Caption { text: root.adjustmentsDirty ? ((adjustmentTimer.running || root.controller.adjusting) ? qsTr("Updating preview…") : qsTr("Preview paused · adjust a slider to resume")) : qsTr("Automatic preview") }
                            Label {text:"⌘ Z";color:Atelier.muted;font.pixelSize:11}
                        }
                        RowLayout {
                            enabled: !root.controlsBusy && !root.adjustmentsDirty
                            IconButton {
                                text: qsTr("Hold for before")
                                Layout.fillWidth: true
                                onPressed: { root.controller.showResult(false); root.holdBefore = true; }
                                onReleased: root.holdBefore = false
                                onCanceled: root.holdBefore = false
                            }
                            IconButton {
                                text: qsTr("Compare")
                                Layout.fillWidth: true
                                borderColor: root.compareImages ? Atelier.accent : Atelier.border
                                onClicked: { root.controller.showResult(false); root.compareImages = !root.compareImages; }
                            }
                        }

                        IconButton {text:qsTr("Delete these adjustments");active:true;Layout.fillWidth:true;onClicked:edits.reset()}

                    }

                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Layout.minimumHeight: 32
            Layout.preferredHeight: 32
            Layout.maximumHeight: 32
            Label {
                text: root.controller.status || qsTr("Open a SER video or a PNG / TIFF image to begin.")
                color: "#b8c7da"
                elide: Text.ElideRight
                Layout.fillWidth: true
                font.pixelSize: 12
                HoverHandler { id: statusHover }
                ToolTip.visible: statusHover.hovered && truncated
                ToolTip.text: text
            }
            // Opacity preserves layout space when no long-running job is active.
            ProgressBar { from: 0; to: 100; value: root.controller.progress; opacity: root.processingJob ? 1 : 0; Layout.preferredWidth: 100 }
            IconButton { text: qsTr("Cancel"); opacity: root.processingJob ? 1 : 0; enabled: root.processingJob; onClicked: { root.playing = false; adjustmentTimer.stop(); root.controller.cancel(); } }
        }
    }
}
