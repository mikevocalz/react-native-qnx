#include "react-native-qnx/views/QtTextInputQml.h"

namespace rnqnx::views {
const char* textInputQml() {
#if RNQNX_ENABLE_TEXT_INPUT_CONTRACT
  return R"TEXTINPUT_ON(import QtQuick
FocusScope {
  id: root
  visible: true
  // A single-line TextInput clips its text to its box on iOS and Android;
  // without this a long value scrolled to the cursor spilled out over
  // neighbouring views. Multi-line fields keep drawing past their box until
  // TextEdit scrolls (it cannot yet), so typed lines never vanish.
  clip: !multiline

  // View style (backgroundColor, borderRadius, border) and Yoga's content
  // insets (padding + border), set by the mounting layer.
  property color backgroundColor: "transparent"
  property real radius: 0
  property real borderWidth: 0
  property color borderColor: "transparent"
  property real paddingLeft: 0
  property real paddingTop: 0
  property real paddingRight: 0
  property real paddingBottom: 0

  Rectangle {
    anchors.fill: parent
    z: -2
    color: root.backgroundColor
    radius: root.radius
    border.width: root.borderWidth
    border.color: root.borderColor
  }

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
    anchors.leftMargin: root.paddingLeft
    anchors.topMargin: root.paddingTop
    anchors.rightMargin: root.paddingRight
    anchors.bottomMargin: root.paddingBottom
    verticalAlignment: TextInput.AlignVCenter
    // Follow the cursor while editing; show the start of the value otherwise,
    // as iOS and Android do.
    autoScroll: activeFocus
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
    anchors.leftMargin: root.paddingLeft
    anchors.topMargin: root.paddingTop
    anchors.rightMargin: root.paddingRight
    anchors.bottomMargin: root.paddingBottom
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
    anchors.leftMargin: root.paddingLeft
    anchors.topMargin: root.paddingTop
    anchors.rightMargin: root.paddingRight
    anchors.bottomMargin: root.paddingBottom
    verticalAlignment: root.multiline ? Text.AlignTop : Text.AlignVCenter
    elide: Text.ElideRight
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
  // A single-line TextInput clips its text to its box on iOS and Android;
  // without this a long value scrolled to the cursor spilled out over
  // neighbouring views. Multi-line fields keep drawing past their box until
  // TextEdit scrolls (it cannot yet), so typed lines never vanish.
  clip: !multiline

  // View style (backgroundColor, borderRadius, border) and Yoga's content
  // insets (padding + border), set by the mounting layer.
  property color backgroundColor: "transparent"
  property real radius: 0
  property real borderWidth: 0
  property color borderColor: "transparent"
  property real paddingLeft: 0
  property real paddingTop: 0
  property real paddingRight: 0
  property real paddingBottom: 0

  Rectangle {
    anchors.fill: parent
    z: -2
    color: root.backgroundColor
    radius: root.radius
    border.width: root.borderWidth
    border.color: root.borderColor
  }

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
    anchors.leftMargin: root.paddingLeft
    anchors.topMargin: root.paddingTop
    anchors.rightMargin: root.paddingRight
    anchors.bottomMargin: root.paddingBottom
    verticalAlignment: TextInput.AlignVCenter
    // Follow the cursor while editing; show the start of the value otherwise,
    // as iOS and Android do.
    autoScroll: activeFocus
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
    anchors.leftMargin: root.paddingLeft
    anchors.topMargin: root.paddingTop
    anchors.rightMargin: root.paddingRight
    anchors.bottomMargin: root.paddingBottom
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
    anchors.leftMargin: root.paddingLeft
    anchors.topMargin: root.paddingTop
    anchors.rightMargin: root.paddingRight
    anchors.bottomMargin: root.paddingBottom
    verticalAlignment: root.multiline ? Text.AlignTop : Text.AlignVCenter
    elide: Text.ElideRight
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
