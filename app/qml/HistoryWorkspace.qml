import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Dialogs

Rectangle {
    id: root
    required property AppLibrary library
    property string selectedPath: ""
    readonly property var selected: library.images.find(function(entry) { return entry.path === root.selectedPath; }) || library.images[0] || null
    function selectFirst() { selectedPath = library.images.length ? library.images[0].path : ""; }
    function removeSelected() {
        if (!selected) return;
        const oldIndex = list.currentIndex;
        library.removeImage(selected.url);
        const remaining = library.images;
        selectedPath = remaining.length ? remaining[Math.min(Math.max(oldIndex, 0), remaining.length - 1)].path : "";
    }
    Connections {
        target: root.library
        function onChanged() {
            if (!root.library.images.some(function(entry) { return entry.path === root.selectedPath; })) root.selectFirst();
        }
    }
    Connections {
        target: Qt.application
        function onStateChanged() {
            if (root.visible && Qt.application.state === Qt.ApplicationActive) root.library.refresh();
        }
    }
    Timer {
        interval: 10000
        repeat: true
        running: root.visible && Qt.application.state === Qt.ApplicationActive
        onTriggered: root.library.refresh()
    }
    onSelectedPathChanged: historyView.fit()
    color: "#0b0d10"
    onVisibleChanged: if (visible) library.refresh()
    FileDialog {
        id: importDialog
        title: qsTr("Add existing images to History")
        fileMode: FileDialog.OpenFiles
        nameFilters: [qsTr("Images (*.tif *.tiff *.png *.jpg *.jpeg *.TIF *.TIFF *.PNG *.JPG *.JPEG)")]
        onAccepted: { root.library.importImages(selectedFiles); root.selectFirst(); }
    }
    DropArea {
        id: imageDrop
        anchors.fill: parent
        z: 50
        enabled: root.visible
        function accepts(event) {
            return event.hasUrls && event.urls.length > 0 && event.urls.every(function(url) {
                return /^file:.*\.(tiff?|png|jpe?g)$/i.test(url.toString());
            });
        }
        onEntered: (drag) => { drag.accepted = accepts(drag); }
        onDropped: (drop) => {
            if (!accepts(drop)) { drop.accepted = false; return; }
            root.library.importImages(drop.urls);
            root.selectFirst();
            drop.acceptProposedAction();
        }
        Rectangle {
            anchors.fill: parent
            anchors.margins: 8
            visible: imageDrop.containsDrag
            color: "#e6111c2c"
            border.color: "#6ca5f8"
            border.width: 2
            radius: 8
            Label { anchors.centerIn: parent; text: qsTr("Drop to add images to History"); color: "#eef3fa"; font.pixelSize: 24 }
        }
    }
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 12
        RowLayout {
            Layout.fillWidth: true
            Label { text: qsTr("History"); color: "#eef3fa"; font.pixelSize: 24; Layout.fillWidth: true }
            IconButton { text: qsTr("Add images…"); onClicked: importDialog.open() }
            IconButton { text: qsTr("Refresh"); onClicked: root.library.refresh() }
            IconButton {
                objectName: "removeHistoryEntry"
                text: qsTr("Remove")
                enabled: root.selected !== null
                onClicked: root.removeSelected()
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Remove from History only. The image file stays on disk.")
                Accessible.description: ToolTip.text
            }
        }
        Label { text: qsTr("Drop TIFF, PNG or JPEG images here. Files stay in their original folders."); color: "#9aa7b8"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 16
            ListView {
                id: list
                Layout.preferredWidth: Math.min(300, root.width * 0.36)
                Layout.fillHeight: true
                model: root.library.images
                clip: true
                spacing: 6
                currentIndex: root.selected ? root.library.images.findIndex(function(entry) { return entry.path === root.selected.path; }) : -1
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AlwaysOff }
                delegate: Rectangle {
                    id: entry
                    required property var modelData
                    required property int index
                    width: list.width
                    height: 82
                    radius: 6
                    color: ListView.isCurrentItem ? "#1b2b42" : "#141a22"
                    border.color: ListView.isCurrentItem ? "#6ca5f8" : "#26303c"
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 3
                        Label { text: entry.modelData.name; color: "#eef3fa"; elide: Text.ElideMiddle; Layout.fillWidth: true }
                        Label { text: entry.modelData.path; color: "#9aa7b8"; font.pixelSize: 11; elide: Text.ElideMiddle; Layout.fillWidth: true }
                        Label { text: entry.modelData.exists ? qsTr("Added ") + new Date(entry.modelData.date).toLocaleString(Qt.locale(root.library.language === "fr" ? "fr_FR" : "en_US"), Locale.ShortFormat) : qsTr("File moved or missing"); color: entry.modelData.exists ? "#9aa7b8" : "#edb06c"; font.pixelSize: 11; elide: Text.ElideRight; Layout.fillWidth: true }
                    }
                    MouseArea { anchors.fill: parent; onClicked: root.selectedPath = entry.modelData.path }
                }
                Label { anchors.centerIn: parent; width: parent.width - 24; visible: list.count === 0; text: qsTr("Drop processed images here, or choose Add images."); color: "#9aa7b8"; wrapMode: Text.WordWrap; horizontalAlignment: Text.AlignHCenter }
            }
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                ImageViewport {
                    id: historyView
                    objectName: "historyViewport"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    sourceWidth: preview.status === Image.Ready ? preview.implicitWidth : 0
                    sourceHeight: preview.status === Image.Ready ? preview.implicitHeight : 0
                    Image {
                        id: preview
                        parent: historyView.imageContainer
                        anchors.fill: parent
                        source: root.selected && root.selected.exists ? root.selected.url + "?revision=" + encodeURIComponent(root.selected.date) : ""
                        fillMode: Image.Stretch
                        asynchronous: true
                        cache: false
                        autoTransform: true
                    }
                    BusyIndicator { anchors.centerIn: parent; running: preview.status === Image.Loading; visible: running }
                    Label {
                        anchors.centerIn: parent
                        width: parent.width - 40
                        text: !root.selected ? qsTr("Select an image") : (!root.selected.exists ? qsTr("This file has been moved or deleted.\nAdd it again from its new location.") : qsTr("Preview unavailable. You can open the image in your image viewer."))
                        visible: !root.selected || !root.selected.exists || preview.status === Image.Error
                        color: "#9aa7b8"; wrapMode: Text.WordWrap; horizontalAlignment: Text.AlignHCenter
                    }
                }
                Label { text: root.selected ? root.selected.path : ""; color: "#a8b5c6"; elide: Text.ElideMiddle; Layout.fillWidth: true }
                RowLayout {
                    enabled: root.selected !== null && root.selected.exists
                    IconButton { text: qsTr("Open image"); onClicked: root.library.openImage(root.selected.url) }
                    IconButton { text: Qt.platform.os === "osx" ? qsTr("Show in Finder") : qsTr("Show in folder"); onClicked: root.library.revealImage(root.selected.url) }
                }
            }
        }
        Label { text: root.library.status; color: "#bdd7fa"; elide: Text.ElideRight; Layout.fillWidth: true; Layout.preferredHeight: 20 }
    }
}
