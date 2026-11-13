import QtQuick
import QtQuick.Shapes
import QtMultimedia

// Qt Quick side of the QtVideo component: a MediaPlayer feeding a VideoOutput
// that lives in the scene graph, plus an optional control bar drawn with
// plain Items (no Qt Quick Controls dependency). QtVideoItem sets the
// properties below and listens to the signals; nothing here reads React state.
Item {
    id: root
    clip: true

    property url source
    property bool paused: false
    property bool muted: false
    property real volume: 1.0
    property real rate: 1.0
    property bool repeat: false
    property int fillMode: VideoOutput.PreserveAspectFit
    property url poster
    property bool controls: false
    property bool fullscreen: false
    // Output device: an id or part of the description from QMediaDevices; empty
    // picks a USB device if one exists, else the first named card, else the platform default.
    property string audioDevice: ""
    readonly property var chosenDevice: pickDevice(devices.audioOutputs, audioDevice)
    readonly property string audioDeviceName: chosenDevice ? chosenDevice.description : ""
    property bool hasFrame: false
    property bool loadedOnce: false
    property real durationSeconds: 0
    property real seekTarget: -1
    property color barColor: "#cc0c1929"
    property color accent: "#79e8dc"
    readonly property real barHeight: 40

    signal loaded(real duration, int videoWidth, int videoHeight, string audioDevice)
    signal progress(real currentTime, real playableDuration)
    signal ended()
    signal failed(string message, int code)
    signal buffering(bool isBuffering)
    signal seeked(real currentTime, real seekTime)
    signal fullscreenToggled(bool fullscreen)

    function seekTo(seconds) {
        if (!(seconds >= 0)) return
        seekTarget = seconds
        player.position = Math.round(seconds * 1000)
        seekReport.restart()
    }
    function play() { player.play() }
    function pause() { player.pause() }
    function applyPaused() {
        if (player.mediaStatus === MediaPlayer.NoMedia
            || player.mediaStatus === MediaPlayer.InvalidMedia
            || player.mediaStatus === MediaPlayer.LoadingMedia) return
        if (paused) player.pause(); else player.play()
    }
    function pickDevice(list, wanted) {
        var key = String(wanted).toLowerCase()
        for (var i = 0; i < list.length; ++i) {
            var d = list[i]
            if (key.length > 0 && (String(d.id) === wanted || d.description.toLowerCase().indexOf(key) >= 0))
                return d
        }
        if (key.length === 0) {
            for (var j = 0; j < list.length; ++j)
                if (list[j].description.toLowerCase().indexOf("usb") >= 0) return list[j]
            // No USB device: name the first real card (plughw converts rate and
            // format) instead of the anonymous "default" route.
            for (var k = 0; k < list.length; ++k)
                if (String(list[k].id).indexOf("plughw:") === 0) return list[k]
        }
        return devices.defaultAudioOutput
    }
    function fmt(ms) {
        var s = Math.max(0, Math.floor(ms / 1000))
        var m = Math.floor(s / 60)
        var r = s % 60
        return m + ":" + (r < 10 ? "0" : "") + r
    }

    onPausedChanged: applyPaused()
    onSourceChanged: { hasFrame = false; loadedOnce = false; durationSeconds = 0 }

    MediaDevices { id: devices }

    MediaPlayer {
        id: player
        source: root.source
        videoOutput: videoOut
        audioOutput: audioOut
        loops: root.repeat ? MediaPlayer.Infinite : 1
        playbackRate: root.rate
        onErrorOccurred: function (error, errorString) {
            root.failed(errorString.length ? errorString : "Playback error", error)
        }
        onMediaStatusChanged: {
            switch (mediaStatus) {
            case MediaPlayer.LoadedMedia:
            case MediaPlayer.BufferedMedia:
                if (!root.loadedOnce) {
                    root.loadedOnce = true
                    root.durationSeconds = duration / 1000
                    var res = metaData.value(MediaMetaData.Resolution)
                    root.loaded(duration / 1000, res ? res.width : 0, res ? res.height : 0, root.audioDeviceName)
                }
                root.buffering(false)
                root.applyPaused()
                break
            case MediaPlayer.StalledMedia:
            case MediaPlayer.BufferingMedia:
                root.buffering(true)
                break
            case MediaPlayer.EndOfMedia:
                root.ended()
                break
            case MediaPlayer.InvalidMedia:
                // errorOccurred carries the message; nothing to add here.
                break
            }
        }
    }

    AudioOutput {
        id: audioOut
        device: root.chosenDevice
        volume: root.volume
        muted: root.muted
    }

    VideoOutput {
        id: videoOut
        anchors.fill: parent
        fillMode: root.fillMode
        Connections {
            target: videoOut.videoSink
            function onVideoFrameChanged() { root.hasFrame = true }
        }
    }

    Image {
        anchors.fill: parent
        source: root.poster
        visible: root.poster.toString().length > 0 && !root.hasFrame
        fillMode: root.fillMode === VideoOutput.PreserveAspectCrop ? Image.PreserveAspectCrop
                : root.fillMode === VideoOutput.Stretch ? Image.Stretch : Image.PreserveAspectFit
        asynchronous: true
    }

    Timer {
        interval: 250
        repeat: true
        running: player.playbackState === MediaPlayer.PlayingState
        onTriggered: root.progress(player.position / 1000, Math.min(player.duration, player.position + player.duration * player.bufferProgress) / 1000)
    }

    // The seek event goes out once the player has moved, or after a short
    // wait when the position did not change (seeking to the current spot).
    Timer {
        id: seekReport
        interval: 120
        onTriggered: {
            root.seeked(player.position / 1000, root.seekTarget)
            root.seekTarget = -1
        }
    }

    Item {
        id: bar
        visible: root.controls
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: root.barHeight

        Rectangle { anchors.fill: parent; color: root.barColor }

        // Play / pause
        Item {
            id: playButton
            objectName: "videoPlayPause"
            width: 40; height: parent.height
            readonly property bool playing: player.playbackState === MediaPlayer.PlayingState
            Shape {
                anchors.centerIn: parent
                width: 14; height: 16
                visible: !playButton.playing
                ShapePath {
                    fillColor: "white"; strokeColor: "transparent"
                    startX: 1; startY: 0
                    PathLine { x: 14; y: 8 }
                    PathLine { x: 1; y: 16 }
                    PathLine { x: 1; y: 0 }
                }
            }
            Row {
                anchors.centerIn: parent
                spacing: 4
                visible: playButton.playing
                Rectangle { width: 4; height: 16; color: "white" }
                Rectangle { width: 4; height: 16; color: "white" }
            }
            MouseArea {
                anchors.fill: parent
                onClicked: {
                    if (playButton.playing) {
                        player.pause()
                    } else {
                        if (player.mediaStatus === MediaPlayer.EndOfMedia) player.position = 0
                        player.play()
                    }
                }
            }
        }

        Text {
            id: elapsed
            anchors.left: playButton.right
            anchors.verticalCenter: parent.verticalCenter
            width: 38
            text: root.fmt(player.position)
            color: "white"
            font.pixelSize: 12
        }

        // Fullscreen
        Item {
            id: fsButton
            objectName: "videoFullscreen"
            anchors.right: parent.right
            width: 40; height: parent.height
            Rectangle {
                anchors.centerIn: parent
                width: 16; height: 12
                color: "transparent"
                border.color: root.fullscreen ? root.accent : "white"
                border.width: 2
            }
            MouseArea {
                anchors.fill: parent
                onClicked: root.fullscreenToggled(!root.fullscreen)
            }
        }

        // Volume (click the speaker to mute, click the track to set level)
        Item {
            id: volumeBox
            anchors.right: fsButton.left
            width: 96; height: parent.height
            Item {
                id: speaker
                objectName: "videoMute"
                width: 28; height: parent.height
                Rectangle {
                    x: 6; anchors.verticalCenter: parent.verticalCenter
                    width: 6; height: 8
                    color: root.muted ? "#ff6b6b" : "white"
                }
                Rectangle {
                    x: 12; anchors.verticalCenter: parent.verticalCenter
                    width: 6; height: 14
                    color: root.muted ? "#ff6b6b" : "white"
                    radius: 2
                }
                MouseArea { anchors.fill: parent; onClicked: root.muted = !root.muted }
            }
            Rectangle {
                id: volTrack
                objectName: "videoVolume"
                anchors.left: speaker.right
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width - speaker.width - 8
                height: 4; radius: 2
                color: "#44ffffff"
                Rectangle {
                    width: parent.width * (root.muted ? 0 : root.volume)
                    height: parent.height; radius: 2
                    color: root.accent
                }
                MouseArea {
                    anchors.fill: parent
                    anchors.topMargin: -10; anchors.bottomMargin: -10
                    onPressed: function (m) { root.volume = Math.max(0, Math.min(1, m.x / width)); root.muted = false }
                    onPositionChanged: function (m) { if (pressed) { root.volume = Math.max(0, Math.min(1, m.x / width)); root.muted = false } }
                }
            }
        }

        Text {
            id: total
            anchors.right: volumeBox.left
            anchors.verticalCenter: parent.verticalCenter
            width: 38
            horizontalAlignment: Text.AlignRight
            text: root.fmt(player.duration)
            color: "#c8d6e5"
            font.pixelSize: 12
        }

        // Scrubber
        Item {
            id: scrub
            objectName: "videoScrubber"
            anchors.left: elapsed.right
            anchors.right: total.left
            anchors.leftMargin: 6; anchors.rightMargin: 6
            height: parent.height
            Rectangle {
                id: scrubTrack
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width; height: 4; radius: 2
                color: "#44ffffff"
                Rectangle {
                    width: player.duration > 0 ? parent.width * Math.min(1, player.position / player.duration) : 0
                    height: parent.height; radius: 2
                    color: root.accent
                }
            }
            Rectangle {
                width: 12; height: 12; radius: 6
                color: "white"
                anchors.verticalCenter: parent.verticalCenter
                x: (player.duration > 0 ? scrubTrack.width * Math.min(1, player.position / player.duration) : 0) - 6
            }
            MouseArea {
                anchors.fill: parent
                function seekFrom(mx) {
                    if (player.duration > 0)
                        root.seekTo(Math.max(0, Math.min(1, mx / width)) * player.duration / 1000)
                }
                onPressed: function (m) { seekFrom(m.x) }
                onPositionChanged: function (m) { if (pressed) seekFrom(m.x) }
            }
        }
    }
}
