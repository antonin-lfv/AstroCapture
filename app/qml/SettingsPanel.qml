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
    width: 420
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

    Popup {
        id: overwriteConfirm
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        focus: true
        padding: 16

        property string pendingName: ""

        background: Rectangle {
            color: "#15181d"
            radius: 10
            border.color: "#3a4048"
            border.width: 1
        }

        ColumnLayout {
            width: 280
            spacing: 14

            Text {
                text: "Replace preset \"" + overwriteConfirm.pendingName + "\"?"
                color: "#e8eaed"
                font.pixelSize: 13
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }

            Text {
                text: "A preset with this name already exists. Saving will overwrite it."
                color: "#9aa0a6"
                font.pixelSize: 11
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Item { Layout.fillWidth: true }

                IconButton {
                    text: "Cancel"
                    onClicked: overwriteConfirm.close()
                }

                IconButton {
                    text: "Replace"
                    borderColor: "#e5484d"
                    onClicked: {
                        controller.savePreset(overwriteConfirm.pendingName);
                        presetStatus.text = "\"" + overwriteConfirm.pendingName + "\" overwritten.";
                        overwriteConfirm.pendingName = "";
                        presetNameField.text = "";
                        overwriteConfirm.close();
                    }
                }
            }
        }
    }

    Popup {
        id: toast
        parent: Overlay.overlay
        x: Overlay.overlay ? (Overlay.overlay.width - width) / 2 : 0
        y: Overlay.overlay ? Overlay.overlay.height - height - 32 : 0
        modal: false
        focus: false
        closePolicy: Popup.NoAutoClose
        padding: 10

        property string message: ""

        function show(msg) {
            message = msg;
            toastTimer.restart();
            open();
        }

        background: Rectangle {
            color: "#1c2026"
            radius: 8
            border.color: "#3a4048"
            border.width: 1
        }

        contentItem: RowLayout {
            spacing: 8

            Icon {
                name: "check"
                color: "#3ddc84"
                Layout.preferredWidth: 14
                Layout.preferredHeight: 14
            }

            Text {
                text: toast.message
                color: "#e8eaed"
                font.pixelSize: 12
            }
        }

        enter: Transition {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 120 }
        }
        exit: Transition {
            NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 150 }
        }

        Timer {
            id: toastTimer
            interval: 1800
            onTriggered: toast.close()
        }
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
                    id: saveLocationText
                    Layout.fillWidth: true
                    text: controller && controller.saveDirectory.length > 0
                          ? controller.saveDirectory
                          : "Default (Pictures/Movies → AstroCapture)"
                    color: "#e8eaed"
                    font.pixelSize: 12
                    elide: Text.ElideMiddle

                    HoverHandler {
                        id: saveLocationHover
                    }

                    ToolTip.visible: saveLocationHover.hovered && saveLocationText.truncated
                    ToolTip.delay: 400
                    ToolTip.text: saveLocationText.text
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
                    text: "Choose"
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
                        // preferredWidth: 0, not just fillWidth: without
                        // it, RowLayout gives each item its own natural
                        // (text-driven) width as a minimum before
                        // distributing extra space, so "TIFF16" ends up
                        // wider than "PNG" instead of all three being an
                        // even third.
                        Layout.preferredWidth: 0
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
                        // See the Image format Repeater's identical
                        // comment: without this, "ACRAW" ends up wider
                        // than "SER" instead of an even 50/50 split.
                        Layout.preferredWidth: 0
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
                    placeholderTextColor: "#9aa0a6"
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
                        const name = presetNameField.text.trim();
                        if (controller.presetNames.indexOf(name) !== -1) {
                            overwriteConfirm.pendingName = name;
                            overwriteConfirm.open();
                            return;
                        }
                        controller.savePreset(name);
                        presetStatus.text = "\"" + name + "\" saved.";
                        presetNameField.text = "";
                    }
                }
            }

            Text {
                id: presetStatus
                Layout.fillWidth: true
                text: ""
                visible: text.length > 0
                color: "#9aa0a6"
                font.pixelSize: 11
                wrapMode: Text.Wrap
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
                        onClicked: {
                            controller.loadPreset(modelData);
                            toast.show("\"" + modelData + "\" applied.");
                        }
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
