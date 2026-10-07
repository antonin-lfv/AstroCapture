import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Window

Rectangle {
    id: root
    property real sourceWidth: 0
    property real sourceHeight: 0
    property alias imageContainer: imageLayer
    property bool fitMode: true
    property real zoom: 1
    property real panX: 0
    property real panY: 0
    readonly property real pixelRatio: Screen.devicePixelRatio || 1
    readonly property bool hasImage: sourceWidth > 0 && sourceHeight > 0
    readonly property real fitZoom: hasImage ? Math.min(surface.width * pixelRatio / sourceWidth, surface.height * pixelRatio / sourceHeight) : 1
    readonly property real effectiveZoom: fitMode ? fitZoom : zoom
    readonly property real renderedWidth: sourceWidth * effectiveZoom / pixelRatio
    readonly property real renderedHeight: sourceHeight * effectiveZoom / pixelRatio
    color: "#07090c"
    radius: 8
    border.color: "#232a34"

    function constrainPan() {
        const xLimit = Math.max(0, (renderedWidth - surface.width) / 2);
        const yLimit = Math.max(0, (renderedHeight - surface.height) / 2);
        panX = Math.max(-xLimit, Math.min(xLimit, panX));
        panY = Math.max(-yLimit, Math.min(yLimit, panY));
    }
    function fit() { fitMode = true; panX = 0; panY = 0; }
    function zoomAt(value, x, y) {
        if (!hasImage) return;
        const next = Math.max(Math.min(0.05, fitZoom), Math.min(Math.max(16, fitZoom), value));
        const ratio = next / effectiveZoom;
        const nextX = (x - surface.width / 2) * (1 - ratio) + panX * ratio;
        const nextY = (y - surface.height / 2) * (1 - ratio) + panY * ratio;
        zoom = next;
        fitMode = false;
        panX = nextX; panY = nextY;
        constrainPan();
    }
    function actualSize() { zoomAt(1, surface.width / 2, surface.height / 2); }
    onSourceWidthChanged: fit()
    onSourceHeightChanged: fit()
    onWidthChanged: constrainPan()
    onHeightChanged: constrainPan()

    Item {
        id: surface
        objectName: "viewportSurface"
        anchors { left: parent.left; right: parent.right; top: parent.top; bottom: toolbar.top; margins: 8 }
        clip: true
        MouseArea {
            anchors.fill: parent
            enabled: root.hasImage
            acceptedButtons: Qt.LeftButton | Qt.MiddleButton
            cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor
            property real lastX: 0
            property real lastY: 0
            onPressed: (mouse) => { lastX = mouse.x; lastY = mouse.y; }
            onPositionChanged: (mouse) => {
                if (!pressed) return;
                root.panX += mouse.x - lastX; root.panY += mouse.y - lastY;
                lastX = mouse.x; lastY = mouse.y;
                root.constrainPan();
            }
            onDoubleClicked: root.fitMode ? root.actualSize() : root.fit()
        }
        Item {
            id: imageLayer
            width: root.sourceWidth / root.pixelRatio
            height: root.sourceHeight / root.pixelRatio
            x: (surface.width - root.renderedWidth) / 2 + root.panX
            y: (surface.height - root.renderedHeight) / 2 + root.panY
            scale: root.effectiveZoom
            transformOrigin: Item.TopLeft
        }
        WheelHandler {
            target: null
            enabled: root.hasImage
            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
            onWheel: (event) => {
                const delta = event.angleDelta.y || event.pixelDelta.y;
                root.zoomAt(root.effectiveZoom * Math.pow(1.0015, delta), event.x, event.y);
                event.accepted = true;
            }
        }
        PinchHandler {
            target: null
            enabled: root.hasImage
            property real initialZoom: 1
            onActiveChanged: if (active) initialZoom = root.effectiveZoom
            onActiveScaleChanged: root.zoomAt(initialZoom * activeScale, centroid.position.x, centroid.position.y)
        }
    }
    RowLayout {
        id: toolbar
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom; margins: 8 }
        height: 28
        spacing: 4
        enabled: root.hasImage
        IconButton { text: "−"; onClicked: root.zoomAt(root.effectiveZoom / 1.25, surface.width / 2, surface.height / 2); Accessible.name: qsTr("Zoom out") }
        IconButton { text: "+"; onClicked: root.zoomAt(root.effectiveZoom * 1.25, surface.width / 2, surface.height / 2); Accessible.name: qsTr("Zoom in") }
        Label { text: Math.round(root.effectiveZoom * 100) + "%"; color: "#a8b5c6"; font.pixelSize: 11; Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter; elide: Text.ElideRight }
        IconButton { text: "100%"; onClicked: root.actualSize(); ToolTip.visible: hovered; ToolTip.text: qsTr("One image pixel per screen pixel") }
        IconButton { text: qsTr("Fit"); borderColor: root.fitMode ? "#6ca5f8" : "#3a4048"; onClicked: root.fit(); ToolTip.visible: hovered; ToolTip.text: qsTr("Fit to window. Double-click the image to switch between Fit and 100%.") }
    }
}
