# Render `react-native-svg` elements

Stage 4 of the [v2 plan](README.md). With `RNQNX_ENABLE_QT_RNSVG=ON` the host
mounts the unmodified `react-native-svg` 15.16.0 package, the version Expo SDK
58 pins in `bundledNativeModules.json`. Its own Fabric components
(`RNSVGSvgView`, `RNSVGPath`, `RNSVGGroup` and the rest) reach a Qt Quick
renderer, so an app imports `react-native-svg` as it does anywhere else:

```tsx
import Svg, { Defs, LinearGradient, Stop, Circle, Rect } from 'react-native-svg';

<Svg width={200} height={120} viewBox="0 0 200 120">
  <Defs>
    <LinearGradient id="g" x1="0" y1="0" x2="1" y2="0">
      <Stop offset="0" stopColor="#79e8dc" />
      <Stop offset="1" stopColor="#2f80ed" />
    </LinearGradient>
  </Defs>
  <Rect width="200" height="120" rx="12" fill="url(#g)" />
  <Circle cx="100" cy="60" r="30" fill="#fff" onPress={() => console.log('tapped')} />
</Svg>;
```

This is separate from [`QtSvgDocument`](svg-component-api.md), which renders one
static XML string and has no element tree, updates or touch targets.

## Enable it

```sh
pnpm install --frozen-lockfile        # installs react-native-svg 15.16.0
cmake --preset <preset> -DRNQNX_ENABLE_QT_RNSVG=ON
```

The switch defaults to OFF and needs only Qt Core, Gui and Quick. It does not
link `Qt6::Svg`. Importing `react-native-svg` in a host built without it fails
the first time a component renders, with React Native's own unknown-component
error for `RNSVGSvgView`.

`pnpm qt:rnsvg:check` regenerates the Fabric props and events from the package's
TypeScript specs with the pinned React Native codegen and fails on drift. The
shadow nodes and the layout rule that keeps every element at the full size of its
`<Svg>` come from the package's own `common/cpp`, compiled from `node_modules`.

## How it renders

Each `react-native-svg` element is an ordinary Fabric view, so Fabric's
create, insert, remove and delete mutations mount a `RnsvgElementItem`: a
non-visual `QQuickItem` that keeps the element's parsed props. `<Svg>` mounts a
`RnsvgRootItem`, a `QQuickPaintedItem` that walks its element children with
`QPainter` and hands Qt one texture. The scene graph therefore holds one textured
node per `<Svg>` however many elements it contains.

- A prop change replaces one element's retained props and schedules one repaint
  of its root. Nothing is remounted, so `Path` `d`, `Circle` `r`, gradient stops
  and fills can change every frame.
- Qt coalesces repaint requests, so several elements changing in one commit paint
  once.
- Unmounting a subtree deletes its items; deleting the `<Svg>` deletes the whole
  tree.
- `RNQNX_RNSVG_TRACE=1` logs the time of the first five full repaints and of every
  60th after that (`RNQNX_RNSVG_PAINT`).

Why QPainter and not Quick Shapes or `QtSvgDocument`: see
[the renderer choice](#renderer-choice).

### Inheritance

react-native-svg sends the names of the props an element set itself (`propList`).
An element takes those from itself and every other paint prop (`fill`, `stroke`,
`strokeWidth`, dashes, caps, joins, opacities, fill rule) from the nearest
ancestor that listed it, falling back to SVG's defaults. `opacity`, `matrix`,
`clipPath` and `mask` are never inherited. `currentColor` resolves to the nearest
`color`.

### Touch targets

`onPress`, `onPressIn`, `onPressOut`, `onLongPress` and the responder props on
an element set `responsible`. When a press lands on an `<Svg>`, the touch
dispatcher asks it for the topmost responsible element under the point (fill, and
stroke when stroked, through the element's transforms) and delivers the touch to
that element's own Fabric tag. A `<G>` that is responsible receives presses on any
of its children that are not responsible themselves. Elements that are not
responsible, `pointerEvents="none"` elements and empty areas fall through to the
`<Svg>` view.

## Supported

| Element                             | Status    | Notes                                                                                                                                                                         |
| ----------------------------------- | --------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `Svg`                               | partial   | `viewBox` with `preserveAspectRatio` (meet, slice, none and the nine alignments), Yoga sizing, `style.backgroundColor`                                                        |
| `Path`                              | supported | `M L H V C S Q T A Z`, absolute and relative; the valid prefix of malformed data is drawn                                                                                     |
| `Rect`, `Circle`, `Ellipse`, `Line` | supported | Numbers, percentages and `px`, `pt`, `pc`, `mm`, `cm`, `in`, `em` lengths                                                                                                     |
| `Polygon`, `Polyline`               | supported | The package renders both as `Path`                                                                                                                                            |
| `G`                                 | supported | Matrix, opacity applied to the group as a unit, inheritance, clip, mask, `onPress`                                                                                            |
| `Defs`                              | supported | Children paint only when referenced                                                                                                                                           |
| `LinearGradient`, `RadialGradient`  | supported | `objectBoundingBox` and `userSpaceOnUse`, `gradientTransform`, per-stop alpha, radial focus                                                                                   |
| `ClipPath`                          | partial   | Union of shapes, groups and `Use`; always nonzero (see below); `Text` inside a clip path contributes nothing                                                                  |
| `Mask`                              | supported | Luminance and alpha masks, both unit systems, gradient and shape content                                                                                                      |
| `Use`                               | partial   | References any element by id with `x`, `y`; `width` and `height` are ignored because `Symbol` is unsupported; recursion is bounded                                            |
| `Text`, `TSpan`                     | partial   | Cursor positioning, first `x`, `y`, `dx`, `dy` value, `textAnchor`, `fontSize`, `fontWeight`, `fontStyle`, `fontFamily`, `letterSpacing`, fill and stroke including gradients |
| `Image`                             | partial   | `data:` URIs, `file://` and absolute paths, `preserveAspectRatio`; bundled assets and HTTP(S) are not loaded                                                                  |
| `Stop`                              | supported | Folded into its gradient by the package; has no native view                                                                                                                   |

### Not supported

These fail the way React Native fails for any unregistered component, so a
missing feature is never a silent blank:

`Pattern`, `Symbol`, `Marker` (and `markerStart/Mid/End`), `TextPath`, `Filter`
and every `Fe*` primitive, `ForeignObject`, and the Android-only view.

Smaller gaps inside supported elements:

| Gap                                                                                                                              | Behavior                                                                                                                                     |
| -------------------------------------------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------- |
| `clipRule="evenodd"`                                                                                                             | Clip paths are always nonzero: the generated `clipRule` default and `evenodd` are both 0                                                     |
| Per-glyph `x`/`y`/`dx`/`dy`/`rotate` lists, `textLength`, `lengthAdjust`, `baselineShift`, `alignmentBaseline`, `textDecoration` | Only the first position value is used; the rest are ignored                                                                                  |
| `textAnchor` across several `TSpan`s in one chunk                                                                                | Each span anchors on its own width                                                                                                           |
| Hit testing                                                                                                                      | Ignores `clipPath`, `mask` and `Image` pixel transparency (an image hits as its rectangle)                                                   |
| `vectorEffect="non-scaling-stroke"`                                                                                              | Uses a cosmetic pen, which ignores device pixel ratio scaling                                                                                |
| `onLayout` on elements (`onSvgLayout`)                                                                                           | Not emitted                                                                                                                                  |
| `toDataURL`, `getBBox` and the other native view commands                                                                        | Not implemented                                                                                                                              |
| Filters on `Image`, `context-fill`, `context-stroke`                                                                             | `context-*` paints draw nothing                                                                                                              |
| Group opacity and masks                                                                                                          | Each draws into a layer the size of the whole `<Svg>` texture, so many of them in one `<Svg>` cost one full-texture clear and composite each |
| Work bound                                                                                                                       | A paint or hit test stops after 200000 element visits; an element that reaches itself through `Use` or a mask is skipped                     |
| Raster size                                                                                                                      | Capped at 4096 physical pixels per side, like `QtSvgDocument`; larger views look softer                                                      |

Gradient stops carry packed ARGB integers. React Native's 32-bit `Float` cannot
hold one with alpha below 255 (it drops the blue channel), so the generator parses
the `gradient` prop as doubles. That is the only deviation from the package's
own generated props.

## Animated props

There is no Worklets or Reanimated path in this host yet (stage 2 and 3 are
unmerged), so props are driven from JavaScript: a timer, `requestAnimationFrame`,
React Native `Animated` through `setState`, or any state library re-renders the
element with new props and the retained tree repaints. The gallery's rotating
group does this at 20 updates per second. `Animated.Value` objects passed straight
into SVG props (`createAnimatedComponent(Circle)`) use the same `setNativeProps`
path as on other platforms; that path is not covered by a test here.
Reanimated's `useAnimatedProps` on SVG elements needs the stage 2 and 3 runtime
and is not claimed.

## Renderer choice

Three Qt-backed designs were considered. The decision rests on what each can
express against what this package sends and on a measurement, not on preference.

| Candidate                         | What it cannot do here                                                                                                                                                                                           |
| --------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `QtSvgDocument` (`QSvgRenderer`)  | Whole-document XML only: no per-element props, no hit testing, no `Use`/mask/text support beyond Qt SVG Tiny, and every prop change would re-serialize and re-parse the document                                 |
| Qt Quick Shapes                   | Fills and strokes of paths only. No luminance masks, no clip by arbitrary shape inside a group, no text outlines, no per-element opacity layers, and a software or GL-curve-renderer dependency per element item |
| `QPainter` in `QQuickPaintedItem` | Everything above, with one texture per `<Svg>`. Costs a CPU repaint of the whole `<Svg>` when anything in it changes                                                                                             |

The measured repaint cost on the Pi 400 is about 2 ms for a six-element `<Svg>`
with two props changing per frame (see [validation](validation.md#react-native-svg)).
The QPainter raster is CPU work on the GUI thread's paint pass, so it scales with
the pixel area and element count of the one `<Svg>` that changed.

## Validation

See [validation](validation.md#react-native-svg) for commands and results.
