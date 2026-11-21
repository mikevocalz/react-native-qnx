import QtQuick

// The node the smoke test waits for is created only after a Loader activates,
// which a later event does. The test never learns when that happens.
Item {
    id: root
    objectName: "fixtureRoot"
    width: 200
    height: 100
    property string nodeName: "fixtureReady"

    Timer {
        interval: 150
        running: true
        onTriggered: loader.active = true
    }

    Loader {
        id: loader
        active: false
        sourceComponent: Rectangle {
            objectName: root.nodeName
            width: 10
            height: 10
            color: "green"
        }
    }
}
