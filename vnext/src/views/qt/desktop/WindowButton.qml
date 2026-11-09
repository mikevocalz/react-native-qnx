import QtQuick
import QtQuick.Controls.Basic

ToolButton {
    id: control
    required property int symbol
    property bool destructive: false
    implicitWidth: 44
    implicitHeight: 36
    focusPolicy: Qt.StrongFocus
    Accessible.name: text
    ToolTip.visible: hovered || activeFocus
    ToolTip.text: text
    ToolTip.delay: 500
    background: Rectangle {
        radius: 5
        color: control.down ? (control.destructive ? "#b91c1c" : "#d9e2ef")
            : control.hovered ? (control.destructive ? "#dc2626" : "#e5ebf4") : "transparent"
        border.width: control.activeFocus ? 2 : 0
        border.color: "#175cd3"
    }
    contentItem: Item {
        readonly property color ink: !control.enabled ? "#929eae"
            : control.destructive && (control.hovered || control.down) ? "white" : "#233146"
        Rectangle {
            anchors.centerIn: parent
            width: 12
            height: control.symbol === 0 ? 2 : 10
            color: control.symbol === 0 ? parent.ink : "transparent"
            border.width: control.symbol === 1 || control.symbol === 2 ? 1 : 0
            border.color: parent.ink
            visible: control.symbol !== 3
        }
        Rectangle {
            x: parent.width / 2 - 3
            y: parent.height / 2 - 7
            width: 10
            height: 9
            color: "transparent"
            border.width: 1
            border.color: parent.ink
            visible: control.symbol === 2
        }
        Rectangle {
            anchors.centerIn: parent
            width: 14
            height: 2
            color: parent.ink
            rotation: 45
            visible: control.symbol === 3
        }
        Rectangle {
            anchors.centerIn: parent
            width: 14
            height: 2
            color: parent.ink
            rotation: -45
            visible: control.symbol === 3
        }
    }
}
