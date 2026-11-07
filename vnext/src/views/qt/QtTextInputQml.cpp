#include "react-native-qnx/views/QtTextInputQml.h"

namespace rnqnx::views {
const char* textInputQml() {
#if RNQNX_ENABLE_TEXT_INPUT_CONTRACT
  return R"TEXTINPUT_ON(import QtQuick
FocusScope {
  id: root
  visible: true
  clip: false

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

  TextInput {
    id: input
    objectName: "qnx-text-input"
    anchors.fill: parent
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
    anchors.fill: parent
    text: root.placeholderText
    color: root.placeholderTextColor
    font: root.font
    visible: root.text === "" && root.placeholderText !== ""
    z: -1
  }
}
)TEXTINPUT_ON";
#else
  // Preserve the pre-experiment component byte-for-byte on the OFF path.
  return R"TEXTINPUT_OFF(import QtQuick
FocusScope {
  id: root
  visible: true
  clip: false

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

  TextInput {
    id: input
    objectName: "qnx-text-input"
    anchors.fill: parent
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
    anchors.fill: parent
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
