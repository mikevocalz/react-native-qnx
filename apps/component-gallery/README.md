# QNX component gallery

The component gallery, an expo-router app: a left drawer around five tabs
(Components, Advanced, Documents, Controls, More). It started as the tabbed
gallery that ran on the Pi 400 on 2026-11-10, taken from `083c4bd` on
`devin/1794282758-v1-integration`. It uses the public `react-native` entry
through `@rn-qnx/metro-config-qnx/public-entry`. The leaner fixture in
`apps/component-conformance` is unchanged.

```text
app/_layout.js            Drawer (navigation only: Home, Components, Media, Settings, About)
app/(tabs)/_layout.js     Tabs: bottom bar under 700 px, left rail from 700 px, live
app/(tabs)/index.js       Components   (screens/ComponentsScreen.js)
app/(tabs)/advanced.js    Advanced     (screens/AdvancedScreen.js, cards in components/advanced)
app/(tabs)/documents.js   Documents
app/(tabs)/controls.js    Controls
app/(tabs)/more.js        More: Sections, Adaptive and Limits pages
app/{components,media,settings,about}.js   drawer pages
app-ctx.js                require.context of ./app (replaces expo-router/_ctx)
index.js                  registers the root; keep it free of components so Fast Refresh works
```

The drawer lists destinations only. Cards belong in the tabs.
[`@rn-qnx/expo-router-qnx`](../../packages/expo-router-qnx/README.md) explains how
`Tabs` and `Drawer` run on QNX. With the host's native Qt drawer and tab bar the
gallery uses them; without them it draws the same UI from React Native views.
For a keyboard-only run, the developer menu (F1) has Toggle drawer and a
Go to entry for every tab and page.

```bash
node packages/react-native-qnx-cli/src/bin.mjs bundle-qnx --app component-gallery \
  --out build/gallery/app.bundle.js
"$RNQNX_HOST_HERMESC" -O -emit-binary -out build/gallery/app.hbc build/gallery/app.bundle.js
```

Stage `assets/icon.png`, `assets/anim.gif` and `assets/anim.webp` under
`assets/assets/` next to the bundle. The Image and Expo Image cards load
those files; Metro's JavaScript bundle does not embed them. Animated WebP
also requires the staged Qt image-format plugin.

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

The Advanced tab route (`app/(tabs)/advanced.js`) renders that array.

## Video and Audio

The Advanced tab has a Video card (`components/advanced/video.card.js`:
`QtVideo` with the native control bar and a Bundled / Mac URL / Bad URL source
switch) and an Audio card (`audio.card.js`: a bundled tone with play, pause and
volume). The Media screen in the drawer shows the expo-image card, then both at
full width. They need a host built with `RNQNX_ENABLE_QT_VIDEO=ON` and the Qt
Multimedia add-on ([media guide](../../docs/getting-started/media.md)); without
them the cards show a notice. Stage `assets/media/` next to the executable. The
Mac URL source expects `python3 -m http.server 8765` in that directory on
192.168.2.1.
