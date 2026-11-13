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
file named `<id>.card.js` with an ES module default export (`export default`;
`module.exports` is rejected):

```js
export default {
  id: 'long-list', // unique; also the React key
  title: 'Long FlatList',
  detail: '5,000 rows, virtualized', // one line under the title
  order: 50, // optional, default 100; lower comes first, ties sort by id
  Component: LongList, // rendered inside the card, no props
  framed: true, // optional; false when Component draws its own frame and title
};
```

The react-native-svg cards are registered this way too:
`components/advanced/svg-*.card.js` wrap the drawings in `svg/` through
`svg/advancedCard.js`, which loads them on first render and shows a notice
when the host was built without react-native-svg. `svg/cards.js` still lists
them for the standalone `RNQnxSvgGallery` component.

`components/advanced/index.js` collects the files with Metro's
`require.context` (enabled in `metro.config.js`) into one `ADVANCED_CARDS`
array, so adding a card adds one file and edits nothing else. Two pull
requests that each add a card therefore cannot conflict. A card that needs a
native module or a new dependency brings that change in the same pull
request; keep the card file itself free of side effects at import time, since
every card module loads when the gallery starts. The index throws at startup,
which stops the whole gallery, if a card is missing `id`, `title`, `detail`
or `Component`, has a non-numeric `order`, or reuses an id.

Editing a card file reloads the whole app rather than refreshing the card in
place, because card modules export objects, not components; put the
component in its own file if you want Fast Refresh to keep its state. Metro
picks up a newly added card file on the next reload.

When the gallery moves to expo-router the same directory feeds the Advanced
tab route, so cards written now keep working.
