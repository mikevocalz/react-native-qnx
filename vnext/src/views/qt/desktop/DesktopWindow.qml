pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic

FocusScope {
    id: frame
    required property var appWindow
    required property bool nativeMinimizeAvailable
    required property bool clientSideFrame
    property string dialogKind: ""
    readonly property bool popupVisible: fileMenu.visible || windowMenu.visible
        || helpMenu.visible || dialogKind !== ""
    anchors.fill: parent

    Rectangle {
        anchors.fill: parent
        color: "#f3f6fb"
        border.width: frame.clientSideFrame ? 1 : 0
        border.color: frame.appWindow.active ? "#a6b8d0" : "#d2dae5"
        Accessible.ignored: true
    }
    DesktopTitleBar {
        id: titleBar
        visible: frame.clientSideFrame
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 4
        appWindow: frame.appWindow
        nativeMinimizeAvailable: frame.nativeMinimizeAvailable
    }
    MenuBar {
        id: menuBar
        objectName: "rnqnxMenuBar"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.topMargin: frame.clientSideFrame ? titleBar.height + 4 : 0
        anchors.leftMargin: frame.clientSideFrame ? 4 : 0
        anchors.rightMargin: frame.clientSideFrame ? 4 : 0
        background: Rectangle { color: "#f3f6fb" }
        delegate: MenuBarItem {
            id: menuEntry
            focusPolicy: Qt.StrongFocus
            implicitHeight: Math.max(32, implicitContentHeight + 12)
            background: Rectangle {
                radius: 4
                color: menuEntry.highlighted || menuEntry.activeFocus ? "#d9e7fd" : "transparent"
                border.width: menuEntry.activeFocus ? 2 : 0
                border.color: "#175cd3"
            }
            Keys.onPressed: function(event) {
                if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Down) {
                    if (!menuEntry.menu.visible)
                        menuEntry.click()
                    menuEntry.menu.currentIndex = 0
                    event.accepted = true
                }
            }
        }
        Menu {
            id: fileMenu
            objectName: "rnqnxFileMenu"
            title: qsTr("&File")
            popupType: Popup.Item
            Action {
                text: qsTr("&Close window")
                shortcut: "Ctrl+W"
                onTriggered: frame.appWindow.close()
            }
        }
        Menu {
            id: windowMenu
            title: qsTr("&Window")
            popupType: Popup.Item
            Action {
                text: qsTr("&Minimize")
                shortcut: "Ctrl+M"
                enabled: frame.nativeMinimizeAvailable
                onTriggered: frame.appWindow.showMinimized()
            }
            Action {
                text: titleBar.expanded ? qsTr("&Restore window") : qsTr("Ma&ximize")
                onTriggered: titleBar.expanded ? frame.appWindow.showNormal() : frame.appWindow.showMaximized()
            }
            MenuSeparator {}
            Action {
                text: frame.appWindow.visibility === Window.FullScreen ? qsTr("Exit &full screen") : qsTr("&Full screen")
                shortcut: "F11"
                onTriggered: frame.appWindow.visibility === Window.FullScreen
                    ? frame.appWindow.showNormal() : frame.appWindow.showFullScreen()
            }
        }
        Menu {
            id: helpMenu
            title: qsTr("&Help")
            popupType: Popup.Item
            Action {
                text: qsTr("&Keyboard shortcuts")
                shortcut: "F1"
                onTriggered: frame.dialogKind = "shortcuts"
            }
            Action {
                text: qsTr("&About %1").arg(frame.appWindow.title)
                onTriggered: frame.dialogKind = "about"
            }
        }
    }
    Item {
        objectName: "rnqnxDesktopContent"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: menuBar.bottom
        anchors.bottom: parent.bottom
        anchors.leftMargin: frame.clientSideFrame ? 8 : 0
        anchors.rightMargin: frame.clientSideFrame ? 8 : 0
        anchors.bottomMargin: frame.clientSideFrame ? 8 : 0
        enabled: !frame.popupVisible
        clip: true
    }
    Shortcut {
        sequence: "F10"
        onActivated: menuBar.itemAt(0).forceActiveFocus(Qt.ShortcutFocusReason)
    }
    Loader {
        id: dialogLoader
        active: frame.dialogKind !== ""
        onLoaded: {
            if (dialogLoader.status === Loader.Ready && dialogLoader.item)
                (dialogLoader.item as Dialog).open()
        }
        sourceComponent: Dialog {
            objectName: "rnqnxDesktopDialog"
            parent: Overlay.overlay
            popupType: Popup.Item
            anchors.centerIn: parent
            width: Math.min(460, frame.width - 48)
            title: frame.dialogKind === "shortcuts" ? qsTr("Keyboard shortcuts")
                : qsTr("About %1").arg(frame.appWindow.title)
            modal: true
            focus: true
            standardButtons: Dialog.Ok
            onClosed: frame.dialogKind = ""
            contentItem: Label {
                text: frame.dialogKind === "shortcuts"
                    ? qsTr("F10   Focus the menu bar\nArrow keys   Navigate menus\nEnter   Activate a focused control\nEscape   Close a menu or dialog\nF11   Toggle full screen\nCtrl+W   Close the window")
                    : qsTr("%1\n\nBuilt with React Native QNX.\nReact Native content in a native Qt Quick window.").arg(frame.appWindow.title)
                wrapMode: Text.WordWrap
                padding: 12
            }
        }
    }
    Repeater {
        model: [Qt.TopEdge, Qt.BottomEdge, Qt.LeftEdge, Qt.RightEdge,
            Qt.TopEdge | Qt.LeftEdge, Qt.TopEdge | Qt.RightEdge,
            Qt.BottomEdge | Qt.LeftEdge, Qt.BottomEdge | Qt.RightEdge]
        delegate: WindowResizeHandle {
            required property int modelData
            visible: frame.clientSideFrame
            appWindow: frame.appWindow
            edges: modelData
            readonly property bool corner: (edges & (Qt.LeftEdge | Qt.RightEdge)) !== 0
                && (edges & (Qt.TopEdge | Qt.BottomEdge)) !== 0
            width: corner ? 8 : edges & (Qt.LeftEdge | Qt.RightEdge) ? 4 : frame.width - 16
            height: corner ? 8 : edges & (Qt.TopEdge | Qt.BottomEdge) ? 4 : frame.height - 16
            x: edges & Qt.LeftEdge ? 0 : edges & Qt.RightEdge ? frame.width - width : 8
            y: edges & Qt.TopEdge ? 0 : edges & Qt.BottomEdge ? frame.height - height : 8
        }
    }
}
