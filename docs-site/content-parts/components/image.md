## Props

`Image` is overridden in `packages/react-native-qnx/Libraries/Image/Image.js`. It resolves `source`, `defaultSource` and `loadingIndicatorSource` with React Native's `resolveAssetSource`, then renders `ImageViewNativeComponent`. `ShadowViewMutationAdapter::applyImage` reads the result and sets properties on a Qt Quick `Image` item.

| Prop                                            | Type                     | Status    | Source-backed note                                                                                    |
| ----------------------------------------------- | ------------------------ | --------- | ----------------------------------------------------------------------------------------------------- |
| `source`                                        | uri object, asset, array | partial   | Only `sources.front().uri` is used. Later list entries are ignored.                                   |
| `defaultSource`                                 | source                   | supported | Used as the URI only when `source` is empty.                                                          |
| `loadingIndicatorSource`                        | source                   | supported | Shown by a child `Image` item while the main source loads, hidden on Ready or Error.                  |
| `resizeMode`                                    | string                   | partial   | `cover` maps to `PreserveAspectCrop`, `contain` to `PreserveAspectFit`, everything else to `Stretch`. |
| `style`                                         | style                    | supported | Width and height come from Yoga like any View.                                                        |
| `onLoadStart`, `onLoad`, `onLoadEnd`, `onError` | function                 | supported | See Events.                                                                                           |

## Events

| Event         | Status    | Note                                                                                                        |
| ------------- | --------- | ----------------------------------------------------------------------------------------------------------- |
| `onLoadStart` | supported | Fired when QML `Image.status` reaches Loading, or just before Ready or Error if Loading was never observed. |
| `onLoad`      | supported | Fired on Ready, with the source.                                                                            |
| `onLoadEnd`   | supported | Fired on Ready, and on Error before `onError`.                                                              |
| `onError`     | supported | Carries QML's `errorString`, or `Could not load <url>` when that is empty (`QtImageBridge.cpp`).            |

## Platform behavior on QNX

Qt Quick's `Image` cannot read `data:` URIs, so `QtQuickItemFactory::resolveImageSource` serves them through `QnxImageProvider` as `image://rnqnx/...`. The host test checks that a data URI reaches `Image.Ready` at the styled size. HTTP(S) sources that are not prefetched go to Qt Quick directly. Prefetched URIs are served from the shared decoded cache (32 MiB LRU, per `docs/image-loader.md`) without a second download.

The statics `Image.getSize`, `getSizeWithHeaders`, `prefetch`, `prefetchWithMetadata` and `queryCache` call the QNX ImageLoader module. Requests time out after 30 seconds, the encoded limit is 16 MiB, and decode rejects images over 16 million pixels. Decoding depends on the Qt image plugins staged with the app. Percent-encoded data URIs are explicitly unsupported (`docs/image-loader.md`).

## Unsupported or partial props

- `resizeMode` values `center` and `repeat`: the switch in `applyImage` has no case for them, so they render as `stretch`.
- `tintColor`, `blurRadius`, `fadeDuration`, `capInsets`, `onProgress`, `onPartialLoad`: No evidence found that the plain `Image` path reads them. The `tintColor` and `blurRadius` code in `QtExpoImageBridge.cpp` belongs to the separate `expo-image` view.
- Source `headers` and `cache` fields on the component: `docs/component-support.md` records header support for the loader statics only. No evidence found for the component.
- `QtImageBridge.cpp` sets its start and end flags once and has no reset in that file. Whether a changed `source` fires events again was not verified.

## Minimal example

```tsx
import React from 'react';
import { Image } from 'react-native';

const pixel =
  'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR4nGMwTpv5HwAENAIyWy0K4AAAAABJRU5ErkJggg==';

export default function App() {
  return (
    <Image
      source={{ uri: pixel }}
      style={{ width: 96, height: 96 }}
      resizeMode="cover"
      onLoad={() => console.log('loaded')}
      onError={(e) => console.log(e.nativeEvent.error)}
    />
  );
}
```

## Known gaps

- No Pi 400 receipt exists for `Image`. The 2026-11-13 loader receipts in `docs/image-loader.md` cover the loader statics on a Pi 400, and that file says the later worker and cache-clear changes still need their own target receipt.
- Pi 4 QSTI has no run.
- `docs/component-support.md` marks `http(s):` component sources as partial.
