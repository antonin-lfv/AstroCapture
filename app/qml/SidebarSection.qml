import QtQuick
import QtQuick.Layouts

// A labeled group of controls in the sidebar: small uppercase title, then
// its content stacked below. Keeps section content declarable as plain
// children (SidebarSection { Text { ... } ... }) via the default alias.
ColumnLayout {
    id: root

    property string title: ""
    default property alias content: inner.children

    Layout.fillWidth: true
    spacing: 8

    Text {
        text: root.title
        color: "#6e7681"
        font.pixelSize: 10
        font.weight: Font.DemiBold
        font.capitalization: Font.AllUppercase
        font.letterSpacing: 0.8
        Layout.fillWidth: true
    }

    ColumnLayout {
        id: inner
        Layout.fillWidth: true
        spacing: 10
    }
}
