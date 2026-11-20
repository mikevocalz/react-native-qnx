## Props

`Switch` is overridden in `packages/react-native-qnx/Libraries/Components/Switch/Switch.js`. The upstream file uses `useState` and the iOS `SwitchCommands.setValue` command, which lower to `@babel/runtime` helpers not installed in this workspace. The override is a `forwardRef` wrapper around `SwitchNativeComponent`.

| Prop                   | Type     | Status             | Source-backed note                           |
| ---------------------- | -------- | ------------------ | -------------------------------------------- |
| `value`                | boolean  | supported          | Coerced with `value === true`.               |
| `disabled`             | boolean  | supported          | Passed to native, applied in `applySwitch`.  |
| `trackColor` (`true`)  | color    | supported          | Mapped to `onTintColor`, the on-state track. |
| `trackColor` (`false`) | color    | supported          | Mapped to `tintColor`, the off-state track.  |
| `thumbColor`           | color    | supported          | Mapped to `thumbTintColor`.                  |
| `style`                | style    | supported          | Merged after `alignSelf: 'flex-start'`.      |
| `ios_backgroundColor`  | color    | accepted-no-effect | No evidence found that QNX reads it.         |
| `onChange`             | function | supported          | Called with the native event.                |
| `onValueChange`        | function | supported          | Called with `event.nativeEvent.value`.       |

## Events

| Event                  | Status    | Note                                                                                                 |
| ---------------------- | --------- | ---------------------------------------------------------------------------------------------------- |
| `onValueChange(value)` | supported | Fired when the QML item's `value` property differs from the last known value (`QtSwitchBridge.cpp`). |
| `onChange(event)`      | supported | Raw native event, called before `onValueChange`.                                                     |

## Platform behavior on QNX

The switch is a Qt Quick item mounted by `QtQuickMountingLayer::updateSwitch`. `QtSwitchBridge` keeps `lastValue_` and notifies JS only when the item's value changes. Before each prop update the adapter calls `setExpectedValue`, so a value that JS itself writes does not echo back as a change. The wrapper sets `onStartShouldSetResponder` to true and `onResponderTerminationRequest` to false, so the switch keeps a press once it takes it. A controlled switch whose JS state does not update stays at the prop value after the next render.

## Unsupported or partial props

- `ios_backgroundColor`: the override reads only `trackColor.true`, `trackColor.false` and `thumbColor` (`Switch.js`).
- Native commands: the upstream `setValue` command path is not used; value flows only through props.

## Minimal example

```tsx
import { useState } from 'react';
import { Switch, View } from 'react-native';

export default function Example() {
  const [on, setOn] = useState(false);
  return (
    <View style={{ padding: 16 }}>
      <Switch
        value={on}
        onValueChange={setOn}
        trackColor={{ false: '#888', true: '#2a7' }}
        thumbColor="#fff"
      />
    </View>
  );
}
```

## Known gaps

- The note in `docs/component-support.json` says value, disabled, tint props and `onValueChange` are host-tested. No Pi 400 receipt exists, so device behavior is unverified.
- Physical HID activation, keyboard focus and assistive technology are unverified.
- Pi 4 QSTI is untested.
