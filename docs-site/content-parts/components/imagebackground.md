## Props

`ImageBackground` is overridden in `packages/react-native-qnx/Libraries/Image/ImageBackground.js`. React Native's version relies on `StyleSheet.absoluteFill` and an image with intrinsic size. The file comment says Qt Quick's `Image` works more reliably with explicit width and height, so the override builds the same View, Image and children structure with a simpler style.

| Prop                                                                                   | Type        | Status    | Source-backed note                                                                                  |
| -------------------------------------------------------------------------------------- | ----------- | --------- | --------------------------------------------------------------------------------------------------- |
| `style`                                                                                | style       | supported | Applied to the outer `View` and merged into the image style.                                        |
| `imageStyle`                                                                           | style       | supported | Merged over `style` for the inner `Image` (`Object.assign({}, style, imageStyle)`).                 |
| `imageRef`                                                                             | ref         | supported | Set as `ref` on the inner `Image`.                                                                  |
| `children`                                                                             | node        | supported | Rendered in a second `View` filled with `StyleSheet.absoluteFill`, above the image.                 |
| `source`, `defaultSource`, `loadingIndicatorSource`, `resizeMode`, `onLoad`, `onError` | image props | supported | Passed through to `Image`. See the [Image page](/docs/reference/components/image) for their limits. |

## Events

`ImageBackground` adds no events. `onLoadStart`, `onLoad`, `onLoadEnd` and `onError` pass through to the inner `Image` and behave as described there.

## Platform behavior on QNX

The tree is a `View` (`collapsable: false`) containing an `Image` and an absolute-fill `View` that holds the children. Because `style` is copied onto the image, the image takes the container's width and height, so a container with explicit dimensions paints the picture at that size. Layout is Yoga, as for any View, and mounting runs on the Qt GUI thread.

Because the style is copied wholesale, any style prop on the container, including padding, margin and border, is also copied onto the image. The file does not strip them. Nothing in the source says whether that causes a visible difference, and no test was found.

## Unsupported or partial props

- Props that `Image` ignores are ignored here too: `resizeMode` values `center` and `repeat` render as `stretch`, and only the first entry of a `source` array is used (`ShadowViewMutationAdapter.cpp`, `applyImage`).
- No evidence found that the override handles `imageStyle` differently from the upstream rule of applying it only to the image. In this file `style` is applied to the image as well.

## Minimal example

```tsx
import React from 'react';
import { ImageBackground, Text } from 'react-native';

const pixel =
  'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR4nGMwTpv5HwAENAIyWy0K4AAAAABJRU5ErkJggg==';

export default function App() {
  return (
    <ImageBackground
      source={{ uri: pixel }}
      resizeMode="cover"
      style={{ width: 240, height: 120, justifyContent: 'center', alignItems: 'center' }}
    >
      <Text style={{ color: '#ffffff', fontSize: 20 }}>Over the image</Text>
    </ImageBackground>
  );
}
```

## Known gaps

- No dedicated test or evidence file is linked. The status in the generated table rests on the component notes ("Composes View and Image; paints the image behind children and forwards resizeMode") and on the override source, not on a recorded run of `ImageBackground` itself.
- No Pi 400 receipt. Pi 4 QSTI has no run.
- A container that relies on content size rather than explicit width and height was not tested. The file comment is the only guidance on that case.
