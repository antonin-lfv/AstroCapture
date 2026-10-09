import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "Atelier.js" as Atelier

ColumnLayout {
    id: control
    property string label: ""
    property string explanation: ""
    property real from: 0
    property real to: 1
    property real value: 0
    property real stepSize: 0
    property int decimals: 4
    property bool textIsValue: false
    property real inputMinimum: from
    property real inputMaximum: to
    property alias text: field.text
    readonly property bool pressed: slider.pressed
    property bool synchronizing: false
    signal moved()
    signal gestureStarted()
    signal gestureEnded()
    Layout.fillWidth: true
    Layout.minimumWidth: 0
    spacing: 2
    onValueChanged: {
        if (!synchronizing && field) field.text = Number(value).toFixed(decimals);
    }
    RowLayout {
        Layout.fillWidth: true
        spacing: 6
        Label {text:control.label;color:Atelier.secondary;Layout.fillWidth:true;wrapMode:Text.Wrap;font.pixelSize:13}
        HelpIcon {text:control.explanation;visible:text.length>0}
        TextField {
            id: field
            property bool edited: false
            Layout.preferredWidth: 88
            implicitHeight: 28
            text: Number(control.value).toFixed(control.decimals)
            horizontalAlignment: Text.AlignRight
            selectByMouse: true
            Accessible.name: control.label
            Accessible.description: control.explanation
            color: control.textIsValue && !activeFocus ? "transparent" : Atelier.text
            validator: DoubleValidator {bottom:control.inputMinimum;top:control.inputMaximum;locale:"C"}
            background: Rectangle {color:Atelier.field;radius:Atelier.controlRadius;border.color:field.activeFocus?Atelier.accent:Atelier.border}
            Text {
                anchors.fill: parent;anchors.margins: 6
                visible: control.textIsValue && !field.activeFocus
                text: Number(control.value).toFixed(control.decimals)
                color: Atelier.text;font: field.font
                horizontalAlignment: Text.AlignRight;verticalAlignment: Text.AlignVCenter
            }
            HoverHandler {id: valueHover}
            DarkToolTip {visible:valueHover.hovered && control.textIsValue;text:field.text}
            onTextEdited: edited = true
            onTextChanged: {
                if (control.textIsValue && !control.synchronizing && text.length && acceptableInput) {
                    control.synchronizing = true;
                    control.value = Number(text);
                    control.synchronizing = false;
                }
            }
            onEditingFinished: {
                if (!edited) return;
                edited = false;
                if (!acceptableInput) {text=Number(control.value).toFixed(control.decimals);return;}
                control.gestureStarted();
                control.synchronizing=true;
                control.value=Number(text);
                control.synchronizing=false;
                control.moved();control.gestureEnded();
            }
        }
    }
    AtelierSlider {
        id: slider
        Layout.fillWidth: true
        implicitHeight: 18
        from: control.from;to:control.to;value:control.value;stepSize:control.stepSize
        Accessible.name: control.label
        onMoved: {
            control.synchronizing=true;
            control.value=value;
            field.text=control.textIsValue?Number(value).toPrecision(9):Number(value).toFixed(control.decimals);
            control.synchronizing=false;
            control.moved();
        }
        onPressedChanged: if(pressed)control.gestureStarted();else control.gestureEnded()
    }
}
