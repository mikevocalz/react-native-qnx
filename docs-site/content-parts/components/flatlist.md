## Props

`FlatList` is React Native's upstream JavaScript. No QNX override exists under `packages/react-native-qnx/Libraries` (the directory holds only `Image`, `Components` entries for StatusBar, RefreshControl, TextInput, Switch and KeyboardAvoidingView, and a few utilities). The host test comment says it plainly: FlatList is upstream JavaScript over `ScrollView`, so it needs a mounted Flickable, measured children, `Dimensions`, timers and scroll offsets coming back (`tests/cpp/QtReactHostHostTest.cpp`, `flatListRendersAndVirtualises`).

| Prop                                 | Type     | Status             | Source-backed note                                                                       |
| ------------------------------------ | -------- | ------------------ | ---------------------------------------------------------------------------------------- |
| `data`, `renderItem`, `keyExtractor` | various  | supported          | Exercised by the 20-row test list.                                                       |
| `initialNumToRender`                 | number   | supported          | Test used 4 and got exactly 4 initial cells.                                             |
| `windowSize`                         | number   | supported          | Test used 1; cells outside the window were released.                                     |
| `onLayout`, `onContentSizeChange`    | function | supported          | Both reached JavaScript in the test.                                                     |
| `onScroll`                           | function | supported          | Received offset 300 after stepped scrolling.                                             |
| `getItemLayout`                      | function | accepted-no-effect | No evidence found that any test covers it; the test relied on measured `onLayout` sizes. |

## Events

`onScroll`, `onLayout` and `onContentSizeChange` are the events the host test asserts. `onEndReached`, `onViewableItemsChanged`, `onRefresh` and `onScrollToIndexFailed`: No evidence found of a test on QNX. They are JavaScript behavior that depends on the same scroll offsets, so they may work, but this page does not claim it.

## Platform behavior on QNX

Virtualization follows scroll offsets that the Flickable reports through `QtScrollBridge` (see [ScrollView](/docs/reference/components/scrollview)). In the test, a 100pt viewport scrolled to offset 300 left cells at offsets 0 to 90 (the retained initial region) and 270 to 390 (the live window), with rows 4 through 7 released. `VirtualizedList` batches window updates behind a real 50 ms `setTimeout`, and QNX timers are `QTimer`s, so updates arrive after wall-clock time, not after a fixed number of event pumps (`docs/handoff/TEST_STATUS.md`).

## Unsupported or partial props

- Everything that depends on `ScrollView` props marked accepted-no-effect there: `scrollEnabled`, `pagingEnabled`, scroll indicators, `bounces`, `contentInset`.
- `scrollToIndex`, `scrollToOffset` and `scrollToEnd` on the list ref route through `ScrollView` commands. Those commands ignore `animated`.
- `refreshing` and `onRefresh` go through `RefreshControl`, which has its own page: /docs/reference/components/refreshcontrol.

## Minimal example

```tsx
import React from 'react';
import { FlatList, Text, View } from 'react-native';

const rows = Array.from({ length: 200 }, (_, i) => ({ id: String(i), title: 'Row ' + i }));

export default function App() {
  return (
    <FlatList
      style={{ flexShrink: 0, height: 240 }}
      data={rows}
      keyExtractor={(item) => item.id}
      initialNumToRender={8}
      windowSize={5}
      renderItem={({ item }) => (
        <View style={{ height: 30, justifyContent: 'center' }}>
          <Text>{item.title}</Text>
        </View>
      )}
    />
  );
}
```

## Known gaps

- Host-only evidence, offscreen Qt platform, from the 2026-08-14 entry in `docs/handoff/TEST_STATUS.md`. No Pi 400 or Pi 4 QSTI receipt.
- The test used fixed 30pt rows with a playground bundle, not a real app list. Variable-height rows, `numColumns`, `horizontal` and `inverted` have no recorded test.
- Scrolling in the test is set by writing `contentY`, not by touch or wheel input.
