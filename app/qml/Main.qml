import QtQuick
// The native macOS style ignores background/contentItem customization, so
// controls we style ourselves (e.g. IconButton) silently keep their
// OS-appearance-dependent look, which can render near-invisible against our
// always-dark background. Basic is a non-native style that fully honors it.
import QtQuick.Controls.Basic
import QtQuick.Layouts

ApplicationWindow {
    id: window

    width: 1100
    height: 720
    minimumWidth: 720
    minimumHeight: 480
    visible: true
    title: "AstroCapture"
    color: "#0b0d10"

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
        visible: previewController.connected
        spacing: 0

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 8

            LivePreviewItem {
                id: livePreview
                controller: previewController
                Layout.fillWidth: true
                Layout.fillHeight: true
            }

            HistogramItem {
                id: histogramItem
                controller: previewController
                Layout.fillWidth: true
                Layout.preferredHeight: 90
                Layout.leftMargin: 12
                Layout.rightMargin: 12
                Layout.bottomMargin: 12
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
                id: sidebarScroll
                anchors.fill: parent
                anchors.margins: 16
                clip: true

                ColumnLayout {
                    width: sidebarScroll.availableWidth
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
                            iconName: "settings"
                            borderColor: "#242830"
                            onClicked: settingsPanel.open()

                            ToolTip.visible: hovered
                            ToolTip.delay: 400
                            ToolTip.text: "Save location and presets"
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10

                        Text {
                            text: previewController.frameCount + " frames"
                            color: "#9aa0a6"
                            font.pixelSize: 11
                        }

                        Text {
                            text: previewController.fps.toFixed(1) + " fps"
                            color: "#9aa0a6"
                            font.pixelSize: 11
                        }

                        Item { Layout.fillWidth: true }

                        Text {
                            text: "skipped " + previewController.previewSkippedFrames
                            color: "#5f6368"
                            font.pixelSize: 11
                        }
                    }

                    IconButton {
                        Layout.fillWidth: true
                        iconName: "close"
                        text: "Disconnect"
                        borderColor: "#242830"
                        onClicked: previewController.disconnectCamera()

                        ToolTip.visible: hovered
                        ToolTip.delay: 400
                        ToolTip.text: "Disconnect this camera so Rescan can pick a different one."
                    }

                    Rectangle { Layout.fillWidth: true; height: 1; color: "#242830" }

                    SidebarSection {
                        title: "Capture"

                        TextField {
                            id: targetNameField
                            Layout.fillWidth: true
                            placeholderText: "Target (optional)"
                            text: previewController.targetName
                            color: "#e8eaed"
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

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            IconButton {
                                Layout.fillWidth: true
                                iconName: "camera"
                                text: "Capture"
                                onClicked: previewController.captureStillImage()
                            }

                            IconButton {
                                Layout.fillWidth: true
                                iconName: previewController.recording ? "stop" : "camera"
                                text: previewController.recording ? "Stop" : "Record"
                                active: previewController.recording
                                activeBorderColor: "#e5484d"
                                onClicked: previewController.recording ? previewController.stopRecording()
                                                                         : previewController.startRecording()
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
                        title: "Recording"

                        ColumnLayout {
                            Layout.fillWidth: true
                            visible: !previewController.recording
                            spacing: 6

                            Text {
                                text: "Stop recording after"
                                color: "#9aa0a6"
                                font.pixelSize: 12
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 6
                                DarkSpinBox {
                                    id: frameTargetSpin
                                    Layout.fillWidth: true
                                    from: 0
                                    to: 1000000
                                    value: previewController.recordingTargetFrameCount
                                    onValueModified: previewController.recordingTargetFrameCount = value
                                }
                                Text { text: "frames"; color: "#9aa0a6"; font.pixelSize: 11 }
                            }

                            Text { text: "or"; color: "#5f6368"; font.pixelSize: 11 }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 6
                                DarkSpinBox {
                                    id: durationTargetSpin
                                    Layout.fillWidth: true
                                    from: 0
                                    to: 36000
                                    value: previewController.recordingTargetDurationSeconds
                                    onValueModified: previewController.recordingTargetDurationSeconds = value
                                }
                                Text { text: "sec"; color: "#9aa0a6"; font.pixelSize: 11 }
                            }

                            Text {
                                text: "0 = unlimited"
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
                                    text: previewController.recordingElapsedSeconds.toFixed(1) + " s"
                                    color: "#e8eaed"
                                    font.pixelSize: 13
                                    font.weight: Font.DemiBold
                                }
                            }

                            Text {
                                text: "Recorded: " + previewController.recordingFrameCount
                                color: "#9aa0a6"
                                font.pixelSize: 12
                            }

                            // Never silent (ROADMAP.md Gate D: "no silent
                            // dropped frames") — shown even at zero so a
                            // sudden increase is noticeable, red once it
                            // happens.
                            Text {
                                text: "Dropped: " + previewController.recordingDroppedFrames
                                color: previewController.recordingDroppedFrames > 0 ? "#e5484d" : "#9aa0a6"
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
                        visible: previewController.exposureSupported || previewController.gainSupported
                    }

                    // Capability-driven: the whole section (and each
                    // control within it) is hidden for backends that don't
                    // expose it — e.g. the UVC/webcam backend today.
                    SidebarSection {
                        title: "Camera Controls"
                        visible: previewController.exposureSupported || previewController.gainSupported

                        ColumnLayout {
                            Layout.fillWidth: true
                            visible: previewController.exposureSupported
                            spacing: 4

                            RowLayout {
                                Layout.fillWidth: true
                                Text { text: "Exposure"; color: "#9aa0a6"; font.pixelSize: 12; Layout.fillWidth: true }
                                Text {
                                    text: (previewController.exposureMicroseconds / 1000).toFixed(1) + " ms"
                                    color: "#e8eaed"
                                    font.pixelSize: 12
                                }
                            }

                            Slider {
                                id: exposureSlider
                                Layout.fillWidth: true
                                from: previewController.exposureMinMicroseconds
                                to: previewController.exposureMaxMicroseconds
                                value: previewController.exposureMicroseconds
                                onMoved: previewController.exposureMicroseconds = value
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            visible: previewController.gainSupported
                            spacing: 4

                            RowLayout {
                                Layout.fillWidth: true
                                Text { text: "Gain"; color: "#9aa0a6"; font.pixelSize: 12; Layout.fillWidth: true }
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
                    }

                    Item { Layout.fillHeight: true }
                }
            }
        }
    }

    ColumnLayout {
        anchors.centerIn: parent
        visible: !previewController.connected
        spacing: 20
        width: Math.min(420, window.width - 80)

        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            width: 96
            height: 96
            radius: 48
            color: "transparent"
            border.color: "#3a4048"
            border.width: 2

            Rectangle {
                anchors.centerIn: parent
                width: 40
                height: 40
                radius: 20
                color: "#3a4048"
            }
        }

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: "No camera connected"
            color: "#e8eaed"
            font.pixelSize: 22
            font.weight: Font.DemiBold
        }

        Text {
            Layout.alignment: Qt.AlignHCenter
            Layout.fillWidth: true
            text: previewController.errorMessage.length > 0
                  ? previewController.errorMessage
                  : "Connect a supported planetary camera to start a live preview."
            color: previewController.errorMessage.length > 0 ? "#e8776a" : "#9aa0a6"
            font.pixelSize: 14
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
        }

        IconButton {
            id: rescanButton
            Layout.alignment: Qt.AlignHCenter
            iconName: "refresh"
            text: "Rescan for cameras"
            onClicked: previewController.rescan()

            ToolTip.visible: hovered
            ToolTip.delay: 400
            ToolTip.text: "Tries a connected webcam first, falls back to the mock test camera. The reference planetary camera backend ships in Gate C."
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
                text: "Multiple cameras found — choose one:"
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
            text: "Development build"
            color: "#5f6368"
            font.pixelSize: 11
        }
    }

    IconButton {
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: 12
        visible: !previewController.connected
        iconName: "settings"
        borderColor: "#242830"
        onClicked: settingsPanel.open()

        ToolTip.visible: hovered
        ToolTip.delay: 400
        ToolTip.text: "Save location and presets"
    }

    SettingsPanel {
        id: settingsPanel
        controller: previewController
    }

}
