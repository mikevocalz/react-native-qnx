import QtQuick

MouseArea {
    id: handle
    required property var appWindow
    required property int edges
    property point pressPosition
    property rect originalGeometry
    property bool manualResize: false
    enabled: appWindow.visibility === Window.Windowed
    hoverEnabled: enabled
    cursorShape: edges === Qt.TopEdge || edges === Qt.BottomEdge ? Qt.SizeVerCursor
        : edges === Qt.LeftEdge || edges === Qt.RightEdge ? Qt.SizeHorCursor
        : edges === (Qt.TopEdge | Qt.LeftEdge) || edges === (Qt.BottomEdge | Qt.RightEdge)
            ? Qt.SizeFDiagCursor : Qt.SizeBDiagCursor
    onPressed: function(mouse) {
        manualResize = false
        if (!appWindow.startSystemResize(edges) && Qt.platform.pluginName === "qnx") {
            pressPosition = mapToGlobal(mouse.x, mouse.y)
            originalGeometry = Qt.rect(appWindow.x, appWindow.y, appWindow.width, appWindow.height)
            manualResize = true
        }
    }
    onPositionChanged: function(mouse) {
        if (!pressed || !manualResize)
            return
        const position = mapToGlobal(mouse.x, mouse.y)
        const dx = position.x - pressPosition.x
        const dy = position.y - pressPosition.y
        const width = Math.max(appWindow.minimumWidth,
            originalGeometry.width + (edges & Qt.LeftEdge ? -dx : edges & Qt.RightEdge ? dx : 0))
        const height = Math.max(appWindow.minimumHeight,
            originalGeometry.height + (edges & Qt.TopEdge ? -dy : edges & Qt.BottomEdge ? dy : 0))
        if (edges & Qt.LeftEdge)
            appWindow.x = originalGeometry.x + originalGeometry.width - width
        if (edges & Qt.TopEdge)
            appWindow.y = originalGeometry.y + originalGeometry.height - height
        appWindow.width = width
        appWindow.height = height
    }
    onReleased: manualResize = false
    onCanceled: manualResize = false
}
