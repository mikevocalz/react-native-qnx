# Gallery layout through real Yoga, red and green (2026-11-13)

Test: `tests/js/qnx-gallery-yoga-layout.test.mjs`, helpers
`tests/js/helpers/yoga-host.mjs` (react-test-renderer tree to `yoga-layout` 3.2.1
nodes, `onLayout` fed back until frames settle) and
`tests/js/helpers/gallery-loader.cjs` (Babel for the gallery's ESM/JSX, host
stand-ins for `react-native` and the native Expo packages, `require.context`).
The route files in `apps/component-gallery/app` are the ones the app ships.

It mounts the five tab pages beside a 96 px rail (and a 64 px bottom bar) and
the four drawer destinations, at 586x600, 880x600 and 1280x800, and checks:

1. every card ends at or before the page's right padding edge, and no box
   leaves the page, with the drawer destination mounted before and after the
   tab pages;
2. each drawer destination is as wide as the window and stays inside it;
3. every tab page has the same frames as when it is alone in the window, after
   a drawer destination mounts, after each other destination mounts, and after
   the drawer scene is resized to three other widths. All five tab pages stay
   laid out in this block, as react-native-screens keeps them.

## Red against the shared-width logic

`git checkout 011ed60^ -- apps/component-gallery/ui.js apps/component-gallery/screens`
(the single `width` in the zustand store written by every `Screen`), then the
test, then `git checkout HEAD -- apps/component-gallery`:

```
AssertionError: 586x600 rail tab=index drawerFirst=true: a 530px card ends at 654, past the page padding edge 558 (page 96..586)
```

Block 3 on its own also fails against that code:
`586x600 focus=index: mounting a drawer page changed the index page`.

## Green against the current code

`node tests/js/qnx-gallery-yoga-layout.test.mjs` prints
`qnx-gallery-yoga-layout: ok`.

## Limits

The text measure is a fixed 0.55 em advance per character, so absolute heights
differ from Qt's font metrics. The assertions compare widths and right edges and
a page against itself, which do not depend on glyph metrics. The navigator is
modelled (stacked scenes, rail, real `MaybeScreen` fallback); expo-router's own
navigators are not run here.
