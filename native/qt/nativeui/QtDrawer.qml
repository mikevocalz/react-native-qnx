import QtQuick

// RNQnxDrawer: a side drawer with a scrim, driven by one `progress` value.
//
// React Native mounts the drawer's two children into `container`, in order:
// the first is the drawer panel, the second is the main content. Yoga owns
// both frames (JavaScript styles them absolute at the container's left edge,
// the panel `drawerWidth` wide); this item only adds a QML Translate transform
// to each, so a drawer on the right and the slide animation need no relayout.
// The scrim is the last child of the main content, above everything in it, so
// it is also what the touch dispatcher's hit test finds there. Nothing here
// uses Reanimated or a gesture-handler package.
Item {
    id: root
    visible: true
    clip: true
    focus: true

    // ---- props (set by the Fabric bridge) ---------------------------------
    property bool open: false
    property real drawerWidth: 280
    property string side: "left"
    property string drawerType: "front"
    property bool swipeEnabled: true
    property color scrimColor: "#99000000"
    property string scrimLabel: ""
    property real edgeWidth: 24
    property real swipeExclusionLeft: 0
    property real topInset: 0
    // px/s of release velocity that decides a drag on its own.
    property real flingVelocity: 500
    property var bridge: null

    // The mounting layer puts React children on `contentItem`.
    readonly property Item contentItem: container

    // 0 is closed, 1 is open. Animated, or driven by a drag.
    property real progress: 0
    property bool dragging: false
    property bool ready: false
    // The state the user asked for and JavaScript has not confirmed yet.
    property var pendingOpen: null

    readonly property int direction: side === "right" ? -1 : 1
    // The panel sits at x = 0 from Yoga. A right drawer starts that far over.
    readonly property real panelOffset: (side === "right" ? width - drawerWidth : 0)
                                        - (panelBehind ? 0 : direction * (1 - progress) * drawerWidth)
    // `slide` pushes the content along with the panel. `back` keeps the panel
    // in place under the content and slides the content away from it.
    readonly property real mainOffset: (drawerType === "slide" || drawerType === "back")
                                       ? direction * progress * drawerWidth : 0
    readonly property bool panelBehind: drawerType === "back"

    property Item drawerPanel: null
    property Item mainPanel: null
    property int lastReported: -1

    function clamp01(v) { return Math.max(0, Math.min(1, v)); }

    function syncPanels() {
        var panels = [];
        var kids = container.children;
        for (var i = 0; i < kids.length; i++) {
            if (kids[i] !== scrim)
                panels.push(kids[i]);
        }
        var nextDrawer = panels.length > 0 ? panels[0] : null;
        var nextMain = panels.length > 1 ? panels[1] : null;
        if (nextDrawer !== drawerPanel) {
            if (drawerPanel)
                drawerPanel.transform = [];
            drawerPanel = nextDrawer;
            if (drawerPanel) {
                drawerPanel.z = root.panelBehind ? -1 : 2;
                drawerPanel.transform = [panelTranslate];
            }
        }
        if (nextMain !== mainPanel) {
            if (mainPanel)
                mainPanel.transform = [];
            mainPanel = nextMain;
            if (mainPanel) {
                mainPanel.z = 0;
                mainPanel.transform = [mainTranslate];
            }
        }
        if (drawerPanel) {
            drawerPanel.visible = progress > 0;
            drawerPanel.z = panelBehind ? -1 : 2;
        }
    }
    onPanelBehindChanged: syncPanels()

    readonly property Translate panelTranslate: Translate { x: root.panelOffset }
    readonly property Translate mainTranslate: Translate { x: root.mainOffset }

    function animateTo(target) {
        anim.stop();
        var distance = Math.abs(target - progress);
        if (distance < 0.001) {
            progress = target;
            reportSettled();
            return;
        }
        anim.to = target;
        anim.duration = Math.round(80 + 180 * distance);
        anim.start();
    }

    function reportSettled() {
        if (!ready || !bridge || dragging)
            return;
        var state = progress >= 0.999 ? 1 : (progress <= 0.001 ? 0 : -1);
        if (state < 0 || state === lastReported)
            return;
        lastReported = state;
        bridge.transitionEnd(state === 1);
    }

    function requestChange(next, reason) {
        pendingOpen = next;
        revertTimer.restart();
        animateTo(next ? 1 : 0);
        if (bridge)
            bridge.requestChange(next, reason);
    }

    onProgressChanged: {
        if (drawerPanel)
            drawerPanel.visible = progress > 0;
        if (progress > 0 && !activeFocus && !dragging)
            forceActiveFocus();
    }

    onOpenChanged: {
        if (dragging)
            return;
        if (pendingOpen !== null && pendingOpen === open)
            revertTimer.stop();
        pendingOpen = null;
        if (!ready) {
            progress = open ? 1 : 0;
            return;
        }
        animateTo(open ? 1 : 0);
    }

    Component.onCompleted: {
        syncPanels();
        // Props arrive right after creation; the first state is not animated.
        Qt.callLater(function() {
            if (root === null)
                return;
            root.ready = true;
            root.progress = root.open ? 1 : 0;
            root.lastReported = root.progress >= 0.999 ? 1 : 0;
        });
    }

    NumberAnimation {
        id: anim
        target: root
        property: "progress"
        easing.type: Easing.OutCubic
        onFinished: root.reportSettled()
    }

    // If JavaScript does not confirm a user-requested change, return to the
    // state it controls.
    Timer {
        id: revertTimer
        interval: 400
        onTriggered: {
            if (root.pendingOpen !== null && root.pendingOpen !== root.open)
                root.animateTo(root.open ? 1 : 0);
            root.pendingOpen = null;
        }
    }

    Keys.onEscapePressed: function(event) {
        if (progress > 0 && !dragging) {
            requestChange(false, "escape");
            event.accepted = true;
        }
    }

    Item {
        id: container
        anchors.fill: parent
        // The mounting layer adds a child and then restacks it with stackBefore;
        // Qt does not signal the restack, so sync again once the event loop runs
        // (and the bridge calls syncPanels after every prop update).
        onChildrenChanged: {
            root.syncPanels();
            Qt.callLater(root.syncPanels);
        }
    }

    // Covers the main content while the drawer is open. It is the last child
    // of the main content (z above all of it), so the touch dispatcher's hit
    // test lands on it instead of on a button underneath, and the tap closes
    // the drawer. Before the main content exists it waits in `container`.
    Rectangle {
        id: scrim
        objectName: "rnqnxDrawerScrim"
        parent: root.mainPanel ? root.mainPanel : container
        x: 0
        y: 0
        width: parent ? parent.width : 0
        height: parent ? parent.height : 0
        z: 100000
        color: root.scrimColor
        // In `back` the content is pushed aside, and the scrim still shades it.
        opacity: root.progress
        visible: root.progress > 0 && root.mainPanel !== null

        Accessible.role: Accessible.Button
        Accessible.name: root.scrimLabel.length > 0 ? root.scrimLabel : "Close navigation drawer"
        Accessible.onPressAction: root.requestChange(false, "scrim")

        TapHandler {
            acceptedButtons: Qt.LeftButton
            onTapped: root.requestChange(false, "scrim")
        }
    }

    // ---- gestures ---------------------------------------------------------
    property real dragStartProgress: 0

    function beginDrag() {
        anim.stop();
        revertTimer.stop();
        dragStartProgress = progress;
        dragging = true;
    }

    function updateDrag(deltaX) {
        if (!dragging)
            return;
        progress = clamp01(dragStartProgress + direction * deltaX / Math.max(1, drawerWidth));
    }

    function endDrag(velocityX) {
        if (!dragging)
            return;
        dragging = false;
        var towardOpen = direction * velocityX;
        var moved = progress - dragStartProgress;
        var next;
        if (Math.abs(velocityX) > flingVelocity)
            next = towardOpen > 0;
        else if (Math.abs(moved) >= 0.4)
            next = moved > 0;
        else
            next = dragStartProgress >= 0.5;
        if (next === open && pendingOpen === null) {
            // Back to where it started: settle without asking JavaScript.
            animateTo(next ? 1 : 0);
        } else {
            requestChange(next, "swipe");
        }
    }

    // Opens from the screen edge. It starts past `swipeExclusionLeft`, so a
    // navigation rail on the left keeps its own clicks and drags.
    Item {
        id: edgeStrip
        z: 3
        enabled: root.swipeEnabled && root.progress <= 0 && !root.open
        visible: enabled
        y: root.topInset
        height: Math.max(0, root.height - root.topInset)
        width: root.edgeWidth
        x: root.side === "right" ? root.width - root.edgeWidth
                                 : Math.min(root.swipeExclusionLeft, Math.max(0, root.width - root.edgeWidth))

        DragHandler {
            id: openDrag
            target: null
            yAxis.enabled: false
            acceptedButtons: Qt.LeftButton
            onActiveChanged: {
                if (active)
                    root.beginDrag();
                else
                    root.endDrag(centroid.velocity.x);
            }
            onTranslationChanged: root.updateDrag(translation.x)
        }
    }

    // Closes by dragging the open drawer or its scrim.
    DragHandler {
        id: closeDrag
        target: null
        yAxis.enabled: false
        acceptedButtons: Qt.LeftButton
        enabled: root.swipeEnabled && (root.open || root.progress > 0) && !openDrag.active
        onActiveChanged: {
            if (active)
                root.beginDrag();
            else
                root.endDrag(centroid.velocity.x);
        }
        onTranslationChanged: root.updateDrag(translation.x)
    }

    Accessible.role: Accessible.Pane
    Accessible.name: "Navigation drawer"
}
