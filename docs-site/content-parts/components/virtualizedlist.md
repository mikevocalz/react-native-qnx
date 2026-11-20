## Props

`VirtualizedList` is upstream React Native JavaScript. No QNX override exists in `packages/react-native-qnx/Libraries`, and no native component is registered for it in `vnext/src/runtime/QtReactHost.cpp`. It renders a `ScrollView`, which QNX mounts as a Qt Quick `Flickable`.

The generated notes give its host coverage as "covered transitively by the FlatList virtualization test". `FlatList` is a thin wrapper over `VirtualizedList`, so that test runs `VirtualizedList` code. No test uses `VirtualizedList` directly.

| Prop                                                                                                  | Type     | Status             | Source-backed note                                                    |
| ----------------------------------------------------------------------------------------------------- | -------- | ------------------ | --------------------------------------------------------------------- |
| `data`, `getItem`, `getItemCount`, `renderItem`, `keyExtractor`                                       | various  | supported          | Exercised through `FlatList` in the host test.                        |
| `initialNumToRender`, `windowSize`                                                                    | number   | supported          | Test used 4 and 1; the window followed the scroll and released cells. |
| `onLayout`, `onContentSizeChange`, `onScroll`                                                         | function | supported          | Delivered from the ScrollView layer.                                  |
| `getItemLayout`                                                                                       | function | accepted-no-effect | No evidence found of a test; the test sizes cells from `onLayout`.    |
| `maxToRenderPerBatch`, `updateCellsBatchingPeriod`, `onEndReachedThreshold`, `onViewableItemsChanged` | various  | accepted-no-effect | No evidence found of a QNX test.                                      |

## Events

The same three events as `FlatList` were asserted in the host test: `onScroll`, `onLayout` and `onContentSizeChange`. Their native sources are `QtScrollBridge` (contentX and contentY changes) and the layout events emitted by `ShadowViewMutationAdapter` when `props->onLayout` is set.

## Platform behavior on QNX

Windowing depends on three things the platform supplies. Scroll offsets come back from the Flickable. `onLayout` gives cell sizes and the viewport (the test saw a 100pt viewport and four 30pt cells). Timers are `QTimer`s, so the 50 ms default batching period is real wall-clock time (`docs/handoff/TEST_STATUS.md`, "FlatList virtualises"). The test measured that rendered rows follow the scroll position and that rows that left the window are removed from the tree.

Because the list is JavaScript, its behavior on QNX is bounded by the ScrollView limits: `animated` is ignored by `scrollTo` and `scrollToEnd`, and props like `scrollEnabled` and `pagingEnabled` have no evidence of native effect. See [ScrollView](/docs/reference/components/scrollview).

## Unsupported or partial props

- `getItemLayout`, `onEndReached` and the viewability callbacks: no QNX-specific evidence either way.
- `CellRendererComponent`, `ListHeaderComponent` and sticky headers: No evidence found. `stickyHeaderIndices` is a ScrollView prop with no recorded native handling.
- `inverted` and `horizontal`: untested; `horizontal` is read natively only by `scrollToEnd`.

## Minimal example

```tsx
import React from 'react';
import { Text, View, VirtualizedList } from 'react-native';

type Row = { key: string; label: string };
const getItem = (_data: unknown, index: number): Row => ({
  key: String(index),
  label: 'Item ' + index,
});

export default function App() {
  return (
    <VirtualizedList<Row>
      style={{ flexShrink: 0, height: 240 }}
      data={null}
      initialNumToRender={6}
      getItemCount={() => 500}
      getItem={getItem}
      keyExtractor={(item) => item.key}
      renderItem={({ item }) => (
        <View style={{ height: 30, justifyContent: 'center' }}>
          <Text>{item.label}</Text>
        </View>
      )}
    />
  );
}
```

## Known gaps

- No direct test. The only evidence is the transitive FlatList host test, run on the offscreen Qt platform on 2026-08-14.
- No Pi 400 or Pi 4 QSTI receipt.
- Scroll in the test was produced by writing `contentY`, so real touch or wheel scrolling of a virtualized list has no recorded run.
