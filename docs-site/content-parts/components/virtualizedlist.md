## Props

`VirtualizedList` is upstream JavaScript from `@react-native/virtualized-lists` (pinned through `react-native` 0.88.1 in `pnpm-lock.yaml`, where `@react-native/virtualized-lists@0.88.1` is a dependency). No QNX override exists in `packages/react-native-qnx/Libraries`, and no native component is registered for it in `vnext/src/runtime/QtReactHost.cpp`. It renders a `ScrollView`, which QNX mounts as a Qt Quick `Flickable`.

Import path: `import { VirtualizedList } from 'react-native'` works on QNX. The public entry, `packages/react-native-qnx/Libraries/ReactNative/PublicAPI.js`, has a getter for it that requires `react-native/Libraries/Lists/VirtualizedList`, the upstream React Native 0.88.1 file that re-exports `VirtualizedList` from `@react-native/virtualized-lists`. Until this was added (issue #178), the entry's Proxy threw `React Native QNX public export "VirtualizedList" is not implemented in this experimental entry.` for it. Projects made by `project create` and `add-qnx` set `withPublicEntry` in Metro (`packages/react-native-qnx-cli/src/create-project.mjs:188-192`, `add-qnx.mjs:63-65`). The export list is asserted in `tests/js/qnx-public-entry.test.mjs`, and `tests/js/qnx-public-entry-render.test.mjs` mounts `VirtualizedList` through the entry with the host test renderer. The [2026-11-20 VM capture](/docs/reference/component-matrix) predates the fix and recorded the throw; no VM or device run of the fixed entry exists yet.

The generated notes give its host coverage as "covered transitively by the FlatList virtualization test". `FlatList` is a thin wrapper over `VirtualizedList` (`FlatList.js:683`), so that test runs `VirtualizedList` code. `tests/js/qnx-public-entry-render.test.mjs` also mounts `VirtualizedList` directly through the public entry and asserts that only the first window of 200 rows renders.

| Prop                                                            | Type     | Status                                   | Source-backed note                                                                          |
| --------------------------------------------------------------- | -------- | ---------------------------------------- | ------------------------------------------------------------------------------------------- |
| `data`, `getItem`, `getItemCount`, `renderItem`, `keyExtractor` | various  | supported                                | Exercised through `FlatList` in the host test.                                              |
| `initialNumToRender`, `windowSize`                              | number   | supported                                | Test used 4 and 1; the window followed the scroll and released cells.                       |
| `onLayout`, `onContentSizeChange`, `onScroll`                   | function | supported                                | Delivered from the ScrollView layer.                                                        |
| `onEndReached`, `onEndReachedThreshold`                         | various  | JavaScript, untested                     | Computed from scroll metrics that QNX supplies. See Events.                                 |
| `onViewableItemsChanged`                                        | function | JavaScript, tested through `SectionList` | One host test asserts the callback runs. See Events.                                        |
| `maxToRenderPerBatch`, `updateCellsBatchingPeriod`              | number   | JavaScript, untested                     | Drive `setTimeout` batching; timers are real `QTimer`s.                                     |
| `getItemLayout`                                                 | function | JavaScript, untested                     | Replaces measured sizes; the test sizes cells from `onLayout`.                              |
| `horizontal`, `inverted`                                        | boolean  | see below                                | `inverted` is dropped for vertical lists by the transform reader. `horizontal` is untested. |
| `refreshing`, `onRefresh`                                       | various  | partial                                  | Routed through a `RefreshControl`; the QNX `ScrollView` calls `onRefresh` on a touch pull.  |

## Events

The same three events as `FlatList` were asserted in the host test: `onScroll`, `onLayout` and `onContentSizeChange`. Their native sources are `QtScrollBridge` (contentX and contentY changes) and the layout events emitted by `ShadowViewMutationAdapter` when `props->onLayout` is set.

`onEndReached`, `onViewableItemsChanged` and `onRefresh` are traced on the [FlatList](/docs/reference/components/flatlist) page: the first two need only `contentOffset`, `contentSize` and `layoutMeasurement` from the scroll event, which `connectScrollEvents` fills (`ShadowViewMutationAdapter.cpp:921-938`). `onViewableItemsChanged` ran in the SectionList test with one or more viewable items. `onEndReached` has no test anywhere in the repository. `onRefresh` never fires.

## Platform behavior on QNX

Windowing depends on three things the platform supplies. Scroll offsets come back from the Flickable. `onLayout` gives cell sizes and the viewport (the test saw a 100pt viewport and four 30pt cells). Timers are `QTimer`s, so the 50 ms default batching period is real wall-clock time (`docs/handoff/TEST_STATUS.md`, "FlatList virtualises"). The test measured that rendered rows follow the scroll position and that rows that left the window are removed from the tree.

Because the list is JavaScript, its behavior on QNX is bounded by the ScrollView limits: `animated` is ignored by `scrollTo` and `scrollToEnd`, and `scrollEnabled`, `pagingEnabled`, scroll indicators, `bounces`, `contentInset` and `refreshControl` are verified as having no effect. See [ScrollView](/docs/reference/components/scrollview).

## Unsupported or partial props

- `inverted`: `VirtualizedList.js:2079-2085` uses `scaleY: -1` for vertical lists on every platform but Android and `scaleX: -1` for horizontal ones. The adapter reads only the x scale factor, so the vertical flip is dropped and the horizontal one mirrors both axes. Details and citations are on the [FlatList](/docs/reference/components/flatlist) page.
- `CellRendererComponent`, `ListHeaderComponent` and `ListFooterComponent`: ordinary React elements rendered as cells; no native dependency. No test sets them.
- `stickyHeaderIndices`: passed to the ScrollView, which implements it in JavaScript; unverified.
- `horizontal`: read natively only by `scrollToEnd`.

## Minimal example

The example imports `VirtualizedList` from the QNX public entry. This import was not run on QNX; the host render test runs it.

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
    <VirtualizedList
      style={{ flexShrink: 0, height: 240 }}
      data={null}
      initialNumToRender={6}
      getItemCount={() => 500}
      getItem={getItem}
      keyExtractor={(item: Row) => item.key}
      renderItem={({ item }: { item: Row }) => (
        <View style={{ height: 30, justifyContent: 'center' }}>
          <Text>{item.label}</Text>
        </View>
      )}
    />
  );
}
```

## Known gaps

- Direct host coverage is the render test in `tests/js/qnx-public-entry-render.test.mjs`, plus the transitive FlatList host test (offscreen Qt platform, 2026-08-14) and the SectionList test for `onViewableItemsChanged`.
- No Pi 400 or Pi 4 QSTI receipt.
- Scroll in the test was produced by writing `contentY`, so real touch or wheel scrolling of a virtualized list has no recorded run.
- The public entry export has host test coverage only. No VM or device run of `import { VirtualizedList } from 'react-native'` exists; the 2026-11-20 VM capture predates the export.

Tracked in [issue 178](https://github.com/mikevocalz/react-native-qnx/issues/178).
