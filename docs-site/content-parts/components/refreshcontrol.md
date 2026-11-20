## Props

`RefreshControl` is a QNX-specific JavaScript module at `packages/react-native-qnx/Libraries/Components/RefreshControl/RefreshControl.js`. Its source comment says QNX targets have no native pull-to-refresh gesture, so it is a minimal no-op. The function returns `null` and declares no logic. The only default is `refreshing: false`.

| Prop                                                                                                             | Type     | Status             | Source-backed note                                         |
| ---------------------------------------------------------------------------------------------------------------- | -------- | ------------------ | ---------------------------------------------------------- |
| `refreshing`                                                                                                     | boolean  | accepted-no-effect | Default `false`. Not read.                                 |
| `onRefresh`                                                                                                      | function | accepted-no-effect | Never called.                                              |
| `colors`, `tintColor`, `title`, `titleColor`, `progressBackgroundColor`, `progressViewOffset`, `size`, `enabled` | various  | accepted-no-effect | The function takes no arguments, so every prop is ignored. |

## Events

| Event       | Status      | Note                                                               |
| ----------- | ----------- | ------------------------------------------------------------------ |
| `onRefresh` | unsupported | Nothing fires it. No gesture, key or timer is wired in the module. |

## Platform behavior on QNX

When passed as the `refreshControl` prop of a `ScrollView` or `FlatList`, the element renders nothing. The list scrolls as usual and no refresh indicator appears. The public entry test (`tests/js/qnx-public-entry.test.mjs`) lists `RefreshControl` among the names the package entry must export, so importing it from `react-native` does not throw. That test checks the export, not any behavior of the component.

## Unsupported or partial props

All props are ignored, for the reason stated at the top of `packages/react-native-qnx/Libraries/Components/RefreshControl/RefreshControl.js`: the target has no native pull-to-refresh. Pointer-driven or keyboard-driven alternatives were not found in the code. If an app needs refresh, put a button in the list header that calls the same function you would pass to `onRefresh`.

## Minimal example

```tsx
import React, { useCallback, useState } from 'react';
import { Button, FlatList, RefreshControl, Text } from 'react-native';

export default function Feed() {
  const [refreshing, setRefreshing] = useState(false);
  const [items, setItems] = useState(['a', 'b', 'c']);

  const reload = useCallback(() => {
    setRefreshing(true);
    setItems((prev) => [...prev, String(prev.length)]);
    setRefreshing(false);
  }, []);

  return (
    <FlatList
      data={items}
      keyExtractor={(item) => item}
      renderItem={({ item }) => <Text>{item}</Text>}
      ListHeaderComponent={<Button title="Reload" onPress={reload} />}
      refreshControl={<RefreshControl refreshing={refreshing} onRefresh={reload} />}
    />
  );
}
```

On QNX the `Button` is the only way to trigger `reload`. The `refreshControl` element is accepted and draws nothing.

## Known gaps

- Pull-to-refresh does not exist on this target and no work toward it was found in the repository.
- The status table shows `no-op` on host and unverified on both devices. No receipt exercises the component.
