## Props

`Button` has a QNX override (`packages/react-native-qnx/Libraries/Components/Button.js`). Upstream's `Button.js` styles itself with `Platform.select({ ios, android })` and puts `color` on the title only when `Platform.OS === 'ios'` (`Button.js:237-243`, `:318-355`). `Platform.OS` is `qnx` here and QNX `Platform.select` returns the `qnx` key, then `native`, then `default` (`Platform.js`), so every style entry resolved to undefined. The title had no color, which is black on a dark background (seen on the Pi 400, 2026-11-24), and `color` was applied as the button background. The override keeps upstream's props and `TouchableOpacity` structure and uses the iOS look: a `#007AFF` 18 px title, `color` sets the title color, and a disabled title is `#cdcdcd`.

The host render test (`tests/js/qnx-public-entry-render.test.mjs`, section 4) asserts those styles through the public entry. The override has not been re-run on the Pi 400.

Press delivery is the `Pressable` path (see the Pressable page for what the native touch path emits). The host test `buttonMounts` (`tests/cpp/QtReactHostHostTest.cpp:2088-2116`) only mounts the playground `ButtonApp` and checks for no mount failures and one root child. It sends no press, and `__buttonPress` (`apps/rn-playground/index.js:478`) is never read by any test. The matrix note "reports presses" therefore rests on the `Pressable` probe, not on a `Button` test.

| Prop                                | Status                         | Trace                                                                                         |
| ----------------------------------- | ------------------------------ | --------------------------------------------------------------------------------------------- |
| `title`                             | supported                      | Mount test only. Not upper-cased, since that happens only on Android (`Button.js:270-271`).   |
| `onPress`                           | inherited                      | Same responder path as `Pressable`. No `Button` press test.                                   |
| `color`                             | supported                      | Sets the title color, as on iOS. Host render test.                                            |
| `disabled`                          | blocks presses, dims the title | Passed to `TouchableOpacity`; the title is `#cdcdcd` and wins over `color`. Host render test. |
| `accessibilityLabel`, `testID`      | forwarded                      | Passed to `TouchableOpacity` (`Button.js:280-300`). No test.                                  |
| `hasTVPreferredFocus`, `nextFocus*` | not read                       | Forwarded as props. No file under `vnext/` references them.                                   |
| `touchSoundDisabled`                | no effect                      | Android-only in React Native. No QNX reader.                                                  |

## Events

| Event     | Status                          | Note                                                               |
| --------- | ------------------------------- | ------------------------------------------------------------------ |
| `onPress` | inherited from `Pressable` path | Delivered through the responder system; no `Button`-specific test. |

## Platform behavior on QNX

Presses arrive as Fabric touch events from `QtTouchDispatcher` (`vnext/src/fabric/QtTouchDispatcher.cpp`). Press feedback is the `TouchableOpacity` opacity animation (`useNativeDriver: true`, `TouchableOpacity.js:168-173`), which needs the NativeAnimated backend (`NativeAnimatedNodesManagerProvider`, `vnext/src/runtime/QtReactHost.cpp:1082`). The host test `nativeAnimatedOpacity` checks that an initial animated opacity of 0.5 reaches the Qt item (`QtReactHostHostTest.cpp:2573-2574`). No test checks an opacity change caused by a press.

## Unsupported or partial props

- Platform styling: the iOS look only. No Material or desktop variant.
- `hasTVPreferredFocus` and `nextFocus*`: no native reader.
- Focus: no Qt tab-chain wiring for `Button` was found. Only `TextInput` joins the tab chain, and only with its contract on (`tests/native/text-input/BridgeTest.cpp:45`).

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
