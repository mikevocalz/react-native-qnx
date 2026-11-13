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

The view is `ExpoImageItem` (`vnext/src/views/qt/image`), a `QQuickPaintedItem`
that loads, decodes and paints the picture with `QPainter`. It does not use
`QtQuick.Effects` or any shader, because the Pi's Qt runtime ships neither.

| expo-image API                                                              | QNX                                                                                                                                                                                                                      |
| --------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| `source` (object or array)                                                  | `uri` with its `headers` and `cacheKey`; with several entries, the smallest `width` that covers the view, else the largest; `blurhash` and `thumbhash` sources                                                           |
| `headers`                                                                   | sent on http(s) requests; the memory and disk cache key includes them (or the `cacheKey`)                                                                                                                                |
| `placeholder`, `placeholderContentFit`                                      | `uri`, `blurhash` or `thumbhash`; shown until the image is ready, then transitioned away                                                                                                                                 |
| `contentFit`                                                                | `cover`, `contain`, `fill`, `none` (natural size), `scale-down`                                                                                                                                                          |
| `contentPosition`                                                           | edge words, offsets in points and percentages, in object or string form                                                                                                                                                  |
| `transition`                                                                | `duration`, `timing`, and the effects `cross-dissolve` and `flip-from-top`, `-right`, `-bottom`, `-left`; `curl-up` and `curl-down` draw as a cross-dissolve and log one warning (expo-image documents them as iOS-only) |
| `tintColor`, `blurRadius`                                                   | yes, in `QPainter`; blur is a three-pass box blur on the CPU, not applied to placeholders                                                                                                                                |
| `borderRadius` (and per-corner radii), `borderWidth`, `borderColor`         | the picture is clipped to the rounded shape                                                                                                                                                                              |
| `cachePolicy`                                                               | `none`, `disk`, `memory`, `memory-disk`; the disk cache holds 128 MiB with least-recently-used eviction                                                                                                                  |
| `recyclingKey`                                                              | a changed key clears the old picture at once, with no transition                                                                                                                                                         |
| `allowDownscaling`                                                          | an image over twice the view in both dimensions decodes at the smallest size that still covers it; `onLoad` reports the original size                                                                                    |
| `autoplay`, animated GIF and WebP                                           | played with `QMovie` when the Qt build has the format plugin; `autoplay: false` shows the first frame                                                                                                                    |
| `onLoadStart`, `onProgress`, `onLoad`, `onError`, `onDisplay`               | `onProgress` carries bytes received and the total; `onLoad.cacheType` is `none`, `disk` or `memory`                                                                                                                      |
| `testID`, `nativeID`, `pointerEvents`, `hitSlop`, `accessibility*`          | `hitSlop` widens the touch area; `pointerEvents` `none` and `box-none` ignore touches; accessibility maps to Qt `Accessible` properties                                                                                  |
| `opacity`, `transform`, `onLayout`                                          | applied by the mounting layer and Fabric, as for any view                                                                                                                                                                |
| `Image.prefetch`, `clearMemoryCache`, `clearDiskCache`, `getCachePathAsync` | forwarded to the renderer's `ImageLoader`, so they act on the cache the view reads; `getCachePathAsync` returns null for a key that is not on disk                                                                       |
| `priority`, `enableLiveTextInteraction`                                     | accepted; no effect (one request at a time per view, no Live Text)                                                                                                                                                       |
| `Image.generateBlurhashAsync`, `loadAsync`, cache read and write functions  | absent (calling one throws "is not a function")                                                                                                                                                                          |
| `startAnimating`, `stopAnimating` on a ref                                  | not reachable: the Expo module cannot address a view instance on this host. Use `autoplay`                                                                                                                               |
