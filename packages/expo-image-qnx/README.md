# @rn-qnx/expo-image-qnx

Registers what [`expo-image`](https://docs.expo.dev/versions/latest/sdk/image/)
58 needs from the native side on QNX: the `ExpoImage` native module and the
view config of its `ExpoImage` view. Add both packages to an app:

```json
"dependencies": {
  "expo-image": "58.0.18",
  "@rn-qnx/expo-image-qnx": "workspace:*"
}
```

`project prepare-native` records the package in `native-app.json` as an
`expoModules` entry, as it does for `@rn-qnx/expo-constants-qnx`. The view
itself is the renderer's Fabric component `ViewManagerAdapter_ExpoImage`,
built with `RNQNX_ENABLE_QT_EXPO_IMAGE` (on by default in a consumer app
build). Without that option the JavaScript loads and the view is an unknown
component.

## What works

| expo-image API                                                                                 | QNX                                                                                         |
| ---------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------- |
| `source` (first entry with a `uri`), `placeholder` (a `uri` entry)                             | yes; a URI already in the decoded cache is served from it                                   |
| `contentFit` `cover`, `contain`, `fill`, `none`, `scale-down`                                  | yes                                                                                         |
| `contentPosition`                                                                              | the named edge only (`left`, `right`, `top`, `bottom`); offsets and percentages are ignored |
| `transition`                                                                                   | the duration, as a cross-dissolve; every other effect is drawn as a cross-dissolve          |
| `tintColor`, `blurRadius`                                                                      | when the Qt build ships `QtQuick.Effects`; otherwise ignored with one console warning       |
| `cachePolicy`                                                                                  | `none` turns Qt's own pixmap cache off for the view; other values behave alike              |
| `onLoadStart`, `onLoad`, `onError`, `onDisplay`                                                | yes                                                                                         |
| `onProgress`                                                                                   | `loaded` and `total` out of 1000, because Qt reports a fraction and no byte counts          |
| `backgroundColor`, `borderRadius`                                                              | the background is drawn with the radius; the image itself is not clipped to rounded corners |
| `headers`, `blurhash`, `thumbhash`, `priority`, `recyclingKey`, `allowDownscaling`, `autoplay` | accepted and ignored                                                                        |
| `Image.prefetch`, `clearMemoryCache`, `clearDiskCache`                                         | resolve `false`: use React Native's `Image.prefetch`, which fills the cache this view reads |
| `Image.generateBlurhashAsync`, `loadAsync` and the cache read/write functions                  | absent (calling one throws "is not a function")                                             |
| animated images                                                                                | drawn as their first frame; `startAnimating` and `stopAnimating` do nothing                 |
