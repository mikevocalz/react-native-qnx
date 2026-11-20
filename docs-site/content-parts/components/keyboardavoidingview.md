## Props

`KeyboardAvoidingView` is overridden in `packages/react-native-qnx/Libraries/Components/KeyboardAvoidingView/KeyboardAvoidingView.js`. The file header says QNX targets have no software keyboard or `Keyboard` native module, so the component is a pass-through `View`.

| Prop                     | Type                                  | Status             | Source-backed note                                                                                        |
| ------------------------ | ------------------------------------- | ------------------ | --------------------------------------------------------------------------------------------------------- |
| `children`               | node                                  | supported          | Rendered inside a `View`.                                                                                 |
| `style`                  | style                                 | supported          | Passed to the `View`.                                                                                     |
| `behavior`               | `'height' \| 'position' \| 'padding'` | accepted-no-effect | Ignored.                                                                                                  |
| `keyboardVerticalOffset` | number                                | accepted-no-effect | Ignored.                                                                                                  |
| `enabled`                | boolean                               | accepted-no-effect | Ignored.                                                                                                  |
| `contentContainerStyle`  | style                                 | accepted-no-effect | Ignored, so `behavior="position"` has no inner container.                                                 |
| other `View` props       | various                               | unsupported        | The function reads only `children` and `style`; `testID`, `onLayout` and accessibility props are dropped. |

## Events

| Event      | Status      | Note                                                                      |
| ---------- | ----------- | ------------------------------------------------------------------------- |
| `onLayout` | unsupported | Not forwarded by the override (source reads only `children` and `style`). |

The component subscribes to no keyboard events and emits none.

## Platform behavior on QNX

The implementation is `React.createElement(View, { style }, children)`. Layout never changes when text input gains focus. The `Keyboard` module is not wired (`docs/component-support.json`: "no Keyboard module is wired"), so `Keyboard.addListener`, `Keyboard.dismiss` and frame events cannot drive layout from JS either. Hardware keyboards need no avoidance, and no on-screen keyboard exists in the evidence.

## Unsupported or partial props

- `behavior`, `keyboardVerticalOffset`, `enabled`, `contentContainerStyle`: accepted and ignored, per the header in `KeyboardAvoidingView.js`.
- Dropped `View` props: the override destructures only `children` and `style`, so every other prop is discarded. The status in the component matrix reads `no-op`.

## Minimal example

```tsx
import { KeyboardAvoidingView, TextInput } from 'react-native';

export default function Example() {
  return (
    <KeyboardAvoidingView behavior="padding" style={{ flex: 1 }}>
      <TextInput placeholder="Type here" style={{ height: 40 }} />
    </KeyboardAvoidingView>
  );
}
```

On QNX this lays out exactly as a plain `View` with `flex: 1`.

## Known gaps

- No Keyboard module exists, so avoidance cannot be implemented from JS. A Qt virtual keyboard or input-panel geometry hook would be needed; No evidence found that one is planned or present.
- Software keyboard geometry is explicitly unclaimed in `docs/evidence/text-input-qnx-2026-11-07.md`.
- Pi 400 and Pi 4 QSTI status is unverified or untested.
