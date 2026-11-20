## Props

`TouchableHighlight` has no QNX override and no direct test. `docs/component-support.json` records host `unverified`. Its feedback does not use native animation: `_showUnderlay` calls `setState` with `extraStyles` (`TouchableHighlight.js:183-190`) and the child gets `opacity: activeOpacity ?? 0.85` and the underlay `backgroundColor: underlayColor ?? 'black'` (lines 172-181). That is an ordinary React re-render, so the change reaches Qt as a normal Fabric prop update on a `View`, the same path other style updates use. No test presses this component and checks the result.

Native touch path: only Fabric `onTouchStart`, `onTouchMove`, `onTouchEnd` and `onTouchCancel` reach React (`vnext/src/fabric/QtTouchDispatcher.cpp:227-247,280-360`). There is no hover or pointer enter and leave event, and hit testing uses item geometry (lines 141-225), so a press cannot start outside the view's box. Long-press and delay timing, `hitSlop` and `pressRetentionOffset` regions are computed in JavaScript by `Pressability` (`Pressability.js:464-478,800-867`) using `setTimeout` and a shadow-tree `measure`. The only press test is the `Pressable` probe (`tests/cpp/QtPressableProbe.cpp`), which covers press in, out, press, cancel, deactivation, disabled, unmount and remount, not long press or regions.

| Prop                                                | Status    | Trace                                                                                                                                                                                                                                                                                     |
| --------------------------------------------------- | --------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `onPress`, `onPressIn`, `onPressOut`, `onLongPress` | inherited | `Pressability` and the touch path above.                                                                                                                                                                                                                                                  |
| `underlayColor`                                     | untested  | Applied as underlay `backgroundColor` through `setState` (`TouchableHighlight.js:176-179`).                                                                                                                                                                                               |
| `activeOpacity`                                     | untested  | Applied as child opacity through `setState` (line 173).                                                                                                                                                                                                                                   |
| `onShowUnderlay`, `onHideUnderlay`                  | untested  | Called from `_showUnderlay` and `_hideUnderlay` (lines 183-205), which run inside the press callbacks. The press handler skips the immediate show and hide when `Platform.isTV` is true (line 140); QNX `Platform.isTV` is `false` (`Platform.js:45-47`), so the show and hide path runs. |
| `delayPressOut`                                     | untested  | JS timer (`TouchableHighlight.js:140-144`).                                                                                                                                                                                                                                               |
| `hitSlop`, `delayLongPress`                         | partial   | JS regions and timers; cannot enlarge the starting area.                                                                                                                                                                                                                                  |
| `disabled`, `style`, `children`                     | untested  | Plain props.                                                                                                                                                                                                                                                                              |

## Events

| Event                              | Status    | Note                                               |
| ---------------------------------- | --------- | -------------------------------------------------- |
| Press lifecycle                    | inherited | Depends on the same touch dispatch as `Pressable`. |
| `onShowUnderlay`, `onHideUnderlay` | untested  | JS callbacks, no native involvement.               |

## Platform behavior on QNX

Touches are produced by `QtTouchDispatcher` and consumed by React Native's JS responder system. The highlight needs no native animation backend. Props are untested here; reasoning from the source says the update is a normal re-render.

## Unsupported or partial props

- `hasTVPreferredFocus`, `nextFocus*`: not read by any file under `vnext/`.
- No prop was found to be unsupported by native code, and none has a test.

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
