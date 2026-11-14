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
shadow nodes and the layout rule that keep every element at the full size of its
`<Svg>` come from the package's own `common/cpp`, compiled from `node_modules`.

## How it renders

Each `react-native-svg` element is an ordinary Fabric view, so Fabric's
create, insert, remove and delete mutations mount a `RnsvgElementItem`: a
non-visual `QQuickItem` that keeps the element's parsed props. `<Svg>` mounts a
`RnsvgRootItem`, a `QQuickPaintedItem` that walks its element children with
`QPainter` and hands Qt one texture. The scene graph therefore holds one textured
node per `<Svg>` however many elements it contains.

The renderer is one class per kind of work under `native/qt/rnsvg/`:

| Directory or file | Work                                                                                                                                                                       |
| ----------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `scene/`          | The tree walk (`Scene`), inheritance, paint servers and patterns, clip and mask, markers, layers (`Effects`), hit testing, bounds, and the text, image and filter adapters |
| `text/`           | The text layout engine: chunks, per-character lists, anchors, `textLength`, baselines, decorations, `textPath`, bidirectional text                                         |
| `filter/`         | The filter engine: region math, colour spaces and all 17 primitives, one file each                                                                                         |
| `image/`          | Load state per `<Image>`, decoding, the provider interface the host fills in                                                                                               |

- A prop change replaces one element's retained props and schedules one repaint
  of its root. Nothing is remounted, so `Path` `d`, `Circle` `r`, gradient stops
  and fills can change every frame.
- Qt coalesces repaint requests, so several elements changing in one commit paint
  once.
- Group opacity, masks and filters draw into a layer the size of the element's
  painted extent (or its filter region), not of the whole `<Svg>`. A test asserts
  a 50x50 group in a 2000x2000 `<Svg>` allocates one layer under 60x60 pixels.
- Unmounting a subtree deletes its items; deleting the `<Svg>` deletes the whole
  tree.
- `RNQNX_RNSVG_TRACE=1` logs the time of the first five full repaints and of every
  60th after that (`RNQNX_RNSVG_PAINT`).

### Inheritance

react-native-svg sends the names of the props an element set itself (`propList`).
An element takes those from itself and every other paint prop (`fill`, `stroke`,
`strokeWidth`, dashes, caps, joins, opacities, fill rule, font) from the nearest
ancestor that listed it, falling back to SVG's defaults. `opacity`, `matrix`,
`clipPath`, `mask` and `filter` are never inherited. `currentColor` resolves to
the nearest `color`.

### Touch targets

`onPress`, `onPressIn`, `onPressOut`, `onLongPress` and the responder props on
an element set `responsible`. When a press lands on an `<Svg>`, the touch
dispatcher asks it for the topmost responsible element under the point and
delivers the touch to that element's own Fabric tag. The test follows the same
geometry the painter uses:

- fill and stroke, through the element's transforms and every ancestor's;
- `clipPath` (the point must be inside it) and `mask` (the mask must be above 0
  at the point), on the element and on every ancestor;
- `pointerEvents`: `none` (the subtree), `box-none` (children only), `box-only`
  (the bounding box, no children), and SVG's `visibleFill`, `visibleStroke`,
  `visible`, `fill`, `stroke`, `all`; `auto` and `visiblePainted` test the
  painted fill and stroke;
- a `<G>` that is responsible receives presses on any of its children that are
  not responsible themselves.

Elements that are not responsible, and empty areas, fall through to the `<Svg>`
view. The live views under a `<ForeignObject>` are ordinary Qt Quick items and
receive presses as any view does.

## Supported

Evidence column: the test that fails if the behavior breaks. `Render` is
`tests/native/qt-rnsvg/RnsvgRenderTest.cpp`, `Scene` is `RnsvgSceneTest.cpp`,
`Corpus` is the resvg-diffed fixture corpus (see
[the reference harness](rnsvg-reference.md)), `Filter`, `Text` and `Image` are the
engine tests of the same name.

| Element                             | Status    | Notes and evidence                                                                                                                                                                                                                                                                |
| ----------------------------------- | --------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `Svg`                               | supported | `viewBox` with `preserveAspectRatio` (meet, slice, none, nine alignments), Yoga sizing, `style.backgroundColor`, nested `<Svg>` painted inline with its own viewBox and clipped to its size, `onLayout`. Render, Corpus                                                           |
| `Path`                              | supported | `M L H V C S Q T A Z`, absolute and relative; the valid prefix of malformed data is drawn. Render, Corpus                                                                                                                                                                         |
| `Rect`, `Circle`, `Ellipse`, `Line` | supported | Numbers, percentages and `px`, `pt`, `pc`, `mm`, `cm`, `in`, `em` lengths. Render, Corpus                                                                                                                                                                                         |
| `Polygon`, `Polyline`               | supported | The package renders both as `Path`. Corpus                                                                                                                                                                                                                                        |
| `G`, `Defs`                         | supported | Matrix, group opacity as a unit, inheritance, clip, mask, filter, `onPress`. Render, Scene                                                                                                                                                                                        |
| `Use`                               | supported | References any element by id with `x`, `y`; on a `Symbol`, `width` and `height` set the viewport. Context paint for `context-fill` and `context-stroke`. Recursion is bounded. Scene, Corpus                                                                                      |
| `Symbol`                            | supported | Laid out through `Use` with its `viewBox` and `preserveAspectRatio`, clipped to the viewport, not drawn by itself. Scene                                                                                                                                                          |
| `LinearGradient`, `RadialGradient`  | supported | `gradientUnits`, `gradientTransform`, per-stop alpha, interpolated per component like SVG, radial focus including a focus outside the circle (the SVG 2 cone), a zero radius (the last stop's colour), `spreadMethod` pad, reflect, repeat (see the overlay below). Scene, Corpus |
| `Pattern`                           | supported | `patternUnits` and `patternContentUnits` in both systems, `viewBox`, `patternTransform`, fill and stroke use, nested patterns bounded at depth 3. Scene, Corpus                                                                                                                   |
| `ClipPath`                          | supported | Union of shapes, groups, `Use` and text; `clipRule` evenodd and nonzero; a clip path that is itself clipped. An unset `clipRule` clips as evenodd, which is what the package's native views do. Scene, Corpus                                                                     |
| `Mask`                              | supported | Luminance and alpha masks, both unit systems, gradient and shape content. Render, Corpus                                                                                                                                                                                          |
| `Marker`                            | supported | `markerStart`, `markerMid`, `markerEnd` on `Path` and `Line`; `orient` auto or an angle, `markerUnits`, `viewBox`, `refX`/`refY`; markers are not clipped to their viewport, as in the package's native views; `context-stroke` fill. Scene, Corpus                               |
| `Text`, `TSpan`                     | supported | See the text table below. Text, Corpus                                                                                                                                                                                                                                            |
| `TextPath`                          | supported | `href`, `startOffset` in px or percent, `side`, `method` align and stretch, `spacing`, `midLine`. Text, Corpus                                                                                                                                                                    |
| `Image`                             | partial   | `data:` URIs, `file://` and absolute paths, bundled assets and HTTP(S) through the host image loader, `preserveAspectRatio`, async load, `onLoad`. Not supported: SVG sources and uncached responses (see the limits below). Image                                                |
| `ForeignObject`                     | partial   | Live React Native views inside a box at `x`, `y`, `width`, `height`, clipped to it, following every ancestor transform. They draw above the painted texture (see the limits below)                                                                                                |
| `Filter` and the `Fe*` elements     | supported | All 17 primitives; see the filter table below. Filter, Corpus                                                                                                                                                                                                                     |
| `Stop`                              | supported | Folded into its gradient by the package; has no native view                                                                                                                                                                                                                       |

### Strokes and paint

| Attribute                                      | Evidence                                                                                                    |
| ---------------------------------------------- | ----------------------------------------------------------------------------------------------------------- |
| `fillRule`, `clipRule` evenodd                 | Render `fillRuleEvenOddPunchesHole`, Scene `clipRuleEvenOddOnTheReferencingElement`                         |
| `strokeDasharray`, `strokeDashoffset`          | Scene `dashesAlternateAlongTheStroke`, `dashOffsetShiftsThePattern`                                         |
| `strokeLinecap`, `strokeLinejoin`              | Scene `lineCapsExtendTheStroke`                                                                             |
| `strokeMiterlimit`                             | Scene `miterLimitBevelsSharpCorners` (Qt's `MiterJoin` ignores the limit; the renderer uses `SvgMiterJoin`) |
| `vectorEffect="non-scaling-stroke"`            | Scene `nonScalingStrokeKeepsItsWidthUnderAViewBox`: the width stays in item pixels under a viewBox          |
| `fill` and `stroke` as a gradient or a pattern | Corpus `Stroke-5`, `Pattern-1` to `-5`                                                                      |

### Text

Positions follow the SVG rules with react-native-svg's native views as the first
reference; where they disagree the choice is in the table.

| Feature                                                                                                           | Behavior                                                                                                                                                |
| ----------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `x`, `y`, `dx`, `dy`, `rotate` lists                                                                              | Entry i applies to the i-th character of the element's subtree; later characters keep the last `dx`/`dy` and the last `rotate` repeats                  |
| `textAnchor`                                                                                                      | Applies per text chunk across spans; an absolute `x` or `y` starts a new chunk                                                                          |
| `letterSpacing`, `wordSpacing`, `kerning`                                                                         | Letter spacing after every glyph, word spacing on U+0020 and U+00A0, kerning auto or a number                                                           |
| `fontFamily`, `fontSize`, `fontWeight`, `fontStyle`, `fontStretch`, `fontVariantLigatures`, `fontFeatureSettings` | Mapped to Qt fonts; the loaded font decides which weights and stretches exist, and a missing bold or italic is synthesised                              |
| `textLength`, `lengthAdjust`                                                                                      | Spacing hits the length exactly; spacingAndGlyphs also scales the outlines. The package never sends them; the overlay below does                        |
| `baselineShift`, `alignmentBaseline`, `verticalAlign`                                                             | Numeric, `sub`, `super` (from the font's OS/2 table), and the CSS baselines                                                                             |
| `textDecoration`                                                                                                  | Underline, overline, line-through and combinations, painted with the text's own fill and stroke                                                         |
| `inlineSize`                                                                                                      | Wraps a `TSpan`'s own content, as the package does                                                                                                      |
| Right-to-left text                                                                                                | Qt's shaper reorders each node; Hebrew and Arabic are tested. A phrase split across `TSpan`s is not reordered across them; `direction` is not supported |
| `side="right"`                                                                                                    | Reverses the path (SVG 2). The package's own arithmetic draws nothing useful                                                                            |

### Filters

The filter engine works on premultiplied ARGB images in the filter region,
converts to `linearRGB` (the SVG default) or `sRGB` per primitive from
`color-interpolation-filters`, and scales lengths with the element's transform.
Every primitive has a golden image from resvg 0.50.0 in
`tests/native/qt-rnsvg/golden/filter/` and a stated tolerance.

| Primitive                                                                                                     | Notes                                                                                                                                    |
| ------------------------------------------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------- |
| `feGaussianBlur`, `feOffset`, `feFlood`, `feMerge`, `feBlend`, `feComposite`, `feColorMatrix`, `feDropShadow` | Edge modes none, duplicate and wrap; all blend modes; the arithmetic composite; saturate above 1 over-saturates (Filter Effects Level 1) |
| `feMorphology`, `feConvolveMatrix`, `feTile`, `feComponentTransfer`, `feImage`                                | `feImage` takes a `data:` URL, a local file or an element reference                                                                      |
| `feTurbulence`, `feDisplacementMap`                                                                           | The reference noise code from the spec; the output matches resvg to within one level                                                     |
| `feDiffuseLighting`, `feSpecularLighting`                                                                     | Distant, point and spot lights                                                                                                           |

Limits that are part of the design:

- `BackgroundImage` and `BackgroundAlpha` are transparent. `FillPaint` and
  `StrokePaint` are the element's flat colour over the region.
- A filter region larger than 8 million device pixels is cut to the visible area
  plus a 1024 px margin, which gives the same pixels for any filter that does not
  read from further away.
- A rotated or skewed transform runs the filter on a device-aligned canvas;
  blur deviations use the per-axis extent, which is exact for scales,
  quarter-turn rotations and flips and approximate at other angles.
- `lighting-color` is converted into the working colour space, as browsers do;
  resvg uses it as it is.

## What the package never sends, and the overlay

react-native-svg 15.16.0 does not forward several features to its host, and its
files cannot be edited in `node_modules`. The package itself returns `null` from
ten filter elements and drops three props:

| Missing upstream                                                                                                                                               | Evidence                                                                                                                 |
| -------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------ |
| `FeMorphology`, `FeTurbulence`, `FeDisplacementMap`, `FeComponentTransfer`, `FeImage`, `FeConvolveMatrix`, `FeTile`, `FeDiffuseLighting`, `FeSpecularLighting` | `src/elements/filters/*.tsx` `render()` calls `warnUnimplementedFilter()` and returns `null`; no Fabric component exists |
| `FeDropShadow` offsets                                                                                                                                         | The package builds it from blur, offset and flood with `dx`/`dy` defaulting to 0 where SVG says 2                        |
| `spreadMethod` on gradients                                                                                                                                    | `extractGradient` does not forward it and the native specs have no such prop                                             |
| `textLength`, `lengthAdjust` on `Text` and `TSpan`                                                                                                             | `extractText` drops them although the native spec declares both                                                          |
| `color-interpolation-filters`                                                                                                                                  | No native prop                                                                                                           |

On the `qnx` platform the Metro configuration serves replacement modules from
`packages/react-native-qnx/svg/overlay/`, which mirrors the package's `src/` tree
(`overlay/elements/filters/FeTurbulence.tsx` replaces
`react-native-svg/src/elements/filters/FeTurbulence.tsx`). The overlay files wrap
the originals where they can, add Fabric specs for the new components
(`packages/react-native-qnx/svg/fabric/`), and the code generator
(`scripts/qt-rnsvg-codegen.mjs`) adds the missing props to the package's own
specs. Nothing in `node_modules` changes, other platforms bundle the package
as shipped, and an app keeps writing `import { FeMorphology } from
'react-native-svg'`. `tests/js/rnsvg-overlay-resolver.test.mjs` and
`rnsvg-overlay-render.test.mjs` bundle through the real Metro configuration and
check each replaced component's native name and props.

The code generator also gives `clipRule` the default -1 instead of 0. The package
maps `evenodd` and "unset" to the same 0, which would make an explicit `evenodd`
on a shape indistinguishable from nothing.

## Limits with their cause

Each of these has a cause that can be shown; none is a guess.

| Limit                                                                                                        | Cause and evidence                                                                                                                                                                                                                                                                                                                                                                                                                  |
| ------------------------------------------------------------------------------------------------------------ | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| An `<Image>` whose source is an SVG document                                                                 | Qt decodes SVG through its `qsvg` image-format plugin, which is part of the QtSvg module. The public QNX Qt build has three submodules (`scripts/qt-public/configure-qnx.sh`: `qtbase,qtshadertools,qtdeclarative`) and the staged runtime ships only the JPEG, GIF and qtimageformats plugins (`stage-runtime.py`). The renderer does not link `Qt6::Svg`. On the host the plugin is present and the load works (`RnsvgImageTest`) |
| An `<Image>` whose HTTP response is `no-store` or `private`, or has request headers                          | `QtImageCache::Completion` carries a size, not pixels; pixels exist only in the shared cache, and `decode()` does not insert those responses. The fix is a completion that carries the `QImage`, in the loader another change is rewriting                                                                                                                                                                                          |
| `<Image>` `onError`                                                                                          | The package's native component declares `onLoad` and `onSvgLayout` only (`src/fabric/ImageNativeComponent.ts`)                                                                                                                                                                                                                                                                                                                      |
| `ForeignObject` children draw above the painted texture                                                      | Live views are scene-graph items; the `<Svg>` is one texture. A painted element after a `ForeignObject` in document order therefore appears behind it. Fixing it means splitting the texture around each `ForeignObject`                                                                                                                                                                                                            |
| Bidirectional text across `TSpan`s, `direction`, `unicode-bidi`                                              | The engine shapes each node with Qt's `QTextLayout`; the package sends neither prop to its native views                                                                                                                                                                                                                                                                                                                             |
| `toDataURL`, `getBBox` and the other native view commands                                                    | Not implemented. `onSvgLayout` (the element's rectangle in the `<Svg>`) is                                                                                                                                                                                                                                                                                                                                                          |
| `<gradient xlink:href>` inheritance                                                                          | `LinearGradient` and `RadialGradient` have no `href` prop in the package, so `SvgXml` documents that rely on it lose their stops                                                                                                                                                                                                                                                                                                    |
| Tags `SvgXml` does not map: `a`, `switch`, `view`, `title`, `desc`, `metadata`, `animate*`, `set`            | `src/xmlTags.ts` has no entry, so the parser drops them                                                                                                                                                                                                                                                                                                                                                                             |
| XML entities, and `style` values containing `;` such as `url(data:...;base64,...)`                           | `src/xml.tsx` does not decode entities and splits `style` on `;`                                                                                                                                                                                                                                                                                                                                                                    |
| `visibility`, `paint-order`, `mix-blend-mode`, `shape-rendering`, `overflow`, `transform-origin` and similar | The package's `extractProps` does not read them, so no host receives them                                                                                                                                                                                                                                                                                                                                                           |
| Work bound                                                                                                   | A paint or hit test stops after 200000 element visits; an element that reaches itself through `Use`, a mask or a pattern is skipped                                                                                                                                                                                                                                                                                                 |
| Raster size                                                                                                  | Capped at 4096 physical pixels per side, like `QtSvgDocument`; larger views look softer                                                                                                                                                                                                                                                                                                                                             |

## Animated props

`Animated` with `useNativeDriver: false`, a timer, `requestAnimationFrame` or any
state library re-renders the element with new props and the retained tree
repaints. `Animated.Value` objects passed into SVG props
(`createAnimatedComponent(Circle)`) use the package's `setNativeProps` path.
Reanimated's `useAnimatedProps` needs the Worklets runtime, which is not merged
(pull request 149 is an open draft), so it is not claimed.

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
