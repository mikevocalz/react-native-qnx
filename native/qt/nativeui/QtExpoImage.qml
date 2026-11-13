import QtQuick

// ViewManagerAdapter_ExpoImage: the native view behind expo-image's <Image>.
//
// Sources are resolved by the Fabric bridge through the platform's image
// provider, so a source that was prefetched is served from the shared decoded
// cache (DecodedImageCache) and nothing is cached a second time here. The
// bridge passes `sourceUrl` and `placeholderUrl` already resolved.
Item {
    id: root
    visible: true
    clip: true

    // ---- props (set by the Fabric bridge) ---------------------------------
    property string sourceUrl: ""
    property string placeholderUrl: ""
    property string contentFit: "cover"
    property string horizontalPosition: "center"
    property string verticalPosition: "center"
    property real transitionMs: 0
    property color tintColor: "transparent"
    property bool hasTint: false
    property real blurRadius: 0
    property bool cacheImages: true
    property string reportedCacheType: "none"
    property var bridge: null
    // The view's own style, set by the mounting layer like any other view.
    property color backgroundColor: "transparent"
    property real radius: 0

    // ---- drawing ----------------------------------------------------------
    property bool displayed: false
    property int lastStatus: -1

    readonly property int fillMode: {
        switch (contentFit) {
        case "contain": return Image.PreserveAspectFit;
        case "fill": return Image.Stretch;
        case "none": return Image.Pad;
        case "scale-down": return main.implicitWidth <= width && main.implicitHeight <= height
                                  ? Image.Pad : Image.PreserveAspectFit;
        default: return Image.PreserveAspectCrop;
        }
    }
    readonly property int hAlign: horizontalPosition === "left" ? Image.AlignLeft
                                  : horizontalPosition === "right" ? Image.AlignRight : Image.AlignHCenter
    readonly property int vAlign: verticalPosition === "top" ? Image.AlignTop
                                  : verticalPosition === "bottom" ? Image.AlignBottom : Image.AlignVCenter

    Rectangle {
        anchors.fill: parent
        color: root.backgroundColor
        radius: root.radius
    }

    // Shown until the main image is ready (and kept under it during the fade).
    Image {
        id: placeholder
        anchors.fill: parent
        source: root.placeholderUrl
        asynchronous: true
        cache: root.cacheImages
        fillMode: root.fillMode
        horizontalAlignment: root.hAlign
        verticalAlignment: root.vAlign
        visible: status === Image.Ready && (main.status !== Image.Ready || main.opacity < 1)
    }

    Image {
        id: main
        anchors.fill: parent
        source: root.sourceUrl
        asynchronous: true
        cache: root.cacheImages
        fillMode: root.fillMode
        horizontalAlignment: root.hAlign
        verticalAlignment: root.vAlign
        opacity: 0
        // Tint and blur render this item through an effect when requested.
        layer.enabled: root.hasTint || root.blurRadius > 0

        onStatusChanged: root.statusChanged2()
        onProgressChanged: {
            if (root.bridge && status === Image.Loading)
                root.bridge.imageProgress(progress);
        }

        Behavior on opacity {
            enabled: root.transitionMs > 0
            NumberAnimation {
                duration: root.transitionMs
                easing.type: Easing.InOutQuad
                onRunningChanged: {
                    if (!running && main.opacity >= 1)
                        root.reportDisplay();
                }
            }
        }
    }

    // Tint and blur need the Qt Quick Effects module, which not every Qt
    // build ships. When it is missing the image is drawn unmodified.
    property var effectItem: null
    property bool effectsMissing: false

    function updateEffect() {
        var wanted = hasTint || blurRadius > 0;
        if (!wanted) {
            if (effectItem) {
                effectItem.destroy();
                effectItem = null;
            }
            main.layer.enabled = false;
            main.visible = true;
            return;
        }
        if (effectsMissing)
            return;
        if (!effectItem) {
            try {
                effectItem = Qt.createQmlObject(
                    'import QtQuick\nimport QtQuick.Effects\n'
                    + 'MultiEffect { anchors.fill: parent; blurEnabled: false; autoPaddingEnabled: false }',
                    root, "rnqnx-expo-image-effect");
            } catch (e) {
                effectsMissing = true;
                console.warn("expo-image: tintColor and blurRadius need QtQuick.Effects (" + e + ")");
                main.layer.enabled = false;
                return;
            }
            effectItem.source = main;
        }
        main.visible = false;
        effectItem.opacity = Qt.binding(function() { return main.opacity; });
        effectItem.colorization = hasTint ? 1.0 : 0.0;
        effectItem.colorizationColor = tintColor;
        effectItem.blurEnabled = blurRadius > 0;
        effectItem.blurMax = 64;
        effectItem.blur = Math.min(1.0, blurRadius / 64.0);
    }

    onHasTintChanged: updateEffect()
    onTintColorChanged: updateEffect()
    onBlurRadiusChanged: updateEffect()

    function reportDisplay() {
        if (displayed || !bridge)
            return;
        displayed = true;
        bridge.imageDisplayed();
    }

    function statusChanged2() {
        var status = main.status;
        if (status === lastStatus)
            return;
        lastStatus = status;
        if (status === Image.Loading) {
            displayed = false;
            main.opacity = 0;
            if (bridge)
                bridge.imageLoadStarted();
        } else if (status === Image.Ready) {
            if (bridge)
                bridge.imageLoaded(sourceUrl, main.implicitWidth, main.implicitHeight, reportedCacheType);
            main.opacity = 1;
            if (transitionMs <= 0)
                reportDisplay();
        } else if (status === Image.Error) {
            main.opacity = 0;
            if (bridge)
                bridge.imageFailed("Could not load " + sourceUrl);
        } else if (status === Image.Null) {
            main.opacity = 0;
            displayed = false;
        }
    }

    onSourceUrlChanged: lastStatus = -1
}
