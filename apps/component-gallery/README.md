# QNX component gallery

The tabbed component gallery (Components, Documents, Sections, Controls,
Adaptive, Limits) that ran on the Pi 400 on 2026-11-10, taken from `083c4bd`
on `devin/1794282758-v1-integration`. It uses the public `react-native`
entry through `@rn-qnx/metro-config-qnx/public-entry`. The leaner fixture in
`apps/component-conformance` is unchanged.

```bash
node packages/react-native-qnx-cli/src/bin.mjs bundle-qnx --app component-gallery \
  --out build/gallery/app.bundle.js
"$RNQNX_HOST_HERMESC" -O -emit-binary -out build/gallery/app.hbc build/gallery/app.bundle.js
```

Stage `assets/icon.png` as `assets/assets/icon.png` next to the bundle; the
Image card loads it from there.

The app opens as a 1280 x 800 window (`extra.rnqnx.window.initialSize`) with
the macOS-style title bar. The Components page lays its cards out in one, two
or three columns from the measured window width. Building and running it on
the Pi 400 is in [the Pi 400 desktop guide](../../docs/getting-started/pi400-desktop-app.md).

## Adding an Advanced card

The Advanced tab shows every card in `components/advanced/`. A card is one
file named `<id>.card.js` that default-exports:

```js
export default {
  id: 'long-list', // unique; also the React key
  title: 'Long FlatList',
  detail: '5,000 rows, virtualized', // one line under the title
  order: 50, // optional, default 100; lower comes first, ties sort by id
  Component: LongList, // rendered inside the card, no props
};
```

`components/advanced/index.js` collects the files with Metro's
`require.context` (enabled in `metro.config.js`) into one `ADVANCED_CARDS`
array, so adding a card adds one file and edits nothing else. Two pull
requests that each add a card therefore cannot conflict. A card that needs a
native module or a new dependency brings that change in the same pull
request; keep the card file itself free of side effects at import time, since
every card module loads when the gallery starts. The index throws at startup
if a card is missing `id`, `title`, `detail` or `Component`, or reuses an id.

When the gallery moves to expo-router the same directory feeds the Advanced
tab route, so cards written now keep working.
