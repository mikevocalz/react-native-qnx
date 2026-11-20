## Props

`TouchableWithoutFeedback` has no QNX override and no direct test. `docs/component-support.json` records host `unverified` with the note "Direct component coverage is unverified." Every row below is therefore unverified.

| Prop                                                | Type     | Status     | Source-backed note                                                                     |
| --------------------------------------------------- | -------- | ---------- | -------------------------------------------------------------------------------------- |
| `onPress`, `onPressIn`, `onPressOut`, `onLongPress` | function | unverified | No direct evidence found. Responder dispatch is covered only through `Pressable`.      |
| `disabled`                                          | boolean  | unverified | No evidence found.                                                                     |
| `delayPressIn`, `delayPressOut`, `delayLongPress`   | number   | unverified | No evidence found.                                                                     |
| `hitSlop`, `pressRetentionOffset`                   | object   | unverified | No evidence found.                                                                     |
| `children`                                          | element  | unverified | React Native requires a single child that accepts responder props; not checked on QNX. |

## Events

| Event                         | Status     | Note                                              |
| ----------------------------- | ---------- | ------------------------------------------------- |
| `onPress` and press lifecycle | unverified | Relies on the same touch dispatch as `Pressable`. |

## Platform behavior on QNX

The component adds responder handlers to its single child and gives no visual feedback of its own. Touch events originate in `QtTouchDispatcher` (`vnext/src/fabric/QtTouchDispatcher.cpp`), which hit-tests the QML tree and emits Fabric touch events. A view whose `pointerEvents` is none or box-only is excluded from hit tests by the input shield, so a child with those settings cannot receive presses. No test exercises this component on QNX.

## Unsupported or partial props

None is documented as unsupported in source or receipts. Support is unconfirmed rather than ruled out.

## Minimal example

```tsx
import { Text, TouchableWithoutFeedback, View } from 'react-native';

export default function Example() {
  return (
    <TouchableWithoutFeedback onPress={() => console.log('tap')}>
      <View style={{ padding: 12 }}>
        <Text>Tap target, no feedback</Text>
      </View>
    </TouchableWithoutFeedback>
  );
}
```

## Known gaps

- Host, Pi 400 and Pi 4 QSTI are all unverified. No host test, device receipt or physical HID run exists for this component.
- The keyboard-dismiss use case (wrapping a screen to dismiss a keyboard) has no meaning here as no Keyboard module is wired (see `KeyboardAvoidingView`).
- Use `Pressable` if you need documented behavior.
