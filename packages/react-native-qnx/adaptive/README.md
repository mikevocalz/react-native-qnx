# Adaptive native layouts

Import these opt-in React Native components from
`@rn-qnx/react-native-qnx/adaptive`. They use the existing React Native public
components and Yoga layout tree. They do not introduce another renderer or
require premium packages. The source and generated Expo starters use the same
components as the component gallery.

## Window space and content space

`getWindowSizeClass({ width, height })` classifies the **available app window in
logical units**. Use the public `useWindowDimensions()` result, not the physical
screen size or a device name. `useWindowSizeClass()` observes that same standard
hook and lets React clean up its subscription on unmount.

| Width class   | Available logical width |
| ------------- | ----------------------- |
| `compact`     | below 600               |
| `medium`      | 600 to below 840        |
| `expanded`    | 840 to below 1200       |
| `large`       | 1200 to below 1600      |
| `extra-large` | 1600 and above          |

Height is `compact` below 480, `medium` below 900, and `expanded` from 900.
A wide but short window still needs scrolling and a compact header.

```ts
import { getWindowSizeClass, getGridLayout } from '@rn-qnx/react-native-qnx/adaptive';

getWindowSizeClass({ width: 1280, height: 400 });
// { width: 'large', height: 'compact' }
getGridLayout({ width: 616, minItemWidth: 300, gap: 16 });
// { columns: 2, itemWidth: 300 }

// Validate measurements at their source. Invalid values are not silently
// converted into a successful layout; these pure functions throw RangeError.
try {
  getGridLayout({ width: Number.NaN });
} catch (error) {
  if (!(error instanceof RangeError)) throw error;
}
```

`getGridLayout` accepts zero width during initial measurement. It returns one
zero-width column until actual space is available. A narrow container uses one
fitting column even when it cannot meet `minItemWidth`. The gap must be finite
and nonnegative; the preferred minimum must be positive; `maxColumns` is 1, 2,
or 3. No physical-pixel conversion occurs in these helpers.

## Grid: preserve children while reflowing

```tsx
import React from 'react';
import { TextInput, View, Text } from 'react-native';
import { AdaptiveGrid } from '@rn-qnx/react-native-qnx/adaptive';

export function Examples() {
  return (
    <AdaptiveGrid minItemWidth={300} gap={16} maxColumns={3}>
      <TextInput key="notes" accessibilityLabel="Notes" defaultValue="Keep editing" />
      <View key="help">
        <Text>Related information</Text>
      </View>
    </AdaptiveGrid>
  );
}
```

The grid measures its own inner content width after outer padding and the
shell's navigation. It keeps each child below the same keyed native wrapper
while changing widths and margins. It never groups children into new row
parents when a breakpoint changes. Supply stable keys and keep application
state above layout decisions. The grid does not save state for children an
application explicitly removes.

## Shell: application-owned navigation

```tsx
import React from 'react';
import { Text } from 'react-native';
import { create } from 'zustand';
import { AdaptiveShell } from '@rn-qnx/react-native-qnx/adaptive';

const navigation = [
  { id: 'notes', label: 'Notes' },
  { id: 'settings', label: 'Settings' },
];
const useApp = create<{ page: string; select: (page: string) => void }>((set) => ({
  page: 'notes',
  select: (page) => set({ page }),
}));

export function App() {
  const page = useApp((state) => state.page);
  const select = useApp((state) => state.select);
  return (
    <AdaptiveShell title="Notes" navigation={navigation} selectedId={page} onSelect={select}>
      <Text>{page}</Text>
    </AdaptiveShell>
  );
}
```

The default sidebar appears at 840 logical units and is 208 units wide. Below
that width, its destinations remain available through a native navigation
dialog. Both the dialog and sidebar can scroll. Compact-height windows use a
shorter header and omit its optional subtitle. The application content always
stays below the same parent. Navigation selection is controlled by the app;
the shell does not replace the app's router or own its business state.

The shell owns only instance-local geometry and menu state, using Zustand.
Closing navigation restores its trigger focus after the overlay commit;
expanding an open menu moves focus to the selected sidebar destination.
Ordinary content focus is not deliberately moved during resize. These focus
commands require the host's standard React Native imperative focus support.
Physical keyboard, input-method and assistive-technology qualification remains
a target-specific validation task.

`defaultAdaptiveColors` supplies the gallery's navy/teal palette. Pass `colors`
to apply a consuming app's theme. These helpers do not infer touch capability,
hinges, system safe areas, or window-manager controls from a size class. The
native host remains responsible for accurate dimensions, display/font scale
and insets.

The generated starter keeps its input and scroll view mounted while changing
pages. The gallery lazily mounts a route on its first visit, then retains the
same native nodes on later visits. Both place inactive routes in transparent,
non-interactive layers with unchanged content geometry. They avoid `display:
none` for cached routes because Fabric removes hidden native subtrees. They
pass standard accessibility visibility props; operating-system assistive
technology support requires separate qualification. Neither app changes
component keys to represent the window size.
