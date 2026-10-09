import "Atelier.js" as Atelier
import QtQuick
import QtQuick.Layouts

// A labeled group of controls in the sidebar: small uppercase title, then
// its content stacked below. Keeps section content declarable as plain
// children (SidebarSection { Text { ... } ... }) via the default alias.
ColumnLayout {
    id: root

    property string title: ""
    // Non-empty shows a clickable "?" next to the title (see HelpIcon.qml).
    property string helpText: ""
    default property alias content: inner.children

    Layout.fillWidth: true
    spacing: 8
    Layout.topMargin: 10

    RowLayout {
        Layout.fillWidth: true
        spacing: 4

        Text {
            text: root.title
            color: Atelier.muted
            font.pixelSize: 13
            font.weight: Font.DemiBold
            font.capitalization: Font.MixedCase
            font.letterSpacing: 0
            Layout.fillWidth: true
        }

        HelpIcon {
            visible: root.helpText.length > 0
            text: root.helpText
        }
    }

    ColumnLayout {
        id: inner
        Layout.fillWidth: true
        spacing: 8
    }
}
