import QtQuick
import QtQuick.Controls.Basic

Rectangle {
    id: bar
    required property var appWindow
    required property bool nativeMinimizeAvailable
    readonly property bool expanded: appWindow.visibility === Window.Maximized
        || appWindow.visibility === Window.FullScreen
    height: 48
    color: "#f3f6fb"

    MouseArea {
        id: moveArea
        anchors.fill: parent
        anchors.rightMargin: controls.width + 8
        property point pressPosition
        property point windowPosition
        property bool manualMove: false
        onPressed: function(mouse) {
            manualMove = false
            if (bar.expanded)
                return
            if (!bar.appWindow.startSystemMove() && Qt.platform.pluginName === "qnx") {
                pressPosition = mapToGlobal(mouse.x, mouse.y)
                windowPosition = Qt.point(bar.appWindow.x, bar.appWindow.y)
                manualMove = true
            }
        }
        onPositionChanged: function(mouse) {
            if (!pressed || !manualMove)
                return
            const position = mapToGlobal(mouse.x, mouse.y)
            bar.appWindow.x = windowPosition.x + position.x - pressPosition.x
            bar.appWindow.y = windowPosition.y + position.y - pressPosition.y
        }
        onReleased: manualMove = false
        onCanceled: manualMove = false
        onDoubleClicked: bar.expanded ? bar.appWindow.showNormal() : bar.appWindow.showMaximized()
    }
    Rectangle {
        x: 16
        anchors.verticalCenter: parent.verticalCenter
        width: 10
        height: 10
        radius: 3
        color: "#175cd3"
        Accessible.ignored: true
    }
    Label {
        anchors.left: parent.left
        anchors.leftMargin: 36
        anchors.right: controls.left
        anchors.rightMargin: 16
        anchors.verticalCenter: parent.verticalCenter
        text: bar.appWindow.title
        color: "#233146"
        font.weight: Font.Medium
        elide: Text.ElideRight
    }
    Row {
        id: controls
        anchors.right: parent.right
        anchors.rightMargin: 6
        anchors.verticalCenter: parent.verticalCenter
        spacing: 2
        WindowButton {
            objectName: "rnqnxMinimizeButton"
            symbol: 0
            text: bar.nativeMinimizeAvailable ? qsTr("Minimize")
                : qsTr("Minimize requires a desktop window manager")
            enabled: bar.nativeMinimizeAvailable
            onClicked: bar.appWindow.showMinimized()
        }
        WindowButton {
            objectName: "rnqnxMaximizeButton"
            symbol: bar.expanded ? 2 : 1
            text: bar.expanded ? qsTr("Restore window") : qsTr("Maximize")
            onClicked: bar.expanded ? bar.appWindow.showNormal() : bar.appWindow.showMaximized()
        }
        WindowButton {
            objectName: "rnqnxCloseButton"
            symbol: 3
            destructive: true
            text: qsTr("Close window")
            onClicked: bar.appWindow.close()
        }
    }
}
