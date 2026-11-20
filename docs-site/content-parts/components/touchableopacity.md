## Props

`TouchableOpacity` has no QNX override. It is React Native's component, which dims through an animated opacity value. The only claims backed here come from `docs/component-support.json`: it mounts through the responder system and uses NativeAnimated opacity, and responder coverage comes from `Pressable`.

| Prop                                                       | Type     | Status     | Source-backed note                                         |
| ---------------------------------------------------------- | -------- | ---------- | ---------------------------------------------------------- |
| `onPress`, `onPressIn`, `onPressOut`                       | function | supported  | Same responder path as `Pressable`.                        |
| `activeOpacity`                                            | number   | supported  | Drives the NativeAnimated opacity; no per-prop test found. |
| `disabled`                                                 | boolean  | unverified | No evidence found.                                         |
| `onLongPress`, `delayPressIn`, `delayLongPress`, `hitSlop` | various  | unverified | No evidence found.                                         |
| `style`, `children`                                        | node     | supported  | Renders a View.                                            |

## Events

| Event                     | Status    | Note                                                                                                  |
| ------------------------- | --------- | ----------------------------------------------------------------------------------------------------- |
| `onPress`                 | supported | Mounts and responds via the touch dispatcher.                                                         |
| `onPressIn`, `onPressOut` | supported | Inherits the responder evidence in `docs/evidence/pressable-qnx-2026-11-07.md`; no direct test found. |

## Platform behavior on QNX

Touch input comes from `QtTouchDispatcher` (`vnext/src/fabric/QtTouchDispatcher.cpp`), which maps Qt mouse and touch events to Fabric touch events and hit-tests the QML tree. React's JS responder logic decides when a press starts and ends. The dimming animation needs the NativeAnimated backend; if it is not connected, the opacity change does not reach the native item. No evidence found for behavior in that case.

## Unsupported or partial props

- Direct component coverage: the matrix lists no device evidence, so props are exercised only indirectly via `Pressable`.
- `hasTVPreferredFocus`, `nextFocus*`: No evidence found that QNX reads them.

## Minimal example

```tsx
import { Text, TouchableOpacity } from 'react-native';

export default function Example() {
  return (
    <TouchableOpacity
      activeOpacity={0.6}
      onPress={() => console.log('tap')}
      style={{ padding: 12, backgroundColor: '#ccd' }}
    >
      <Text>Tap me</Text>
    </TouchableOpacity>
  );
}
```

## Known gaps

- Pi 400 status is unverified; no device evidence is attached.
- Responder coverage is inherited from `Pressable`, not tested on this component.
- Physical HID is unverified.
- Pi 4 QSTI is untested.
