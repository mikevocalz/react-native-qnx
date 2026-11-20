## Props

`Pressable` has no QNX override: `packages/react-native-qnx/Libraries/Components` contains only KeyboardAvoidingView, RefreshControl, StatusBar, Switch and TextInput. The component is React Native's own (RN 0.88.1 per the evidence receipt), running on the JS responder system. Props are therefore React Native's; the table lists those the evidence exercises.

| Prop                                                | Type     | Status     | Source-backed note                                            |
| --------------------------------------------------- | -------- | ---------- | ------------------------------------------------------------- |
| `onPress`, `onPressIn`, `onPressOut`                | function | supported  | Exercised by the Pressable fixture (tap, cancel, deactivate). |
| `onLongPress`                                       | function | unverified | No evidence found.                                            |
| `disabled`                                          | boolean  | supported  | The fixture covers the disabled path.                         |
| `style`, `children`                                 | node     | supported  | Renders as a View.                                            |
| `hitSlop`, `pressRetentionOffset`, `android_ripple` | various  | unverified | No evidence found in QNX sources or receipts.                 |

## Events

| Event                     | Status    | Note                                                                                                     |
| ------------------------- | --------- | -------------------------------------------------------------------------------------------------------- |
| `onPress`                 | supported | Host pointer press delivers it (`docs/component-support.json`).                                          |
| `onPressIn`, `onPressOut` | supported | Fixture log: `RNQNX_PRESSABLE_OK tap cancel deactivate disabled unmount remount callbacks=11`.           |
| Touch cancel              | supported | Delivered on `QEvent::TouchCancel`, window deactivation and mouse ungrab when the input lifecycle is on. |

## Platform behavior on QNX

Touches reach React through `QtTouchDispatcher` (`vnext/src/fabric/QtTouchDispatcher.cpp`). It hit-tests the QML item tree and emits Fabric `onTouchStart`, `onTouchMove`, `onTouchEnd` and `onTouchCancel`. A single-pointer mouse path (left button only when the lifecycle option is on) keeps desktop and pointer-only devices working without synthesized touches. A claimed responder keeps the gesture for its remainder. Views with `pointerEvents` none or box-only are skipped by the input shield.

The input lifecycle is behind a default-OFF CMake option. The remount fix needs a three-file RN patch that forwards null-target `topTouchCancel` when `__RNQNX_INPUT_LIFECYCLE__` is strictly true. Without it, a press pending during `stopSurface` leaves responder state alive and the next `onPressIn` never runs (receipt `docs/evidence/pressable-qnx-2026-11-07.md`).

## Unsupported or partial props

- `android_ripple`: Android-only in React Native; no QNX source reads it.
- Gesture handler integration: the receipt states it does not implement RNGH or Worklets.
- Other props: No evidence found either way.

## Minimal example

```tsx
import { useState } from 'react';
import { Pressable, Text } from 'react-native';

export default function Counter() {
  const [n, setN] = useState(0);
  return (
    <Pressable onPress={() => setN(n + 1)} style={{ padding: 12, backgroundColor: '#ddd' }}>
      <Text>Pressed {n} times</Text>
    </Pressable>
  );
}
```

## Known gaps

- Pi 400 evidence uses synthetic Qt input. Physical HID delivery is unverified.
- RNGH and Worklets are unverified (`docs/reanimated-gesture-handler-qnx.md` covers their separate audit).
- The input lifecycle is default OFF; with it off, no evidence covers cancel and remount recovery.
- Pi 4 QSTI is untested.
