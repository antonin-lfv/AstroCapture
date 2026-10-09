import "Atelier.js" as Atelier
import QtQuick
import QtQuick.Controls.Basic

ToolTip {
    id: tip
    objectName: "darkHelpTip"
    width: Math.min(380, Overlay.overlay ? Overlay.overlay.width - 32 : 380)
    readonly property point anchorPosition: {
        // Track ancestors too: an inspector can scroll or a dialog can move.
        let item = parent;
        const geometry = [];
        while (item) {
            geometry.push(item.x, item.y, item.width, item.height, item.scale, item.rotation);
            item = item.parent;
        }
        return parent && Overlay.overlay ? parent.mapToItem(Overlay.overlay, 0, 0) : Qt.point(0, 0);
    }
    x: Overlay.overlay ? Math.max(12-anchorPosition.x, Math.min(0, Overlay.overlay.width-12-anchorPosition.x-width)) : 0
    y: parent && Overlay.overlay && anchorPosition.y+parent.height+6+height>Overlay.overlay.height-12 ? -height-6 : parent ? parent.height+6 : 0
    delay: 350
    timeout: -1
    padding: 12
    contentItem: Text {
        text: tip.text
        color: Atelier.text
        font.pixelSize: 13
        wrapMode: Text.WordWrap
    }
    background: Rectangle {
        color: Atelier.surface
        radius: 8
        border.color: Atelier.border
    }
}
