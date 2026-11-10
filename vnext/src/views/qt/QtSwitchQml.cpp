#include "react-native-qnx/views/QtSwitchQml.h"

namespace rnqnx::views {

const char* switchQml() {
  // Yoga owns the control's dimensions. The root must not paint behind the
  // rounded track, and the default off track must contrast with the thumb.
  return R"SWITCH(import QtQuick
Item {
  id: root
  property bool value: false
  property bool disabled: false
  property color trackOnColor: '#4cd964'
  property color trackOffColor: '#767577'
  property color thumbColor: '#ffffff'
  signal toggled()
  function toggle() {
    if (disabled) return;
    value = !value;
    toggled();
  }
  visible: true

  Rectangle {
    id: track
    objectName: "qnx-switch-track"
    anchors.fill: parent
    radius: parent.height / 2
    color: root.value ? root.trackOnColor : root.trackOffColor
  }
  Rectangle {
    id: thumb
    objectName: "qnx-switch-thumb"
    width: parent.height - 4
    height: width
    radius: width / 2
    color: root.thumbColor
    anchors.verticalCenter: parent.verticalCenter
    x: root.value ? parent.width - width - 2 : 2
  }
  MouseArea {
    anchors.fill: parent
    enabled: !root.disabled
    onClicked: root.toggle()
  }
}
)SWITCH";
}

}  // namespace rnqnx::views
