import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Dialogs

Popup {
    id: root

    property var controller

    parent: Overlay.overlay
    anchors.centerIn: parent
    modal: true
    focus: true
    width: 360
    padding: 16

    background: Rectangle {
        color: "#15181d"
        radius: 10
        border.color: "#3a4048"
        border.width: 1
    }

    FolderDialog {
        id: folderDialog
        title: "Choose a save location"
        onAccepted: controller.saveDirectory = String(selectedFolder).replace("file://", "")
    }

    ColumnLayout {
        width: parent.width
        spacing: 14

        RowLayout {
            Layout.fillWidth: true

            Text {
                text: "Settings"
                color: "#e8eaed"
                font.pixelSize: 14
                font.weight: Font.DemiBold
                Layout.fillWidth: true
            }

            IconButton {
                iconName: "close"
                borderColor: "transparent"
                onClicked: root.close()
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6

            Text {
                text: "Save location"
                color: "#9aa0a6"
                font.pixelSize: 11
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Text {
                    Layout.fillWidth: true
                    text: controller && controller.saveDirectory.length > 0
                          ? controller.saveDirectory
                          : "Default (Pictures/Movies → AstroCapture)"
                    color: "#e8eaed"
                    font.pixelSize: 12
                    elide: Text.ElideMiddle
                }

                IconButton {
                    iconName: "folder"
                    text: "Open"
                    onClicked: controller.openSaveDirectory()

                    ToolTip.visible: hovered
                    ToolTip.delay: 400
                    ToolTip.text: "Open this folder in the file manager"
                }

                IconButton {
                    iconName: "folder"
                    onClicked: folderDialog.open()

                    ToolTip.visible: hovered
                    ToolTip.delay: 400
                    ToolTip.text: "Choose a different folder"
                }
            }

            Text {
                visible: controller && controller.saveDirectory.length > 0
                text: "Reset to default"
                color: "#6e93ff"
                font.pixelSize: 11

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: controller.saveDirectory = ""
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: "#242830"
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6

            RowLayout {
                Layout.fillWidth: true
                spacing: 4

                Text {
                    text: "Image format"
                    color: "#9aa0a6"
                    font.pixelSize: 11
                }

                HelpIcon {
                    text: "PNG: lossless, opens everywhere — good default.\n\n" +
                          "TIFF16: 16-bit per channel, for processing pipelines that expect more precision than 8-bit " +
                          "(the source is still 8-bit today; this stores it losslessly at 16-bit).\n\n" +
                          "FITS: the astronomy-standard format, for tools that expect it (PixInsight, DSS, etc)."
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Repeater {
                    model: ["PNG", "TIFF16", "FITS"]

                    delegate: IconButton {
                        Layout.fillWidth: true
                        text: modelData
                        active: controller && controller.saveFormat === modelData
                        activeBorderColor: "#6e93ff"
                        onClicked: controller.saveFormat = modelData
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: "#242830"
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6

            RowLayout {
                Layout.fillWidth: true
                spacing: 4

                Text {
                    text: "Video format"
                    color: "#9aa0a6"
                    font.pixelSize: 11
                }

                HelpIcon {
                    text: "SER: standard, uncompressed planetary video (AutoStakkert, PIPP, Registax, FireCapture) — use this for real capture sessions you'll stack.\n\n" +
                          "ACRAW: AstroCapture's own interim format from before SER support existed. Kept only as a fallback."
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                enabled: controller && !controller.recording

                Repeater {
                    model: ["SER", "ACRAW"]

                    delegate: IconButton {
                        Layout.fillWidth: true
                        text: modelData
                        active: controller && controller.videoFormat === modelData
                        activeBorderColor: "#6e93ff"
                        onClicked: controller.videoFormat = modelData

                        ToolTip.visible: hovered
                        ToolTip.delay: 400
                        ToolTip.text: modelData === "SER"
                                      ? "Standard planetary video format (AutoStakkert, PIPP, Registax, FireCapture)"
                                      : "AstroCapture's own interim format"
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: "#242830"
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8
            visible: controller && controller.connected

            Text {
                text: "Presets — " + (controller ? controller.cameraName : "")
                color: "#9aa0a6"
                font.pixelSize: 11
                elide: Text.ElideRight
                Layout.fillWidth: true
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                TextField {
                    id: presetNameField
                    Layout.fillWidth: true
                    placeholderText: "Preset name"
                    color: "#e8eaed"
                    selectedTextColor: "#e8eaed"
                    selectionColor: "#2a3d6b"
                    font.pixelSize: 12
                    background: Rectangle {
                        implicitHeight: 28
                        radius: 6
                        color: "#1c2026"
                        border.color: "#3a4048"
                        border.width: 1
                    }
                }

                IconButton {
                    iconName: "plus"
                    enabled: presetNameField.text.trim().length > 0
                    onClicked: {
                        controller.savePreset(presetNameField.text.trim());
                        presetNameField.text = "";
                    }
                }
            }

            ListView {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(contentHeight, 160)
                clip: true
                spacing: 4
                model: controller ? controller.presetNames : []

                delegate: RowLayout {
                    width: ListView.view.width
                    spacing: 8

                    Text {
                        Layout.fillWidth: true
                        text: modelData
                        color: "#e8eaed"
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }

                    IconButton {
                        iconName: "check"
                        onClicked: controller.loadPreset(modelData)
                    }

                    IconButton {
                        iconName: "trash"
                        onClicked: controller.deletePreset(modelData)
                    }
                }
            }

            Text {
                visible: controller && controller.presetNames.length === 0
                text: "No presets yet."
                color: "#5f6368"
                font.pixelSize: 11
            }
        }
    }
}
