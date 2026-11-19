pragma ComponentBehavior: Bound
import QtQuick

// RNQnxTabBar: a bottom tab bar that becomes a left rail on wide windows.
//
// The mode is decided here, not by JavaScript, so a window resize switches it
// without a React render. The size it occupies goes back to JavaScript through
// layoutModeChanged(mode, size); JavaScript uses that to inset the content.
// Items are positioned from this item's own mode, never from the frame Yoga
// last applied, so a frame that lags one pass cannot draw a half-switched bar.
FocusScope {
    id: root
    visible: !hidden
    clip: true

    // ---- props (set by the Fabric bridge) ---------------------------------
    property var tabs: []
    property string activeKey: ""
    property string layout: "auto"
    // Where the bar sits: bottom, top, left or right. Empty follows `layout`
    // (bottom, or a left rail when the window is wide).
    property string position: ""
    property bool showLabel: true
    // Right-to-left: the bar's tabs run right to left, and a rail on the
    // start side sits on the right.
    property bool rtl: false
    property real sideBreakpoint: 700
    property real railWidth: 88
    property real barHeight: 60
    property real availableWidth: 0
    property color tintActive: "#4da3ff"
    property color tintInactive: "#8a94a3"
    property color badgeColor: "#e5484d"
    property color badgeTextColor: "#ffffff"
    property color dividerColor: "#33ffffff"
    property color backgroundColor: "#13202f"
    property bool hidden: false
    property var bridge: null

    readonly property real measuredWidth: availableWidth > 0 ? availableWidth
                                          : (parent ? parent.width : width)
    readonly property string mode: layout === "bottom" ? "bottom"
                                   : layout === "side" ? "side"
                                   : (measuredWidth >= sideBreakpoint ? "side" : "bottom")
    readonly property string edge: position === "top" || position === "left" || position === "right"
                                   || position === "bottom" ? position : (mode === "side" ? "left" : "bottom")
    readonly property bool sideMode: edge === "left" || edge === "right"
    // `left` is the start side and `right` the end side (react-navigation's
    // tabBarPosition), so right-to-left puts a `left` rail on the physical right.
    readonly property bool railOnRight: sideMode && ((edge === "right") !== rtl)
    readonly property real occupied: hidden ? 0 : (sideMode ? railWidth : barHeight)
    readonly property int count: tabs ? tabs.length : 0

    property int focusedIndex: 0
    property bool focusVisible: false
    property bool ready: false
    property string reportedMode: ""
    property string reportedEdge: ""
    property real reportedSize: -1

    // The mounting layer decorates nothing here; this bar has no React children.
    readonly property real tabExtent: sideMode ? (showLabel ? 72 : 52) : (count > 0 ? width / count : width)

    function activeIndex() {
        for (var i = 0; i < count; i++) {
            if (tabs[i].key === activeKey)
                return i;
        }
        return -1;
    }

    function isImageUri(icon) {
        return icon.indexOf("://") > 0 || icon.indexOf("data:") === 0
            || icon.indexOf("/") === 0 || icon.indexOf(":/") === 0;
    }

    function report() {
        if (!ready || !bridge)
            return;
        var reported = sideMode ? "side" : "bottom";
        if (reportedMode === reported && reportedSize === occupied && reportedEdge === edge)
            return;
        reportedMode = reported;
        reportedSize = occupied;
        reportedEdge = edge;
        bridge.layoutModeChanged(reported, occupied);
    }

    function press(index) {
        if (index < 0 || index >= count)
            return;
        focusedIndex = index;
        if (bridge)
            bridge.tabPressed(tabs[index].key);
    }

    function longPress(index) {
        if (index < 0 || index >= count)
            return;
        focusedIndex = index;
        if (bridge)
            bridge.tabLongPressed(tabs[index].key);
    }

    function moveFocus(delta) {
        if (count === 0)
            return;
        focusedIndex = (focusedIndex + delta + count) % count;
        focusVisible = true;
    }

    onEdgeChanged: Qt.callLater(report)
    onModeChanged: Qt.callLater(report)
    onOccupiedChanged: Qt.callLater(report)
    onBridgeChanged: Qt.callLater(report)
    onTabsChanged: {
        if (focusedIndex >= count)
            focusedIndex = Math.max(0, count - 1);
    }
    onActiveKeyChanged: {
        var index = activeIndex();
        if (index >= 0)
            focusedIndex = index;
    }
    Component.onCompleted: Qt.callLater(function() {
        root.ready = true;
        var index = root.activeIndex();
        if (index >= 0)
            root.focusedIndex = index;
        root.report();
    })

    Accessible.role: Accessible.PageTabList
    Accessible.name: "Tabs"
    activeFocusOnTab: true
    onActiveFocusChanged: {
        if (!activeFocus)
            focusVisible = false;
    }

    Keys.onPressed: function(event) {
        switch (event.key) {
        case Qt.Key_Left:
            moveFocus(sideMode ? 0 : (rtl ? 1 : -1));
            event.accepted = !sideMode;
            break;
        case Qt.Key_Up:
            moveFocus(-1);
            event.accepted = true;
            break;
        case Qt.Key_Right:
            moveFocus(sideMode ? 0 : (rtl ? -1 : 1));
            event.accepted = !sideMode;
            break;
        case Qt.Key_Down:
            moveFocus(1);
            event.accepted = true;
            break;
        case Qt.Key_Escape:
            // Leaves the bar: focus returns to the content.
            focusVisible = false;
            focusedIndex = Math.max(0, activeIndex());
            root.focus = false;
            event.accepted = true;
            break;
        case Qt.Key_Home:
            focusedIndex = 0;
            focusVisible = true;
            event.accepted = true;
            break;
        case Qt.Key_End:
            focusedIndex = Math.max(0, count - 1);
            focusVisible = true;
            event.accepted = true;
            break;
        case Qt.Key_Return:
        case Qt.Key_Enter:
        case Qt.Key_Space:
            focusVisible = true;
            press(focusedIndex);
            event.accepted = true;
            break;
        default:
            break;
        }
    }

    Rectangle {
        anchors.fill: parent
        color: root.backgroundColor
    }

    // A hairline on the edge that faces the content.
    Rectangle {
        color: root.dividerColor
        width: root.sideMode ? 1 : root.width
        height: root.sideMode ? root.height : 1
        // The edge that faces the content: right of a left rail, left of a
        // right rail, top of a bottom bar, bottom of a top bar.
        x: root.sideMode ? (root.railOnRight ? 0 : root.width - 1) : 0
        y: root.sideMode ? 0 : (root.edge === "top" ? root.height - 1 : 0)
    }

    Repeater {
        model: root.count

        delegate: Item {
            id: tab
            required property int index
            readonly property var info: root.tabs[index]
            readonly property bool active: info.key === root.activeKey
            readonly property string iconText: info.icon ? info.icon : ""
            readonly property bool hasBadge: info.badge !== undefined && info.badge !== null
                                             && String(info.badge).length > 0
            readonly property color tint: active ? root.tintActive : root.tintInactive

            x: root.sideMode ? 0 : (root.rtl ? root.width - (index + 1) * root.tabExtent : index * root.tabExtent)
            y: root.sideMode ? 8 + index * root.tabExtent : 0
            width: root.sideMode ? root.width : root.tabExtent
            height: root.sideMode ? root.tabExtent : root.height

            Accessible.role: Accessible.PageTab
            Accessible.name: (info.accessibilityLabel ? info.accessibilityLabel : info.title)
                             + (hasBadge ? ", " + info.badge : "")
            Accessible.selected: active
            Accessible.focusable: true
            Accessible.onPressAction: root.press(index)

            Rectangle {
                id: plate
                anchors.fill: parent
                anchors.margins: root.sideMode ? 6 : 4
                radius: 10
                // The wash comes from the tab's own tint, which the app picks to
                // read against its bar color. A fixed white wash vanished on a
                // light bar.
                color: Qt.rgba(tab.tint.r, tab.tint.g, tab.tint.b,
                               tap.pressed ? (tab.active ? 0.30 : 0.24)
                                           : (tab.active ? 0.16 : (hover.hovered ? 0.12 : 0)))
                Behavior on color { ColorAnimation { duration: 120 } }
            }

            // Focus ring: only for keyboard focus.
            Rectangle {
                anchors.fill: plate
                radius: plate.radius
                color: "transparent"
                border.width: 2
                border.color: root.tintActive
                visible: root.activeFocus && root.focusVisible && root.focusedIndex === tab.index
            }

            Column {
                anchors.centerIn: parent
                spacing: 2
                scale: tap.pressed ? 0.94 : 1
                Behavior on scale { NumberAnimation { duration: 90 } }

                Item {
                    id: iconBox
                    width: 28
                    height: 28
                    anchors.horizontalCenter: parent.horizontalCenter

                    Text {
                        anchors.centerIn: parent
                        visible: tab.iconText.length > 0 && !root.isImageUri(tab.iconText)
                        text: tab.iconText
                        color: tab.tint
                        font.pixelSize: 22
                    }
                    Image {
                        anchors.fill: parent
                        visible: tab.iconText.length > 0 && root.isImageUri(tab.iconText)
                        source: visible && root.bridge ? root.bridge.resolveImage(tab.iconText) : ""
                        fillMode: Image.PreserveAspectFit
                        asynchronous: true
                    }

                    Rectangle {
                        visible: tab.hasBadge
                        x: parent.width - 8
                        y: -4
                        height: 16
                        width: Math.max(16, badgeLabel.implicitWidth + 8)
                        radius: 8
                        color: root.badgeColor
                        Text {
                            id: badgeLabel
                            anchors.centerIn: parent
                            text: tab.hasBadge ? String(tab.info.badge) : ""
                            color: root.badgeTextColor
                            font.pixelSize: 10
                            font.bold: true
                        }
                    }
                }

                Text {
                    visible: root.showLabel
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: Math.min(implicitWidth, tab.width - 8)
                    elide: Text.ElideRight
                    horizontalAlignment: Text.AlignHCenter
                    text: tab.info.title
                    color: tab.tint
                    font.pixelSize: 11
                    font.bold: tab.active
                }
            }

            HoverHandler {
                id: hover
                cursorShape: Qt.PointingHandCursor
            }
            TapHandler {
                id: tap
                acceptedButtons: Qt.LeftButton
                onLongPressed: root.longPress(tab.index)
                onPressedChanged: {
                    if (pressed) {
                        root.focusVisible = false;
                        root.forceActiveFocus(Qt.MouseFocusReason);
                    }
                }
                onTapped: root.press(tab.index)
            }
        }
    }
}
