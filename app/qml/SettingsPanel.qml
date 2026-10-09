import "Atelier.js" as Atelier
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
        border.color: Atelier.border
        border.width: 1
    }

    Popup {
        id: overwriteConfirm
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        focus: true
        padding: 16
        // See Main.qml's mockConfirm Popup comment: a fixed width: on this
        // plain child ColumnLayout doesn't feed the Popup's own
        // auto-sizing (which reads contentItem.implicitWidth), so the
        // Popup stayed smaller than its actual content. Explicit width
        // here + ColumnLayout using parent.width makes it deterministic.
        width: 312

        property string pendingName: ""

        background: Rectangle {
            color: "#15181d"
            radius: 10
            border.color: Atelier.border
            border.width: 1
        }

        ColumnLayout {
            width: parent.width
            spacing: 14

            Text {
                text: qsTr("Replace preset “%1”?").arg(overwriteConfirm.pendingName)
                color: Atelier.text
                font.pixelSize: 13
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }

            Text {
                text: qsTr("A preset with this name already exists. Saving will overwrite it.")
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
                    text: qsTr("Cancel")
                    onClicked: overwriteConfirm.close()
                }

                IconButton {
                    text: qsTr("Replace")
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
            color: Atelier.field
            radius: 8
            border.color: Atelier.border
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
                color: Atelier.text
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
                text: qsTr("Capture settings")
                color: Atelier.text
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

            RowLayout {
                Layout.fillWidth: true
                spacing: 4

                Text {
                    text: qsTr("Image format")
                    color: "#9aa0a6"
                    font.pixelSize: 11
                }

                HelpIcon {
                    text: qsTr("PNG saves the displayed 8-bit preview for sharing. TIFF16 converts the sensor data to 16-bit RGB for image processing. FITS keeps sensor samples and acquisition metadata for scientific calibration. A larger format cannot add precision absent from the source.")
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
                    text: qsTr("Video format")
                    color: "#9aa0a6"
                    font.pixelSize: 11
                }

                HelpIcon {
                    text: qsTr("SER is an uncompressed planetary video suitable for stacking. FITS saves individual sensor frames and metadata for deep-sky calibration. ACRAW is the native raw-sequence fallback. None of these formats improves detail missing from the capture.")
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                enabled: controller && !controller.recording

                Repeater {
                    model: ["SER", "ACRAW", "FITS"]

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

                        helpText: modelData === "SER"
                                      ? qsTr("Standard planetary video format (AutoStakkert, PIPP, Registax, FireCapture)")
                                      : modelData === "FITS" ? qsTr("Individual linear FITS frames for deep-sky imaging") : qsTr("AstroCapture's own interim format")
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
                text: qsTr("Presets — ") + (controller ? controller.cameraName : "")
                color: "#9aa0a6"
                font.pixelSize: 11
                elide: Text.ElideRight
                Layout.fillWidth: true
            }

            SettingLabel {text:qsTr("Preset name");explanation:qsTr("Saves the current supported camera controls for this camera. Loading changes capture settings, not finishing adjustments. Use a new name to keep another configuration; replacing overwrites the existing preset.")}
            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                TextField {
                    id: presetNameField
                    Layout.fillWidth: true
                    placeholderText: qsTr("Preset name")
                    color: Atelier.text
                    placeholderTextColor: "#9aa0a6"
                    selectedTextColor: Atelier.text
                    selectionColor: "#2a3d6b"
                    font.pixelSize: 12
                    background: Rectangle {
                        implicitHeight: 28
                        radius: 6
                        color: Atelier.field
                        border.color: Atelier.border
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
                        presetStatus.text = qsTr("“%1” saved.").arg(name);
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
                        color: Atelier.text
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }

                    IconButton {
                        iconName: "check"
                        onClicked: {
                            controller.loadPreset(modelData);
                            toast.show(qsTr("“%1” applied.").arg(modelData));
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
                text: qsTr("No presets yet.")
                color: Atelier.muted
                font.pixelSize: 11
            }
        }
    }
}
