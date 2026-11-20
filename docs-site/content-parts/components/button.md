## Props

`Button` has no QNX override. It is React Native's component, built on its touchable and `Text` primitives. Evidence for it is limited to the host note in `docs/component-support.json`: it mounts, reports presses, and its opacity press animation reaches the connected NativeAnimated backend.

| Prop                                                                        | Type     | Status     | Source-backed note                |
| --------------------------------------------------------------------------- | -------- | ---------- | --------------------------------- |
| `title`                                                                     | string   | supported  | Mounting is host-tested.          |
| `onPress`                                                                   | function | supported  | Host presses are reported.        |
| `color`                                                                     | color    | unverified | No evidence found.                |
| `disabled`                                                                  | boolean  | unverified | No evidence found.                |
| `accessibilityLabel`, `testID`, `hasTVPreferredFocus`, `touchSoundDisabled` | various  | unverified | No evidence found in QNX sources. |

## Events

| Event     | Status    | Note                                                                   |
| --------- | --------- | ---------------------------------------------------------------------- |
| `onPress` | supported | Delivered through the responder path that `Pressable` evidence covers. |

## Platform behavior on QNX

Presses arrive as Fabric touch events from `QtTouchDispatcher` (`vnext/src/fabric/QtTouchDispatcher.cpp`), which translates Qt mouse and touch events and hit-tests the mounted item tree. The press feedback is an opacity animation through NativeAnimated, so it depends on that backend being connected. The platform-specific rendering that iOS and Android give `Button` (system button styling) is not reproduced; no source found draws a native Qt button for it.

## Unsupported or partial props

- `color`: React Native applies it as a text or background color through styles. No QNX evidence checks the result.
- `hasTVPreferredFocus` and `nextFocus*`: No evidence found that focus navigation reads them.
- Focus: no Qt tab-chain wiring for `Button` was found. Only TextInput joins the tab chain, and only with its contract on.

## Minimal example

```tsx
import { Button, View } from 'react-native';

export default function Example() {
  return (
    <View style={{ padding: 16 }}>
      <Button title="Save" onPress={() => console.log('saved')} />
    </View>
  );
}
```

## Known gaps

- Pi 400 status is unverified; `docs/component-support.json` lists no device evidence.
- Physical HID presses, keyboard activation and assistive technology are unverified.
- Prop-level coverage beyond `title` and `onPress` has no test evidence.
- Pi 4 QSTI is untested.
