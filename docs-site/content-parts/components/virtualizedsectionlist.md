## Props

`VirtualizedSectionList` is a React Native JavaScript component from `@react-native/virtualized-lists`. `SectionList` renders it, and it renders `VirtualizedList` over a `ScrollView`. QNX has no override for it (nothing under `packages/react-native-qnx/Libraries`) and registers no native component for it in `vnext/src/runtime/QtReactHost.cpp`.

Is it exported? Yes. The QNX public entry, `packages/react-native-qnx/Libraries/ReactNative/PublicAPI.js`, has a getter for `VirtualizedSectionList` that requires `react-native/Libraries/Lists/VirtualizedSectionList`, the upstream React Native 0.88.1 file that re-exports it from `@react-native/virtualized-lists` (pinned through `react-native@0.88.1` in `pnpm-lock.yaml`). Until this was added (issue #178), the entry's Proxy threw `React Native QNX public export "VirtualizedSectionList" is not implemented in this experimental entry.` for it. `tests/js/qnx-public-entry.test.mjs` asserts the export list, and `tests/js/qnx-public-entry-render.test.mjs` mounts the component through the entry. The 2026-11-20 VM capture predates the fix; no VM or device run of the fixed entry exists yet.

The generated notes keep the host status at unverified, because the render test checks headers and rows only. It mounts `VirtualizedSectionList` by name through the public entry, and the `SectionList` host test mounts it indirectly (see [SectionList](/docs/reference/components/sectionlist)). Apps normally reach it through `SectionList`, so the props are the `SectionList` props plus the `VirtualizedList` ones.

| Prop                                     | Type     | Status                            | Source-backed note                                                                                               |
| ---------------------------------------- | -------- | --------------------------------- | ---------------------------------------------------------------------------------------------------------------- |
| `sections`, `renderItem`, `keyExtractor` | various  | supported (through `SectionList`) | Used by the `SectionList` host test.                                                                             |
| `getItemCount`, `getItem`                | function | supported                         | These come from `VirtualizedList`, which the FlatList host test covers.                                          |
| `initialNumToRender`, `windowSize`       | number   | supported                         | Inherited from `VirtualizedList`; tested through `FlatList`.                                                     |
| `onViewableItemsChanged`                 | function | supported (initial report)        | Wrapped by `_onViewableItemsChanged` (`VirtualizedSectionList.js:340-348`); ran in the `SectionList` test.       |
| `stickySectionHeadersEnabled`            | boolean  | JavaScript, untested              | Builds `stickyHeaderIndices` (`VirtualizedSectionList.js:189-200`); no default here, `SectionList` supplies one. |

## Events

`onScroll`, `onLayout` and `onContentSizeChange` reach JavaScript from the Qt Flickable (`QtScrollBridge`) and the layout emitter, as for `FlatList`. `onViewableItemsChanged` is the only VirtualizedSectionList-level event with a recorded run, through `SectionList`.

## Platform behavior on QNX

Nothing in this component is native. Its behavior is the behavior of the `VirtualizedList` and `ScrollView` underneath it: scroll offsets come back from a Qt Quick `Flickable`, content size is recomputed from the Flickable's children (`updateScrollContentSize`), and `scrollTo` and `scrollToEnd` ignore `animated`. See [ScrollView](/docs/reference/components/scrollview) and [FlatList](/docs/reference/components/flatlist).

## Unsupported or partial props

- Sticky section headers: JavaScript through ScrollView `stickyHeaderIndices`; not tested. `SectionList` turns them on only when asked, because its default is iOS-only.
- `scrollToLocation`: JavaScript (`VirtualizedSectionList.js:140-169`), ends in the `scrollTo` command, which ignores `animated`. No test calls it.
- Section separators: ordinary cells; no test.
- Direct use by app code is not a documented React Native pattern.

## Minimal example

Import `SectionList`, which wraps this component. `VirtualizedSectionList` is also importable from `react-native` on QNX, but app code rarely needs it.

```tsx
import React from 'react';
import { SectionList, Text } from 'react-native';

const sections = [
  { title: 'A', data: ['one', 'two'] },
  { title: 'B', data: ['three'] },
];

export default function App() {
  return (
    <SectionList
      style={{ flexShrink: 0, height: 200 }}
      sections={sections}
      keyExtractor={(item) => item}
      renderItem={({ item }) => <Text>{item}</Text>}
      renderSectionHeader={({ section }) => <Text>{section.title}</Text>}
    />
  );
}
```

## Known gaps

- Direct host coverage is a render check of two sections. Evidence missing: a test that scrolls a `SectionList` or exercises sticky headers, on host and on a device.
- No Pi 400 or Pi 4 QSTI receipt.
- The public entry export has host test coverage only. No VM or device run of `import { VirtualizedSectionList } from 'react-native'` exists; the 2026-11-20 VM capture predates the export.

Tracked in [issue 178](https://github.com/mikevocalz/react-native-qnx/issues/178).
