import QtQuick

// Client-drawn title bar (ADR-0016). Geometry and the list of live controls
// come from the C++ WindowChrome controller; this file only draws them and
// forwards activations. QtQuick only: no Controls dependency on the target.
Item {
    id: root
    objectName: "rnqnxWindowChrome"
    required property QtObject chrome
    readonly property bool macos: chrome.controlsLayout === "macos"
    readonly property bool maximized: chrome.maximized

    x: 0
    y: 0
    z: 1000
    width: chrome.windowWidth
    height: chrome.barHeight
    visible: chrome.barHeight > 0

    Rectangle {
        id: bar
        objectName: "rnqnxTitleBar"
        anchors.fill: parent
        visible: root.chrome.barVisible
        color: root.chrome.active ? "#ececec" : "#f6f6f6"
        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 1
            color: "#d0d0d0"
        }
        // Drag-to-move only where the QPA reports support (never on QNX).
        MouseArea {
            anchors.fill: parent
            enabled: root.chrome.systemMoveAvailable
            acceptedButtons: Qt.LeftButton
            onPressed: root.chrome.beginSystemMove()
        }
        Text {
            objectName: "rnqnxWindowTitle"
            x: root.chrome.titleRect.x
            y: root.chrome.titleRect.y
            width: root.chrome.titleRect.width
            height: root.chrome.titleRect.height
            visible: width > 0
            text: root.chrome.title
            color: root.chrome.active ? "#1d1d1f" : "#86868b"
            font.pixelSize: 14
            font.weight: Font.DemiBold
            elide: Text.ElideRight
            horizontalAlignment: root.chrome.titleCentered ? Text.AlignHCenter : Text.AlignLeft
            verticalAlignment: Text.AlignVCenter
            Accessible.role: Accessible.TitleBar
            Accessible.name: text
        }
    }

    Repeater {
        model: root.chrome.controls
        delegate: Item {
            id: control
            required property var modelData
            readonly property string action: modelData.action
            readonly property bool hovered: hover.hovered
            readonly property bool pressed: tap.pressed
            objectName: "rnqnxWindowControl_" + action
            x: modelData.x
            y: modelData.y
            width: modelData.width
            height: modelData.height
            activeFocusOnTab: true

            Accessible.role: Accessible.Button
            Accessible.name: action === "maximize" && root.maximized ? "Restore window" : modelData.name
            Accessible.focusable: true
            Accessible.onPressAction: root.chrome.trigger(action)
            Keys.onReturnPressed: root.chrome.trigger(action)
            Keys.onEnterPressed: root.chrome.trigger(action)
            Keys.onSpacePressed: root.chrome.trigger(action)

            HoverHandler { id: hover }
            TapHandler {
                id: tap
                gesturePolicy: TapHandler.WithinBounds
                onTapped: root.chrome.trigger(control.action)
            }

            // macOS layout: 14 px round glyph centred in the 44 px target.
            Rectangle {
                visible: root.macos
                anchors.centerIn: parent
                width: 14
                height: 14
                radius: 7
                color: control.action === "close" ? "#ff5f57"
                     : control.action === "minimize" ? "#febc2e" : "#28c840"
                border.width: 1
                border.color: Qt.darker(color, 1.25)
                opacity: control.pressed ? 0.7 : 1
                Text {
                    anchors.centerIn: parent
                    visible: control.hovered || control.activeFocus
                    text: control.action === "close" ? "×"
                        : control.action === "minimize" ? "−"
                        : root.maximized ? "↙" : "↗"
                    color: "#4d0000"
                    font.pixelSize: 11
                    font.bold: true
                }
            }

            // Windows layout: full-height cell, red fill for close on hover.
            Rectangle {
                visible: !root.macos
                anchors.fill: parent
                color: control.action === "close"
                    ? (control.pressed ? "#f1707a" : control.hovered ? "#e81123" : "transparent")
                    : (control.pressed ? "#cccccc" : control.hovered ? "#e0e0e0" : "transparent")
                Item {
                    anchors.centerIn: parent
                    width: 10
                    height: 10
                    readonly property color ink: control.action === "close" && control.hovered ? "white" : "#1d1d1f"
                    Rectangle {
                        visible: control.action === "minimize"
                        anchors.verticalCenter: parent.verticalCenter
                        width: 10; height: 1; color: parent.ink
                    }
                    Rectangle {
                        visible: control.action === "maximize"
                        anchors.fill: parent
                        color: "transparent"
                        border.width: 1
                        border.color: parent.ink
                    }
                    Rectangle {
                        visible: control.action === "close"
                        anchors.centerIn: parent
                        width: 13; height: 1; rotation: 45; color: parent.ink
                    }
                    Rectangle {
                        visible: control.action === "close"
                        anchors.centerIn: parent
                        width: 13; height: 1; rotation: -45; color: parent.ink
                    }
                }
            }

            // Focus ring only for keyboard focus; taps do not take focus.
            Rectangle {
                objectName: "focusRing"
                anchors.fill: parent
                anchors.margins: 3
                visible: control.activeFocus
                color: "transparent"
                radius: root.macos ? width / 2 : 2
                border.width: 2
                border.color: "#0a64d6"
            }
        }
    }
}
