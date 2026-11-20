## Props

`Text` is React Native's Fabric `Paragraph` descriptor (plus the nested `Text` and `RawText` descriptors), registered in `vnext/src/runtime/QtReactHost.cpp`. The adapter maps `Paragraph` to a Qt Quick `Text` item. Measuring uses a QNX `TextLayoutManager` (`vnext/src/text/QtTextLayoutManager.cpp`) built on `QTextLayout`, because React Native's generic C++ manager returns the minimum size and would collapse every Text.

| Prop                                            | Type   | Status    | Source-backed note                                                                                                                    |
| ----------------------------------------------- | ------ | --------- | ------------------------------------------------------------------------------------------------------------------------------------- |
| children (strings, nested `Text`)               | node   | supported | Fragments are flattened onto one item (`paragraphTextStyle`).                                                                         |
| `numberOfLines`                                 | number | supported | Sets `maximumLineCount`; 1 uses `NoWrap`.                                                                                             |
| `ellipsizeMode`                                 | string | supported | `head`, `middle`, `tail`, `clip` map to Qt elide modes, applied only when a line limit is set.                                        |
| `style.fontSize`                                | number | supported | Applied as pixel size.                                                                                                                |
| `style.color`                                   | color  | supported | Applied from the first fragment, or per span in rich text.                                                                            |
| `style.fontFamily`                              | string | supported | Resolved through `resolveFontFamily`; bundled fonts are registered from `assets/fonts` per `docs/component-support.md`.               |
| `style.fontWeight`, `style.fontStyle: 'italic'` | style  | partial   | Used when measuring and in rich-text spans for mixed styling. The single-style path in `updateText` sets only size, family and color. |

## Events

No Text-specific event is bridged. No evidence found that `onTextLayout` or `onPress` is wired in the adapter or mounting layer. Responder and touch events follow the same dispatcher as `View` (`QtTouchDispatcher`).

## Platform behavior on QNX

Text is measured against the available width with `QTextLayout`, and the Qt item is set to `Text.Wrap` so the painted lines match the measured height (comment in `QtQuickMountingLayer::updateText`). When more than one styled fragment exists, the paragraph is sent as Qt rich text, because a Qt Quick `Text` applies one font and color to the whole item. Plain strings use `PlainText`, so markup characters are never interpreted. Mounting runs on the Qt GUI thread; see [threading](/docs/explanation/threading).

## Unsupported or partial props

- Inline attachments (views inside Text): `QtTextLayoutManager.cpp` reports them as zero-sized. The comment states this platform has no attachment support.
- `elide` without `numberOfLines`: `updateText` only sets elide inside the one-line and multi-line branches.
- `textAlign`, `lineHeight`, `letterSpacing`, `textDecorationLine`, `selectable`, `allowFontScaling`: No evidence found that `ShadowViewMutationAdapter.cpp`, `QtQuickMountingLayer.cpp` or `QtTextLayoutManager.cpp` read them (searched by name).
- A comment above `fragmentStyleAttribute` in `ShadowViewMutationAdapter.cpp` says per-fragment attributes are not yet carried onto the item. The code below it does carry size, color, weight, italic and family into rich-text spans, so the comment looks stale. The component notes say mixed styling works.

## Minimal example

```tsx
import React from 'react';
import { Text, View } from 'react-native';

export default function App() {
  return (
    <View style={{ padding: 16, width: 240 }}>
      <Text style={{ fontSize: 18, color: '#222222' }} numberOfLines={2} ellipsizeMode="tail">
        A long line of text that wraps and is cut at two lines with an ellipsis.
        <Text style={{ color: '#cc3300' }}> Nested spans use rich text.</Text>
      </Text>
    </View>
  );
}
```

## Known gaps

- Inline attachments have zero size.
- Pi 400 evidence is the hello-world receipt only. Wrapping, ellipsis and mixed styling on device are not separately evidenced.
- Pi 4 QSTI has no run.
- No evidence was found for text alignment, line height or letter spacing; treat them as unsupported until a test shows otherwise.
