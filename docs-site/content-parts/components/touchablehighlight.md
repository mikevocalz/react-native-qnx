## Props

`TouchableHighlight` has no QNX override and no direct test. `docs/component-support.json` records host status `unverified` and the note "Direct component coverage is unverified." The table lists React Native's props with the status the evidence supports, which is unverified throughout.

| Prop                                                | Type     | Status     | Source-backed note                                                                |
| --------------------------------------------------- | -------- | ---------- | --------------------------------------------------------------------------------- |
| `onPress`, `onPressIn`, `onPressOut`, `onLongPress` | function | unverified | No direct evidence found. Responder dispatch is covered only through `Pressable`. |
| `underlayColor`                                     | color    | unverified | No evidence found.                                                                |
| `activeOpacity`                                     | number   | unverified | No evidence found.                                                                |
| `onShowUnderlay`, `onHideUnderlay`                  | function | unverified | No evidence found.                                                                |
| `disabled`, `style`, `children`                     | various  | unverified | No evidence found.                                                                |

## Events

| Event                              | Status     | Note                                                      |
| ---------------------------------- | ---------- | --------------------------------------------------------- |
| `onPress` and press lifecycle      | unverified | Depends on the same touch dispatch that `Pressable` uses. |
| `onShowUnderlay`, `onHideUnderlay` | unverified | No evidence found.                                        |

## Platform behavior on QNX

Touches are produced by `QtTouchDispatcher` (`vnext/src/fabric/QtTouchDispatcher.cpp`) and consumed by React Native's JS responder system. React Native's highlight behavior restyles the child view when pressed. Whether that restyle reaches the Qt item through the mount layer on QNX has not been tested; No evidence found for it.

## Unsupported or partial props

No prop is documented as unsupported by source or receipts. All are unverified because the component has no test. Do not read this page as support.

## Minimal example

```tsx
import { Text, TouchableHighlight } from 'react-native';

export default function Example() {
  return (
    <TouchableHighlight
      underlayColor="#99a"
      onPress={() => console.log('tap')}
      style={{ padding: 12 }}
    >
      <Text>Tap me</Text>
    </TouchableHighlight>
  );
}
```

## Known gaps

- Host, Pi 400 and Pi 4 QSTI are all unverified. Evidence is missing at every level: no host test, no Pi 400 receipt, no physical HID run.
- Prefer `Pressable`, which has host support and a scoped Pi 400 receipt (`docs/evidence/pressable-qnx-2026-11-07.md`).
- The underlay rendering path (style changes on press) has no recorded check.
