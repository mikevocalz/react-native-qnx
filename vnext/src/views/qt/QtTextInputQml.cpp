#include "react-native-qnx/views/QtTextInputQml.h"

namespace rnqnx::views {
const char* textInputQml() {
#if RNQNX_ENABLE_TEXT_INPUT_CONTRACT
  return R"TEXTINPUT_ON(import QtQuick
FocusScope {
  id: root
  visible: true
  clip: false

  property color backgroundColor: "transparent"
  property color borderColor: "transparent"
  property real borderWidth: 0
  property real radius: 0
  property real topLeftRadius: 0
  property real topRightRadius: 0
  property real bottomRightRadius: 0
  property real bottomLeftRadius: 0
  property bool usePerCornerRadius: false
  property real contentInsetLeft: 0
  property real contentInsetTop: 0
  property real contentInsetRight: 0
  property real contentInsetBottom: 0

  property bool contractEnabled: false
  property string text: ""
  property string placeholderText: ""
  property color placeholderTextColor: "#999999"
  property bool readOnly: false
  property bool multiline: false
  property int echoMode: TextInput.Normal
  property int maximumLength: 0
  property font font
  property color color

  signal submitEditing()
  signal keyPressed()
  signal selectionChanged()
  property string lastKeyText: ""
  property int selectionStart: 0
  property int selectionEnd: 0

  function mapKeyText(event) {
    if (event.key === Qt.Key_Backspace) return "";
    if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) return "\n";
    if (event.key === Qt.Key_Tab || event.key === Qt.Key_Backtab) return "\t";
    if (event.key === Qt.Key_Escape) return "\x1B";
    return event.text;
  }

  Rectangle {
    objectName: "qnx-text-input-background"
    anchors.fill: parent
    color: root.backgroundColor
    border.width: root.borderWidth
    border.color: root.borderColor
    radius: root.radius
    topLeftRadius: root.usePerCornerRadius ? root.topLeftRadius : root.radius
    topRightRadius: root.usePerCornerRadius ? root.topRightRadius : root.radius
    bottomRightRadius: root.usePerCornerRadius ? root.bottomRightRadius : root.radius
    bottomLeftRadius: root.usePerCornerRadius ? root.bottomLeftRadius : root.radius
    z: -2
  }

  TextInput {
    id: input
    objectName: "qnx-text-input"
    anchors.fill: parent
    anchors.leftMargin: root.contentInsetLeft
    anchors.topMargin: root.contentInsetTop
    anchors.rightMargin: root.contentInsetRight
    anchors.bottomMargin: root.contentInsetBottom
    clip: true
    visible: !root.multiline
    focus: !root.multiline
    readOnly: root.readOnly
    echoMode: root.echoMode
    maximumLength: root.maximumLength > 0 ? root.maximumLength : 32767
    font: root.font
    color: root.color
    onTextChanged: {
      if (root.text !== input.text) {
        root.text = input.text;
      }
    }
    onAccepted: { root.submitEditing(); }
    onCursorPositionChanged: {
      root.selectionStart = input.selectionStart;
      root.selectionEnd = input.selectionEnd;
      root.selectionChanged();
    }
    onSelectionStartChanged: {
      if (root.contractEnabled) {
        root.selectionStart = input.selectionStart;
        root.selectionEnd = input.selectionEnd;
        root.selectionChanged();
      }
    }
    onSelectionEndChanged: {
      if (root.contractEnabled) {
        root.selectionStart = input.selectionStart;
        root.selectionEnd = input.selectionEnd;
        root.selectionChanged();
      }
    }
    Keys.onPressed: {
      root.lastKeyText = root.mapKeyText(event);
      root.keyPressed();
    }
  }

  TextEdit {
    id: edit
    objectName: "qnx-text-edit"
    anchors.fill: parent
    anchors.leftMargin: root.contentInsetLeft
    anchors.topMargin: root.contentInsetTop
    anchors.rightMargin: root.contentInsetRight
    anchors.bottomMargin: root.contentInsetBottom
    clip: true
    visible: root.multiline
    focus: root.multiline
    readOnly: root.readOnly
    font: root.font
    color: root.color
    wrapMode: TextEdit.WordWrap
    onTextChanged: {
      if (root.text !== edit.text) {
        root.text = edit.text;
      }
    }
    onCursorPositionChanged: {
      root.selectionStart = edit.selectionStart;
      root.selectionEnd = edit.selectionEnd;
      root.selectionChanged();
    }
    onSelectionStartChanged: {
      if (root.contractEnabled) {
        root.selectionStart = edit.selectionStart;
        root.selectionEnd = edit.selectionEnd;
        root.selectionChanged();
      }
    }
    onSelectionEndChanged: {
      if (root.contractEnabled) {
        root.selectionStart = edit.selectionStart;
        root.selectionEnd = edit.selectionEnd;
        root.selectionChanged();
      }
    }
    Keys.onPressed: {
      root.lastKeyText = root.mapKeyText(event);
      root.keyPressed();
    }
  }

  Text {
    id: placeholder
    objectName: "qnx-text-input-placeholder"
    anchors.fill: parent
    anchors.leftMargin: root.contentInsetLeft
    anchors.topMargin: root.contentInsetTop
    anchors.rightMargin: root.contentInsetRight
    anchors.bottomMargin: root.contentInsetBottom
    clip: true
    text: root.placeholderText
    color: root.placeholderTextColor
    font: root.font
    visible: root.text === "" && root.placeholderText !== ""
    z: -1
  }
}
)TEXTINPUT_ON";
#else
  // Keep the legacy input event contract while sharing the same field surface
  // and Yoga content bounds as the opt-in event/selection path.
  return R"TEXTINPUT_OFF(import QtQuick
FocusScope {
  id: root
  visible: true
  clip: false

  property color backgroundColor: "transparent"
  property color borderColor: "transparent"
  property real borderWidth: 0
  property real radius: 0
  property real topLeftRadius: 0
  property real topRightRadius: 0
  property real bottomRightRadius: 0
  property real bottomLeftRadius: 0
  property bool usePerCornerRadius: false
  property real contentInsetLeft: 0
  property real contentInsetTop: 0
  property real contentInsetRight: 0
  property real contentInsetBottom: 0

  property string text: ""
  property string placeholderText: ""
  property color placeholderTextColor: "#999999"
  property bool readOnly: false
  property bool multiline: false
  property int echoMode: TextInput.Normal
  property int maximumLength: 0
  property font font
  property color color

  signal submitEditing()
  signal keyPressed()
  signal selectionChanged()
  property string lastKeyText: ""
  property int selectionStart: 0
  property int selectionEnd: 0

  function mapKeyText(event) {
    if (event.key === Qt.Key_Backspace) return "";
    if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) return "\n";
    if (event.key === Qt.Key_Tab) return "\t";
    if (event.key === Qt.Key_Escape) return "\x1B";
    return event.text;
  }

  Rectangle {
    objectName: "qnx-text-input-background"
    anchors.fill: parent
    color: root.backgroundColor
    border.width: root.borderWidth
    border.color: root.borderColor
    radius: root.radius
    topLeftRadius: root.usePerCornerRadius ? root.topLeftRadius : root.radius
    topRightRadius: root.usePerCornerRadius ? root.topRightRadius : root.radius
    bottomRightRadius: root.usePerCornerRadius ? root.bottomRightRadius : root.radius
    bottomLeftRadius: root.usePerCornerRadius ? root.bottomLeftRadius : root.radius
    z: -2
  }

  TextInput {
    id: input
    objectName: "qnx-text-input"
    anchors.fill: parent
    anchors.leftMargin: root.contentInsetLeft
    anchors.topMargin: root.contentInsetTop
    anchors.rightMargin: root.contentInsetRight
    anchors.bottomMargin: root.contentInsetBottom
    clip: true
    visible: !root.multiline
    focus: !root.multiline
    readOnly: root.readOnly
    echoMode: root.echoMode
    maximumLength: root.maximumLength > 0 ? root.maximumLength : 32767
    font: root.font
    color: root.color
    onTextChanged: {
      if (root.text !== input.text) {
        root.text = input.text;
      }
    }
    onAccepted: { root.submitEditing(); }
    onCursorPositionChanged: {
      root.selectionStart = input.selectionStart;
      root.selectionEnd = input.selectionEnd;
      root.selectionChanged();
    }
    Keys.onPressed: {
      root.lastKeyText = root.mapKeyText(event);
      root.keyPressed();
    }
  }

  TextEdit {
    id: edit
    objectName: "qnx-text-edit"
    anchors.fill: parent
    anchors.leftMargin: root.contentInsetLeft
    anchors.topMargin: root.contentInsetTop
    anchors.rightMargin: root.contentInsetRight
    anchors.bottomMargin: root.contentInsetBottom
    clip: true
    visible: root.multiline
    focus: root.multiline
    readOnly: root.readOnly
    font: root.font
    color: root.color
    wrapMode: TextEdit.WordWrap
    onTextChanged: {
      if (root.text !== edit.text) {
        root.text = edit.text;
      }
    }
    onCursorPositionChanged: {
      root.selectionStart = edit.selectionStart;
      root.selectionEnd = edit.selectionEnd;
      root.selectionChanged();
    }
    Keys.onPressed: {
      root.lastKeyText = root.mapKeyText(event);
      root.keyPressed();
    }
  }

  Text {
    id: placeholder
    objectName: "qnx-text-input-placeholder"
    anchors.fill: parent
    anchors.leftMargin: root.contentInsetLeft
    anchors.topMargin: root.contentInsetTop
    anchors.rightMargin: root.contentInsetRight
    anchors.bottomMargin: root.contentInsetBottom
    clip: true
    text: root.placeholderText
    color: root.placeholderTextColor
    font: root.font
    visible: root.text === "" && root.placeholderText !== ""
    z: -1
  }
}
)TEXTINPUT_OFF";
#endif
}
}
