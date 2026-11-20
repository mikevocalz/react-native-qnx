## Props

`VirtualizedSectionList` is an internal React Native JavaScript component. `SectionList` renders it, and it renders `VirtualizedList` over a `ScrollView`. QNX has no override for it (nothing under `packages/react-native-qnx/Libraries`) and registers no native component for it in `vnext/src/runtime/QtReactHost.cpp`. The generated notes say direct coverage is unverified, and I found no test that mounts it by name.

Apps normally reach it through [SectionList](/docs/reference/components/sectionlist), so the props are the `SectionList` props plus the `VirtualizedList` ones. This page lists only what source evidence supports.

| Prop                                     | Type     | Status             | Source-backed note                                                      |
| ---------------------------------------- | -------- | ------------------ | ----------------------------------------------------------------------- |
| `sections`, `renderItem`, `keyExtractor` | various  | supported          | Through `SectionList`, per the component notes.                         |
| `getItemCount`, `getItem`                | function | supported          | These come from `VirtualizedList`, which the FlatList host test covers. |
| `initialNumToRender`, `windowSize`       | number   | supported          | Inherited from `VirtualizedList`; tested through `FlatList`.            |
| `stickySectionHeadersEnabled`            | boolean  | accepted-no-effect | No evidence found that native code implements sticky headers.           |

## Events

`onScroll`, `onLayout` and `onContentSizeChange` reach JavaScript from the Qt Flickable (`QtScrollBridge`) and the layout emitter, as for `FlatList`. No VirtualizedSectionList-specific event has evidence.

## Platform behavior on QNX

Nothing in this component is native. Its behavior is the behavior of the `VirtualizedList` and `ScrollView` underneath it: scroll offsets come back from a Qt Quick `Flickable`, content size is recomputed from the Flickable's children (`updateScrollContentSize`), and `scrollTo` and `scrollToEnd` ignore `animated`. See [ScrollView](/docs/reference/components/scrollview) and [FlatList](/docs/reference/components/flatlist).

## Unsupported or partial props

- Sticky section headers: No evidence found of native handling or a test.
- `scrollToLocation` and section separators: No evidence found of a QNX run.
- Direct use by app code is not a documented React Native pattern, so there is no reason to expect a recorded test.

## Minimal example

Import `SectionList`, which wraps this component. `VirtualizedSectionList` is not exported from the `react-native` entry as a public component.

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

- Direct component coverage is unverified. Evidence missing: a test that renders `VirtualizedSectionList` or a `SectionList` with sections and scrolls it, on host and on a device.
- No Pi 400 or Pi 4 QSTI receipt.
- I did not verify whether `react-native` exports `VirtualizedSectionList` on the QNX entry; the minimal example avoids depending on it.
