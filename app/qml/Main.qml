import QtQuick
// The native macOS style ignores background/contentItem customization, so
// controls we style ourselves (e.g. rescanButton below) silently keep their
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
    ColumnLayout {
        anchors.fill: parent
        visible: previewController.connected
        spacing: 0

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
            Layout.preferredHeight: 64
            Layout.leftMargin: 12
            Layout.rightMargin: 12
            Layout.topMargin: 8
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.margins: 12
            spacing: 10

            RowLayout {
                Layout.fillWidth: true
                spacing: 16

                Text {
                    text: previewController.cameraName
                    color: "#e8eaed"
                    font.pixelSize: 13
                }

                Text {
                    text: "Frames: " + previewController.frameCount
                    color: "#9aa0a6"
                    font.pixelSize: 13
                }

                Text {
                    text: previewController.fps.toFixed(1) + " fps"
                    color: "#9aa0a6"
                    font.pixelSize: 13
                }

                Text {
                    text: "Preview skipped: " + previewController.previewSkippedFrames
                    color: "#9aa0a6"
                    font.pixelSize: 13
                }

                Item { Layout.fillWidth: true }
            }

            // Capability-driven: hidden entirely for backends that don't
            // expose the control (e.g. the UVC/webcam backend today).
            RowLayout {
                Layout.fillWidth: true
                visible: previewController.exposureSupported
                spacing: 12

                Text {
                    text: "Exposure"
                    color: "#9aa0a6"
                    font.pixelSize: 12
                    Layout.preferredWidth: 64
                }

                Slider {
                    id: exposureSlider
                    Layout.fillWidth: true
                    from: previewController.exposureMinMicroseconds
                    to: previewController.exposureMaxMicroseconds
                    value: previewController.exposureMicroseconds
                    onMoved: previewController.exposureMicroseconds = value
                }

                Text {
                    text: (previewController.exposureMicroseconds / 1000).toFixed(1) + " ms"
                    color: "#e8eaed"
                    font.pixelSize: 12
                    Layout.preferredWidth: 64
                }
            }

            RowLayout {
                Layout.fillWidth: true
                visible: previewController.gainSupported
                spacing: 12

                Text {
                    text: "Gain"
                    color: "#9aa0a6"
                    font.pixelSize: 12
                    Layout.preferredWidth: 64
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

                Text {
                    text: previewController.gainValue
                    color: "#e8eaed"
                    font.pixelSize: 12
                    Layout.preferredWidth: 64
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

        Button {
            id: rescanButton
            Layout.alignment: Qt.AlignHCenter
            text: "Rescan for cameras"
            onClicked: previewController.rescan()

            ToolTip.visible: hovered
            ToolTip.delay: 400
            ToolTip.text: "Tries a connected webcam first, falls back to the mock test camera. The reference planetary camera backend ships in Gate C."

            // Explicit styling: the default Qt Quick Controls look follows
            // the OS light/dark appearance independently of this window's
            // always-dark background, which can render near-invisible text.
            contentItem: Text {
                text: rescanButton.text
                color: "#e8eaed"
                font.pixelSize: 14
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            background: Rectangle {
                implicitWidth: 172
                implicitHeight: 36
                radius: 6
                color: rescanButton.pressed ? "#252b32" : (rescanButton.hovered ? "#2a3038" : "#1c2026")
                border.color: "#3a4048"
                border.width: 1
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
}
