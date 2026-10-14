import QtQuick
import QtQuick.Controls.Basic

Loader {
    id: root
    property bool visited: false
    active: visible || visited
    asynchronous: true
    onVisibleChanged: if (visible) visited = true
    onLoaded: visited = true
    BusyIndicator {
        anchors.centerIn: parent
        running: root.visible && root.status === Loader.Loading
        visible: running
        Accessible.name: qsTr("Loading workspace")
    }
}
