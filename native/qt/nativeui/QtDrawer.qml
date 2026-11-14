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
//
// Items in the panel. React Native draws them, so QML cannot restyle them; it
// finds the ones that are `accessible` with a button-like `accessibilityRole`
// (the nav items) and draws hover, pressed and keyboard-focus layers over them.
// Keyboard, while the drawer is open and has focus: Up and Down (and Tab and
// Shift+Tab, which wrap, so focus cannot leave the panel) move between nav
// items, Home and End jump to the first and last, Enter and Space press the
// focused one through its accessibility press action, Escape closes. Focus goes
// to the drawer when it opens, is held there while it is open, and returns to
// the item that had it before the drawer opened once it closes.
Item {
    id: root
    visible: true
    clip: true

    // ---- props (set by the Fabric bridge) ---------------------------------
    property bool open: false
    property real drawerWidth: 280
    property string side: "left"
    property string drawerType: "front"
    property bool swipeEnabled: true
    property color scrimColor: "#99000000"
    property string scrimLabel: ""
    property real edgeWidth: 24
    // Px a drag must cover to count, unless it is a fling. 0: only the 40% rule.
    property real swipeMinDistance: 0
    property real swipeExclusionLeft: 0
    property real topInset: 0
    // px/s of release velocity that decides a drag on its own.
    property real flingVelocity: 500
    property var bridge: null

    // ---- nav item states ------------------------------------------------------
    property color itemHoverColor: "#22ffffff"
    property color itemPressedColor: "#33ffffff"
    property color focusRingColor: "#ff79e8dc"
    readonly property var navRoles: ["button", "link", "menuitem", "tab", "radio", "checkbox", "switch"]
    // The panel's nav items in reading order, refreshed on every query.
    property var navItems: []
    property int focusedIndex: -1
    property bool focusVisible: false
    property Item hoveredItem: null
    property Item pressedItem: null
    // The item that had focus when the drawer opened; it gets focus back on close.
    property Item opener: null
    property bool focusEngaged: false

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

    // ---- nav items, focus and states --------------------------------------
    function itemValue(item, name) {
        // Fabric supplies QObject dynamic properties, which QML's dot lookup
        // does not expose. The native bridge reads those through QObject.
        return bridge && typeof bridge.itemProperty === "function"
               ? bridge.itemProperty(item, name) : item[name];
    }

    function isNavItem(item) {
        return item.visible && item.enabled && itemValue(item, "accessible") === true
               && navRoles.indexOf(itemValue(item, "accessibilityRole")) >= 0
               && itemValue(item, "accessibilityStateDisabled") !== true;
    }

    function collectNavItems() {
        var found = [];
        var stack = drawerPanel ? [drawerPanel] : [];
        while (stack.length > 0) {
            var kids = stack.pop().children;
            for (var i = 0; i < kids.length; i++) {
                var kid = kids[i];
                if (!kid.visible)
                    continue;
                if (isNavItem(kid))
                    found.push(kid);
                else
                    stack.push(kid);
            }
        }
        found.sort(function(a, b) {
            var pa = a.mapToItem(root, 0, 0);
            var pb = b.mapToItem(root, 0, 0);
            return (pa.y - pb.y) || (pa.x - pb.x);
        });
        navItems = found;
        if (focusedIndex >= found.length)
            focusedIndex = found.length - 1;
        return found;
    }

    function itemAt(position) {
        var items = collectNavItems();
        for (var i = 0; i < items.length; i++) {
            var topLeft = items[i].mapToItem(root, 0, 0);
            if (position.x >= topLeft.x && position.x < topLeft.x + items[i].width
                    && position.y >= topLeft.y && position.y < topLeft.y + items[i].height)
                return items[i];
        }
        return null;
    }

    function isInside(item) {
        for (var node = item; node; node = node.parent) {
            if (node === root)
                return true;
        }
        return false;
    }

    function placeOver(layer, item) {
        if (!item || !drawerPanel) {
            layer.visible = false;
            return;
        }
        var origin = drawerPanel.mapFromItem(item, 0, 0);
        layer.x = origin.x;
        layer.y = origin.y;
        layer.width = item.width;
        layer.height = item.height;
        layer.radius = typeof item.radius === "number" ? item.radius : 0;
        layer.visible = true;
    }

    // Redraws the hover, pressed and focus layers for the current state.
    function updateLayers() {
        var shown = pressedItem ? pressedItem : hoveredItem;
        stateLayer.color = pressedItem ? itemPressedColor : itemHoverColor;
        placeOver(stateLayer, shown);
        var ringed = (activeFocus && focusVisible && focusedIndex >= 0 && focusedIndex < navItems.length)
                     ? navItems[focusedIndex] : null;
        placeOver(focusRing, ringed);
    }

    function moveFocus(delta) {
        var items = collectNavItems();
        if (items.length === 0)
            return;
        focusedIndex = focusedIndex < 0 ? (delta > 0 ? 0 : items.length - 1)
                                        : (focusedIndex + delta + items.length) % items.length;
        focusVisible = true;
        updateLayers();
    }

    function focusEdge(last) {
        var items = collectNavItems();
        if (items.length === 0)
            return;
        focusedIndex = last ? items.length - 1 : 0;
        focusVisible = true;
        updateLayers();
    }

    // Presses the focused nav item the way an assistive technology would.
    function activateFocused() {
        var items = collectNavItems();
        if (focusedIndex < 0 || focusedIndex >= items.length)
            return false;
        if (bridge && typeof bridge.activateItem === "function")
            return bridge.activateItem(items[focusedIndex]);
        items[focusedIndex].Accessible.pressAction();
        return true;
    }

    // Focus goes to the drawer as it starts to open, remembering who had it.
    function engageFocus() {
        focusEngaged = true;
        var window = root.Window.window;
        var current = window ? window.activeFocusItem : null;
        opener = (current && !isInside(current)) ? current : null;
        var items = collectNavItems();
        focusedIndex = -1;
        for (var i = 0; i < items.length; i++) {
            if (itemValue(items[i], "accessibilityStateSelected") === true) {
                focusedIndex = i;
                break;
            }
        }
        if (focusedIndex < 0 && items.length > 0)
            focusedIndex = 0;
        forceActiveFocus(Qt.PopupFocusReason);
    }

    function releaseFocus() {
        focusEngaged = false;
        focusVisible = false;
        hoveredItem = null;
        pressedItem = null;
        updateLayers();
        var back = opener;
        opener = null;
        if (back && back.visible)
            back.forceActiveFocus(Qt.PopupFocusReason);
    }

    // While the drawer is open, focus that leaves it comes back.
    function enforceFocusTrap() {
        if (!open || progress <= 0.001 || dragging)
            return;
        var window = root.Window.window;
        var current = window ? window.activeFocusItem : null;
        if (current && !isInside(current))
            forceActiveFocus(Qt.TabFocusReason);
    }

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
        if (progress > 0 && !focusEngaged && !dragging)
            engageFocus();
        else if (progress <= 0 && focusEngaged && !dragging)
            releaseFocus();
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

    Keys.onPressed: function(event) {
        if (progress <= 0 || dragging)
            return;
        switch (event.key) {
        case Qt.Key_Tab:
            // Platforms send Backtab for Shift+Tab; some send Tab with Shift.
            moveFocus((event.modifiers & Qt.ShiftModifier) ? -1 : 1);
            break;
        case Qt.Key_Down:
            moveFocus(1);
            break;
        case Qt.Key_Up:
        case Qt.Key_Backtab:
            moveFocus(-1);
            break;
        case Qt.Key_Home:
            focusEdge(false);
            break;
        case Qt.Key_End:
            focusEdge(true);
            break;
        case Qt.Key_Return:
        case Qt.Key_Enter:
        case Qt.Key_Space:
            focusVisible = true;
            if (!activateFocused())
                return;
            break;
        default:
            return;
        }
        event.accepted = true;
    }

    onActiveFocusChanged: {
        if (!activeFocus)
            focusVisible = false;
        updateLayers();
    }

    Connections {
        target: root.Window.window
        function onActiveFocusItemChanged() { root.enforceFocusTrap(); }
    }

    // Hover, pressed and focus layers over the panel's nav items. They sit in
    // the panel, above everything React drew there.
    Rectangle {
        id: stateLayer
        objectName: "rnqnxDrawerItemState"
        parent: root.drawerPanel
        z: 99990
        visible: false
        color: root.itemHoverColor
        Behavior on color { ColorAnimation { duration: 100 } }
    }
    Rectangle {
        id: focusRing
        objectName: "rnqnxDrawerFocusRing"
        parent: root.drawerPanel
        z: 99991
        visible: false
        color: "transparent"
        border.width: 2
        border.color: root.focusRingColor
    }

    HoverHandler {
        acceptedDevices: PointerDevice.Mouse | PointerDevice.Stylus
        enabled: root.progress > 0.001
        onPointChanged: {
            root.hoveredItem = root.itemAt(point.position);
            root.updateLayers();
        }
        onHoveredChanged: {
            if (!hovered) {
                root.hoveredItem = null;
                root.updateLayers();
            }
        }
    }

    // Watches presses without taking them from the React views or the drags.
    PointHandler {
        acceptedButtons: Qt.LeftButton
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchScreen | PointerDevice.Stylus
        enabled: root.progress > 0.001
        onActiveChanged: {
            if (active) {
                root.focusVisible = false;
                root.pressedItem = root.itemAt(point.position);
                var index = root.navItems.indexOf(root.pressedItem);
                if (index >= 0)
                    root.focusedIndex = index;
            } else {
                root.pressedItem = null;
            }
            root.updateLayers();
        }
        onPointChanged: {
            if (!active)
                return;
            var hit = root.itemAt(point.position);
            if (hit !== root.pressedItem) {
                root.pressedItem = hit;
                root.updateLayers();
            }
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
        if (bridge)
            bridge.gestureStart(open);
    }

    // The system took the drag away: settle where the drawer was and say so.
    function cancelDrag() {
        if (!dragging)
            return;
        dragging = false;
        animateTo(open ? 1 : 0);
        if (bridge)
            bridge.gestureCancel(open);
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
        if (bridge)
            bridge.gestureEnd(open);
        var towardOpen = direction * velocityX;
        var moved = progress - dragStartProgress;
        var next;
        if (Math.abs(velocityX) > flingVelocity)
            next = towardOpen > 0;
        else if (Math.abs(moved) * drawerWidth < swipeMinDistance)
            next = dragStartProgress >= 0.5;
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
            onCanceled: root.cancelDrag()
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
        onCanceled: root.cancelDrag()
    }

    Accessible.role: Accessible.Pane
    Accessible.name: "Navigation drawer"
}
