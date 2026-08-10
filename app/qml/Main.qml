import QtQuick
import QtQuick.Controls
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

        RowLayout {
            Layout.fillWidth: true
            Layout.margins: 12
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
