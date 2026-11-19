#include "react-native-qnx/views/QtModalQml.h"

namespace rnqnx::views {

const char* modalQml() {
  return R"MODAL(import QtQuick
Item {
  id: root
  objectName: "rnqnxModal"
  visible: true
  clip: true
  property color backgroundColor: "transparent"
  property real radius: 0

  // Escape is the QNX counterpart of Android's hardware back button. The
  // handler accepts the key so it stops at the modal and never reaches the
  // covered app.
  signal requestClose()
  Keys.onEscapePressed: function(event) {
    event.accepted = true
    root.requestClose()
  }

  Rectangle {
    anchors.fill: parent
    z: -1
    color: root.backgroundColor
    radius: root.radius
  }
}
)MODAL";
}

}  // namespace rnqnx::views
