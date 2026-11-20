## Props

`TextInput` is overridden in `packages/react-native-qnx/Libraries/Components/TextInput/TextInput.js`. The wrapper renders React Native's single-line iOS native component and forwards every prop to it. Native code then reads a fixed set in `applyTextInput` (`vnext/src/fabric/ShadowViewMutationAdapter.cpp`).

| Prop                                                                                               | Type    | Status                  | Source-backed note                                                                                           |
| -------------------------------------------------------------------------------------------------- | ------- | ----------------------- | ------------------------------------------------------------------------------------------------------------ |
| `value`                                                                                            | string  | supported               | Controlled text; the wrapper sends it as `text`.                                                             |
| `defaultValue`                                                                                     | string  | supported               | Seeds the per-instance store.                                                                                |
| `editable`, `readOnly`                                                                             | boolean | supported               | Either one false or true sets native `readOnly`.                                                             |
| `secureTextEntry`                                                                                  | boolean | supported               | Sets Qt `echoMode` to Password (2).                                                                          |
| `maxLength`                                                                                        | number  | supported               | Sets Qt `maximumLength`.                                                                                     |
| `placeholder`                                                                                      | string  | supported               | Shown when text is empty.                                                                                    |
| `placeholderTextColor`                                                                             | color   | supported               | Applied when the color is meaningful.                                                                        |
| `multiline`                                                                                        | boolean | supported               | Switches to the `TextEdit` child.                                                                            |
| `style` (font size, family, color, padding)                                                        | style   | partial                 | Font size, family, foreground color and Yoga content insets are applied. Other text attributes are not read. |
| `selection`                                                                                        | object  | supported with contract | Applied only when `RNQNX_ENABLE_TEXT_INPUT_CONTRACT` is on.                                                  |
| `keyboardType`, `returnKeyType`, `autoCapitalize`, `autoCorrect`, `autoFocus`, `selectTextOnFocus` | various | accepted-no-effect      | No evidence found that native code reads them. They pass through to the native component untouched.          |

## Events

| Event                                             | Status             | Note                                                                   |
| ------------------------------------------------- | ------------------ | ---------------------------------------------------------------------- |
| `onChange`, `onChangeText`                        | supported          | Fired from the native change emitter; the wrapper tracks `eventCount`. |
| `onSelectionChange`                               | supported          | UTF-16 offsets.                                                        |
| `onFocus`, `onBlur`                               | supported          | Focus transitions are delivered after the outer JS update finishes.    |
| `onSubmitEditing`                                 | supported          | Wired to the QML `onAccepted` signal.                                  |
| `onKeyPress`                                      | supported          | QML `keyPressed` signal with mapped key text.                          |
| `onEndEditing`, `onContentSizeChange`, `onScroll` | accepted-no-effect | No evidence found that native code emits them.                         |

## Platform behavior on QNX

React Native's own `TextInput.js` branches on `Platform.OS` and has no `qnx` branch, so without this override nothing renders. The override uses the portable iOS single-line descriptor, which the QNX mount layer already handles. Editors are QML items from `vnext/src/views/qt/QtTextInputQml.cpp`: a `TextInput` for single line and a `TextEdit` for multiline.

With the default-OFF TextInput contract enabled, the Qt bridge (`QtTextInputBridge.cpp`) owns an edit token, rejects stale text and selection commands, and suppresses feedback from JS writes. Each editable wrapper then gets one Qt tab stop; Tab and Backtab both map to the RN Tab key value.

## Unsupported or partial props

- Secure multiline: with the contract on, `TextInput.js` throws `RNQNX_TEXT_INPUT_SECURE_MULTILINE_UNSUPPORTED`. The bridge also rejects secure input-method queries and commits (`QtTextInputBridge.cpp`).
- Software-keyboard props such as `keyboardType` and `returnKeyType`: no Keyboard native module is wired (see `KeyboardAvoidingView`), and no source reads them.

## Minimal example

```tsx
import { useState } from 'react';
import { TextInput, View } from 'react-native';

export default function Field() {
  const [text, setText] = useState('');
  return (
    <View style={{ padding: 16 }}>
      <TextInput
        value={text}
        onChangeText={setText}
        placeholder="Name"
        maxLength={32}
        style={{ fontSize: 18, height: 40 }}
      />
    </View>
  );
}
```

## Known gaps

- Pi 400 evidence uses synthetic Qt key and input-method events driven through real React (`docs/evidence/text-input-qnx-2026-11-07.md`, `docs/evidence/text-input-focus-qnx-2026-11-07.md`). That receipt states it does not establish complete TextInput or IME support.
- No evidence found for physical hardware keyboard (HID), a software keyboard, an installed IME provider, assistive technology output or clipboard privacy.
- The contract is default OFF and experimental.
- Pi 4 QSTI is untested.
