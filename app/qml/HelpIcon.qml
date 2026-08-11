import QtQuick
import QtQuick.Controls.Basic

// Small clickable "?" affordance next to a label, showing `text` in a
// popup on click. Clickable rather than hover-only: a plain ToolTip
// requires already knowing something is hoverable to discover it exists,
// which defeats the point for text meant to explain something unfamiliar.
Item {
    id: root

    property string text: ""
    property int maxWidth: 260

    implicitWidth: 14
    implicitHeight: 14

    Rectangle {
        anchors.fill: parent
        radius: width / 2
        color: mouseArea.containsMouse ? "#2a3038" : "#1c2026"
        border.color: "#3a4048"
        border.width: 1

        Text {
            anchors.centerIn: parent
            text: "?"
            color: "#9aa0a6"
            font.pixelSize: 9
            font.weight: Font.DemiBold
        }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: {
            // Positioned imperatively, on click, rather than via a
            // declarative "x: root.mapToItem(...)" binding: mapToItem()
            // is a method call, and QML's binding dependency tracker
            // doesn't pick up everything it reads internally (root's and
            // the overlay's transforms), so a declarative binding built
            // on it can silently freeze at a stale (and, empirically,
            // wildly wrong — off by hundreds of pixels) position instead
            // of recomputing when the window's actual geometry settles.
            // Recomputing fresh right before every open() sidesteps that
            // entirely.
            const overlay = root.Overlay.overlay;
            const pos = root.mapToItem(overlay, 0, root.height + 6);
            popup.parent = overlay;
            // Clamped so the popup can't run past the window's right
            // edge — most HelpIcon placements in this app sit close to
            // the sidebar's right edge, where an unclamped position
            // would clip the text.
            popup.x = Math.min(pos.x, overlay.width - popup.width - 8);
            popup.y = pos.y;
            popup.open();
        }
    }

    Popup {
        id: popup
        width: root.maxWidth
        padding: 10
        modal: false
        focus: true
        closePolicy: Popup.CloseOnPressOutside | Popup.CloseOnEscape

        background: Rectangle {
            color: "#15181d"
            radius: 8
            border.color: "#3a4048"
            border.width: 1
        }

        contentItem: Text {
            text: root.text
            color: "#c8ccd1"
            font.pixelSize: 11
            wrapMode: Text.Wrap
        }
    }
}
