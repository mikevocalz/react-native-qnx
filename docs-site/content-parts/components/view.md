## Props

`View` is React Native's own component. On QNX the Fabric `View` descriptor is registered in `vnext/src/runtime/QtReactHost.cpp`, and `toViewStyle` in `vnext/src/fabric/ShadowViewMutationAdapter.cpp` converts the resolved `ViewProps` into a `ViewStyle` that `QtQuickMountingLayer::updateStyle` applies to a Qt Quick item.

| Prop                                                                                                                     | Type          | Status                 | Source-backed note                                                                                                |
| ------------------------------------------------------------------------------------------------------------------------ | ------------- | ---------------------- | ----------------------------------------------------------------------------------------------------------------- |
| Yoga layout props (`flex`, `padding`, `position`, and so on)                                                             | style         | supported              | Yoga output is applied as absolute geometry by `updateLayout`.                                                    |
| `backgroundColor`                                                                                                        | color         | supported              | Packed to ARGB and set on the item's `backgroundColor` property.                                                  |
| `opacity`                                                                                                                | number        | supported              | `QQuickItem::setOpacity`.                                                                                         |
| `borderRadius` and per-corner radii                                                                                      | number        | supported              | Uniform radii use a `Rectangle`; differing corners use a Qt Quick `Shape` path.                                   |
| `borderWidth`, `borderColor`                                                                                             | number, color | supported when uniform | Non-uniform widths and colors collapse to the top edge.                                                           |
| `transform`                                                                                                              | array         | partial                | Only translate, scale and rotateZ operations are read.                                                            |
| `accessible`, `accessibilityLabel`, `accessibilityRole`, `accessibilityHint`, `accessibilityValue`, `accessibilityState` | various       | partial                | Forwarded as QML dynamic properties. The Qt accessibility bridge is a build option, default OFF.                  |
| `onLayout`                                                                                                               | function      | supported              | Emitted only when the prop is set.                                                                                |
| Responder props                                                                                                          | function      | supported              | React's responder negotiation runs over the touch events; covered by a host test per `docs/component-support.md`. |

## Events

| Event                                       | Status    | Note                                                                                                                                    |
| ------------------------------------------- | --------- | --------------------------------------------------------------------------------------------------------------------------------------- |
| `onLayout`                                  | supported | `ViewEventEmitter::onLayout` is called when the layout is applied and `props->onLayout` is set.                                         |
| `onTouchStart`, `onTouchMove`, `onTouchEnd` | partial   | `QtTouchDispatcher` hit-tests the item tree, ordering siblings by `z`. Single pointer from the mouse, multi-pointer from `QTouchEvent`. |
| `onResponderGrant`, `onResponderRelease`    | supported | Same path as the touch events.                                                                                                          |

## Platform behavior on QNX

Yoga is the only layout authority. Each View becomes a Qt Quick item whose position and size come from the Yoga frame, and `translateX` and `translateY` are added on top of that frame. See [threading](/docs/explanation/threading) for where mounting runs.

The View QML used by the playground host (`vnext/probes/playground/main.cpp`, `kViewSource`) sets `clip: true` on every View. That is the only registration of the View type I read, so clipping behavior in other hosts was not verified.

## Unsupported or partial props

- `transform` skew, perspective, `transformOrigin` and arbitrary matrices: the loop in `toViewStyle` handles Translate, Scale and Rotate and ignores every other operation type (`default: break`).
- Scale uses only the x factor (`op.x.value`), so non-uniform scale is not represented.
- `borderRadius` with `overflow`: children are clipped to the item rectangle, not the rounded shape (`docs/component-support.md`, View props table).
- Non-uniform `borderWidth` and `borderColor`: the code falls back to the top edge (`toViewStyle`, comments on uniform checks).
- Accessibility: with the bridge OFF there is no screen reader output. `updateStyle` can return `MountError::UnsupportedAccessibility` when the bridge is ON and a prop combination cannot be mapped.
- `overflow`, `pointerEvents`, `hitSlop`, `shadow*` and `elevation` on a plain View: No evidence found that `toViewStyle` or the mounting layer reads them.

## Minimal example

```tsx
import React from 'react';
import { View } from 'react-native';

export default function App() {
  return (
    <View
      style={{
        flex: 1,
        alignItems: 'center',
        justifyContent: 'center',
        backgroundColor: '#202830',
      }}
    >
      <View
        style={{
          width: 120,
          height: 60,
          borderRadius: 8,
          backgroundColor: '#3aa675',
          opacity: 0.9,
        }}
        onLayout={(event) => console.log(event.nativeEvent.layout)}
      />
    </View>
  );
}
```

## Known gaps

- Qt Quick's accessibility engine is not connected unless the bridge is enabled. Pi 400 evidence covers generic View semantic queries and activate delivery with semantic-label updates only. Qt action-list and custom-label parity, the RN disabled state, values and reader output are unqualified (see the notes above).
- Pi 4 QSTI has no run.
- Touch hardware (USB, touchscreen) input is listed as unverified in `docs/component-support.md`.
