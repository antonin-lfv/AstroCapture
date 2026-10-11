import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "Atelier.js" as Atelier

ColumnLayout {
    id: panel
    required property var controller
    property var settings: ({palette:"RGB", luminanceAmount:1})
    signal settingsEdited(var settings)
    signal composeRequested(var settings)
    readonly property var plan: {controller.groups; return controller.compositionPlan(settings);}
    readonly property var selected: plan.settings || ({})
    readonly property var applied: controller.compositionState
    readonly property bool pending: {
        if (!applied.imageKey) return true;
        for (const key of ["palette", "red", "green", "blue", "luminance"])
            if (applied[key] !== selected[key]) return true;
        return selected.luminance && applied.luminanceAmount !== selected.luminanceAmount;
    }
    readonly property var paletteKeys: ["RGB", "LRGB", "HOO", "SHO", "Custom"]
    Layout.fillWidth: true
    Layout.minimumWidth: 0
    spacing: 8
    onSettingsChanged: if(luminanceControl) luminanceControl.value=100*(settings.luminanceAmount === undefined ? 1 : settings.luminanceAmount)
    function choosePalette(index) {
        settingsEdited(Object.assign({}, settings, {palette:paletteKeys[index]}));
    }
    function chooseChannel(key, value) {
        const next=Object.assign({}, selected, {palette:"Custom"});
        next[key]=value;
        settingsEdited(next);
    }
    RowLayout {
        Layout.fillWidth: true
        Label {text:qsTr("Channel composition");font.weight:Font.Medium;color:Atelier.text;Layout.fillWidth:true;wrapMode:Text.Wrap}
        HelpIcon {text:qsTr("Combine the integrated filters before finishing. Palettes assign the channels automatically; Custom lets you choose each layer. The source integrations are preserved. Changing these choices requires Compose image to update the result.")}
    }
    DarkComboBox {
        id: paletteChoice
        objectName: "compositionPalette"
        Layout.fillWidth: true; Layout.minimumWidth: 0
        model: [qsTr("RGB · color"), qsTr("LRGB · color and luminance"), qsTr("HOO · hydrogen and oxygen"), qsTr("SHO · Hubble palette"), qsTr("Custom")]
        currentIndex: Math.max(0, panel.paletteKeys.indexOf(panel.settings.palette || "RGB"))
        enabled: !panel.controller.busy
        Accessible.name: qsTr("Composition palette")
        explanation: qsTr("RGB assigns R/G/B. LRGB also uses L. HOO assigns Hα/OIII/OIII; SHO assigns SII/Hα/OIII. HOO and SHO are representative palettes, not natural RGB colors. Every required filter must first be integrated.")
        onActivated: panel.choosePalette(currentIndex)
    }
    GridLayout {
        columns: panel.settings.palette === "Custom" ? 1 : 2
        Layout.fillWidth: true; Layout.minimumWidth: 0
        columnSpacing: 16; rowSpacing: 4
        Repeater {
            model: ["red", "green", "blue", "luminance"]
            delegate: RowLayout {
                required property string modelData
                Layout.fillWidth: true; Layout.minimumWidth: 0
                readonly property string channelLabel: ({red:qsTr("Red"), green:qsTr("Green"), blue:qsTr("Blue"), luminance:qsTr("Luminance")})[modelData]
                readonly property var choices: modelData === "luminance" ? [qsTr("None")].concat(panel.controller.groups) : panel.controller.groups
                Label {text:channelLabel;color:Atelier.secondary;Layout.preferredWidth:80;wrapMode:Text.Wrap;font.pixelSize:12}
                Label {
                    visible: panel.settings.palette !== "Custom"
                    text: panel.selected[modelData] || qsTr("None")
                    color: panel.selected[modelData] && panel.controller.groups.indexOf(panel.selected[modelData]) < 0 ? Atelier.danger : Atelier.text
                    Layout.fillWidth: true; Layout.minimumWidth: 0
                    horizontalAlignment: Text.AlignRight; elide: Text.ElideRight; font.pixelSize:12
                    Accessible.name: parent.channelLabel + ": " + text
                }
                DarkComboBox {
                    objectName: "compositionChannel_" + modelData
                    Layout.fillWidth: true; Layout.minimumWidth: 0
                    model: parent.choices
                    currentIndex: modelData === "luminance" && !panel.selected.luminance ? 0 : parent.choices.indexOf(panel.selected[modelData])
                    displayText: currentIndex >= 0 ? currentText : panel.selected[modelData] ? qsTr("Missing: %1").arg(panel.selected[modelData]) : qsTr("Select a filter")
                    enabled: !panel.controller.busy
                    visible: panel.settings.palette === "Custom"
                    Accessible.name: parent.channelLabel
                    explanation: qsTr("Use an integrated layer for this output channel. Choosing a different layer switches to Custom. Luminance is optional in Custom; None preserves the RGB composition. The layers are aligned on stars when composed.")
                    onActivated: panel.chooseChannel(modelData, modelData === "luminance" && currentIndex === 0 ? "" : currentText)
                }
            }
        }
    }
    ParameterControl {
        id: luminanceControl
        objectName: "compositionLuminanceAmount"
        label: qsTr("Luminance contribution (%)")
        from: 0; to: 100; stepSize: 1; decimals: 0
        value: 100 * (panel.settings.luminanceAmount === undefined ? 1 : panel.settings.luminanceAmount)
        visible: !!panel.selected.luminance
        enabled: !panel.controller.busy
        explanation: qsTr("0 keeps the RGB brightness and ignores L. 100 uses the aligned L brightness; intermediate values blend both. RGB channel ratios are retained where their mean is positive. This does not equalize different exposure units, recover saturated detail or sharpen the image. Recompose after changing the value.")
        onMoved: panel.settingsEdited(Object.assign({}, panel.settings, {luminanceAmount:value / 100}))
    }
    Label {
        objectName: "compositionReadiness"
        Layout.fillWidth: true; Layout.minimumWidth: 0
        text: panel.plan.message || ""
        color: panel.plan.ready ? Atelier.secondary : Atelier.danger
        wrapMode: Text.Wrap; font.pixelSize: 11
    }
    Label {
        objectName: "compositionPending"
        Layout.fillWidth: true; Layout.minimumWidth: 0
        visible: !!panel.applied.imageKey && panel.controller.displayingResult && !panel.controller.comparingRaw && !panel.controller.showingMask && !panel.controller.showingBackgroundModel && panel.controller.starLayerView === 0
        text: panel.pending ? qsTr("Previous composition displayed. Recompose to apply these choices.") : qsTr("This composition is displayed.")
        color: Atelier.muted; wrapMode: Text.Wrap; font.pixelSize: 11
    }
    IconButton {
        objectName: "compositionApply"
        text: qsTr("Compose image")
        Layout.fillWidth: true; Layout.minimumWidth: 0
        enabled: panel.plan.ready && !panel.controller.busy
        onClicked: panel.composeRequested(panel.selected)
    }
    DarkComboBox {
        objectName: "compositionInspectLayer"
        Layout.fillWidth: true; Layout.minimumWidth: 0
        model: panel.controller.groups
        currentIndex: -1
        displayText: qsTr("View an integrated filter")
        Accessible.name: displayText
        enabled: !panel.controller.busy
        explanation: qsTr("Display an individual filter integration for inspection or finishing. Its original data is preserved. Compose image returns to a calculated color composition using the selected palette.")
        onActivated: {panel.controller.selectResult(currentText); currentIndex=-1;}
    }
}
