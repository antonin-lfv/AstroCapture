import "Atelier.js" as Atelier
import QtQuick
import QtQuick.Controls.Basic
import AstroCapture 1.0

Label {
    id: hint
    property var controller: null
    property string format: "png"
    property real fixedBytes: -1
    property bool ready: true
    property bool estimating: visible
    readonly property real estimatedBytes: fixedBytes>=0 ? fixedBytes : estimator.bytes
    objectName: "exportSizeHint"
    color: Atelier.secondary
    font.pixelSize: 12
    wrapMode: Text.Wrap
    function formattedSize(bytes) {
        if(bytes>=1000000)return qsTr("%1 MB").arg(Number(bytes/1000000).toLocaleString(Qt.locale(),"f",1));
        if(bytes>=1000)return qsTr("%1 kB").arg(Number(bytes/1000).toLocaleString(Qt.locale(),"f",1));
        return qsTr("%1 bytes").arg(Math.round(bytes));
    }
    text: !ready ? qsTr("Updating size estimate…")
          : estimatedBytes>=0 ? qsTr("Estimated file size: %1").arg((fixedBytes>=0 || estimator.exact ? "" : "≈ ")+formattedSize(estimatedBytes))
          : estimator.calculating ? qsTr("Estimating file size…") : qsTr("File size estimate unavailable")
    ExportSizeEstimator {
        id: estimator
        input: hint.controller ? hint.controller.exportSizeInput : null
        format: hint.format
        active: hint.estimating && hint.fixedBytes<0 && hint.ready && hint.controller && !hint.controller.busy
    }
    HoverHandler {id:hover}
    DarkToolTip {
        visible:hover.hovered && hint.estimatedBytes>=0
        text:fixedBytes>=0 || estimator.exact
             ? qsTr("Calculated for the selected export format.")
             : qsTr("Estimated from native-resolution image samples, including the caption. Compression depends on image content; the final file size may vary.")
    }
}
