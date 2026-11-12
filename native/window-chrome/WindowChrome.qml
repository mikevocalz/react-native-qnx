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
    // Light only when the platform says the system is light (macOS, Windows).
    // QNX reports no colour scheme, so the bar is dark there, like the QNX
    // desktop launcher and most QNX apps (ADR-0016 item 10).
    readonly property bool light: Qt.styleHints.colorScheme === Qt.ColorScheme.Light
    readonly property color barColor: light ? (chrome.active ? "#ececec" : "#f6f6f6")
                                            : (chrome.active ? "#13202f" : "#172433")
    readonly property color separatorColor: light ? "#d0d0d0" : "#060c14"
    readonly property color titleColor: light ? (chrome.active ? "#1d1d1f" : "#86868b")
                                              : (chrome.active ? "#e6edf3" : "#7d8a99")
    readonly property color glyphInk: light ? "#1d1d1f" : "#e6edf3"
    readonly property color unavailableLight: light ? "#d4d4d4" : "#3b4858"

    x: chrome.frameRect.x
    y: chrome.frameRect.y
    z: 1000
    width: chrome.windowWidth
    height: chrome.barHeight
    visible: chrome.barHeight > 0

    // Outline of a contained window, so it reads as a window on the display.
    Rectangle {
        objectName: "rnqnxWindowFrame"
        visible: root.chrome.contained && !root.maximized
        x: -1
        y: -1
        z: -1
        width: root.chrome.frameRect.width + 2
        height: root.chrome.frameRect.height + 2
        color: "transparent"
        border.width: 1
        border.color: root.light ? "#9aa4b0" : "#3b4858"
    }

    // Resize hint beside the pointer on an edge or corner. QNX Screen has no
    // resize cursor shapes, so the arrow cursor stays and this shows the
    // direction instead; elsewhere the platform cursor changes as well.
    Text {
        objectName: "rnqnxResizeHint"
        readonly property string hint: root.chrome.resizeHint
        visible: root.chrome.contained && hint !== ""
        x: root.chrome.pointer.x - root.x + 12
        y: root.chrome.pointer.y - root.y + 12
        z: 10
        text: "\u2194"
        rotation: hint === "vertical" ? 90 : hint === "diagonal" ? 45 : hint === "antidiagonal" ? -45 : 0
        color: "#ffffff"
        style: Text.Outline
        styleColor: "#000000"
        font.pixelSize: 22
        font.bold: true
    }

    Rectangle {
        id: bar
        objectName: "rnqnxTitleBar"
        anchors.fill: parent
        visible: root.chrome.barVisible
        color: root.barColor
        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 1
            color: root.separatorColor
        }
        // Drag-to-move only where the QPA reports support (never on QNX).
        MouseArea {
            anchors.fill: parent
            enabled: root.chrome.systemMoveAvailable
            acceptedButtons: Qt.LeftButton
            onPressed: root.chrome.beginSystemMove()
        }
        // Double click on the bar toggles maximize, as on macOS and Windows.
        TapHandler {
            enabled: root.chrome.maximizeAvailable
            acceptedButtons: Qt.LeftButton
            onDoubleTapped: root.chrome.trigger("maximize")
        }
        // Icon and title as one group, centred (macOS) or left-aligned
        // (Windows) inside the title rectangle the C++ layout reserved.
        Item {
            id: titleArea
            x: root.chrome.titleRect.x
            y: root.chrome.titleRect.y
            width: root.chrome.titleRect.width
            height: root.chrome.titleRect.height
            visible: width > 0
            readonly property bool hasIcon: icon.status === Image.Ready
            readonly property real iconSpace: hasIcon ? icon.width + 8 : 0
            readonly property real textWidth: Math.max(0, Math.min(title.implicitWidth, width - iconSpace))
            readonly property real groupX: root.chrome.titleCentered
                ? (width - iconSpace - textWidth) / 2 : 0
            Image {
                id: icon
                objectName: "rnqnxWindowIcon"
                x: titleArea.groupX
                anchors.verticalCenter: parent.verticalCenter
                width: 20
                height: 20
                sourceSize.width: 40
                sourceSize.height: 40
                source: root.chrome.iconSource
                fillMode: Image.PreserveAspectFit
                smooth: true
                visible: titleArea.hasIcon
            }
            Text {
                id: title
                objectName: "rnqnxWindowTitle"
                x: titleArea.groupX + titleArea.iconSpace
                width: titleArea.textWidth
                height: parent.height
                text: root.chrome.title
                color: root.titleColor
                font.pixelSize: 14
                font.weight: Font.DemiBold
                elide: Text.ElideRight
                verticalAlignment: Text.AlignVCenter
                Accessible.role: Accessible.TitleBar
                Accessible.name: text
            }
        }
    }

    Repeater {
        model: root.chrome.controls
        delegate: Item {
            id: control
            required property var modelData
            readonly property string action: modelData.action
            readonly property bool live: modelData.enabled !== false
            readonly property bool hovered: hover.hovered
            readonly property bool pressed: tap.pressed
            readonly property var geometry: root.chrome.controlGeometry[action]
            objectName: "rnqnxWindowControl_" + action
            x: geometry.x
            y: geometry.y
            width: geometry.width
            height: geometry.height
            activeFocusOnTab: live

            Accessible.role: Accessible.Button
            Accessible.name: action === "maximize" && root.maximized ? "Restore window" : modelData.name
            Accessible.description: live ? "" : "Not available on this platform"
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
                // Disabled lights are grey, as macOS draws a control the
                // window cannot use.
                color: !control.live ? root.unavailableLight
                     : control.action === "close" ? "#ff5f57"
                     : control.action === "minimize" ? "#febc2e" : "#28c840"
                border.width: 1
                border.color: Qt.darker(color, 1.25)
                opacity: control.pressed && control.live ? 0.7 : 1
                readonly property bool glyphShown: control.live && (root.chrome.alwaysShowGlyphs || control.hovered || control.activeFocus)
                Text {
                    anchors.centerIn: parent
                    visible: parent.glyphShown && control.action !== "maximize"
                    text: control.action === "close" ? "×" : "−"
                    color: "#4d0000"
                    font.pixelSize: 11
                    font.bold: true
                }
                // Full-screen light, drawn like macOS: two triangles pointing
                // out of the corners while the window is compact, and into the
                // centre while it fills the screen.
                Canvas {
                    objectName: "rnqnxExpandGlyph"
                    readonly property string glyph: root.maximized ? "restore" : "expand"
                    anchors.centerIn: parent
                    width: 8
                    height: 8
                    visible: parent.glyphShown && control.action === "maximize"
                    onGlyphChanged: requestPaint()
                    onPaint: {
                        const ctx = getContext("2d")
                        ctx.reset()
                        ctx.fillStyle = "#0a5a12"
                        const tri = (ax, ay, bx, by, cx, cy) => {
                            ctx.beginPath()
                            ctx.moveTo(ax, ay)
                            ctx.lineTo(bx, by)
                            ctx.lineTo(cx, cy)
                            ctx.closePath()
                            ctx.fill()
                        }
                        if (glyph === "expand") {
                            tri(0.5, 0.5, 5.5, 0.5, 0.5, 5.5)
                            tri(7.5, 7.5, 2.5, 7.5, 7.5, 2.5)
                        } else {
                            tri(3.5, 3.5, 0, 3.5, 3.5, 0)
                            tri(4.5, 4.5, 8, 4.5, 4.5, 8)
                        }
                    }
                }
            }

            // Windows layout: full-height cell, red fill for close on hover.
            Rectangle {
                visible: !root.macos
                anchors.fill: parent
                opacity: control.live ? 1 : 0.35
                color: !control.live ? "transparent"
                    : control.action === "close"
                    ? (control.pressed ? "#f1707a" : control.hovered ? "#e81123" : "transparent")
                    : (control.pressed ? (root.light ? "#cccccc" : "#2c3b4d")
                                       : control.hovered ? (root.light ? "#e0e0e0" : "#223142") : "transparent")
                Item {
                    anchors.centerIn: parent
                    width: 10
                    height: 10
                    readonly property color ink: control.action === "close" && control.hovered ? "white" : root.glyphInk
                    Rectangle {
                        visible: control.action === "minimize"
                        anchors.verticalCenter: parent.verticalCenter
                        width: 10; height: 1; color: parent.ink
                    }
                    // Maximize is one square; restore is two overlapping
                    // squares, as Windows draws them.
                    Item {
                        objectName: "rnqnxMaximizeGlyph"
                        readonly property string glyph: root.maximized ? "restore" : "expand"
                        visible: control.action === "maximize"
                        anchors.fill: parent
                        Rectangle {
                            visible: parent.glyph === "expand"
                            anchors.fill: parent
                            color: "transparent"
                            border.width: 1
                            border.color: parent.parent.ink
                        }
                        Rectangle {
                            visible: parent.glyph === "restore"
                            x: 0; y: 2; width: 8; height: 8
                            color: "transparent"
                            border.width: 1
                            border.color: parent.parent.ink
                        }
                        Rectangle {
                            visible: parent.glyph === "restore"
                            x: 2; y: 0; width: 8; height: 1
                            color: parent.parent.ink
                        }
                        Rectangle {
                            visible: parent.glyph === "restore"
                            x: 9; y: 0; width: 1; height: 8
                            color: parent.parent.ink
                        }
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
