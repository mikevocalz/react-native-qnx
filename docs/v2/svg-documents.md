# Render a static SVG document

This opt-in v2 component renders a bounded, static SVG document through the
existing Qt Quick backend. It accepts inline XML, basic vector shapes, groups,
and local gradients. It does not implement the `react-native-svg` element tree,
SVG scripting, embedded images, text layout, or per-element touch handling.
QNX and Marathon device qualification are separate from the host tests below.

## API contract defined before implementation

The native interface is `rnqnx::views::QtSvgItem` in
`native/qt/svg/QtSvgItem.h`. Construct, mutate, query, and destroy it on the Qt
GUI owner thread. The host owns the item through normal Qt parent ownership.
Construction on a worker fails. Changing QObject thread affinity invalidates
the item and releases its document; moving it back does not make it reusable.

```cpp
QtSvgItem item;
item.setSize(QSizeF(120, 80));
item.setResizeMode(SvgResizeMode::Contain);
if (!item.setDocument(R"(<svg xmlns="http://www.w3.org/2000/svg"
    viewBox="0 0 120 80"><rect width="120" height="80"
    fill="#2563eb"/></svg>)")) {
  qWarning() << item.errorString();
}
// Cached metadata is available only after a successful load.
const auto info = item.documentInfo();
item.clearDocument();
item.clearDocument(); // Repeated cleanup has no additional effect.
```

`setDocument(QByteArrayView)` returns `true` after a successful load or an empty
source clears the item. It returns `false` for malformed, unsupported, or
oversized input. A rejected replacement clears the previous document and
pixels; it cannot keep displaying a stale success. `documentState()` is
`Empty`, `Ready`, or `Error`. `documentInfo()` contains the intrinsic size and
view box only in `Ready`; `errorString()` contains a diagnostic only in `Error`.

The input view is borrowed only for the duration of the call. Oversized input
is rejected before reading or copying its bytes. A new bounded source is copied
into item-owned storage before parsing or caching, so the caller may immediately
mutate or release its buffer. `QByteArray` and string literals convert to the
view directly; the adapter can pass `QByteArrayView(xml.data(), xml.size())`
without making a preliminary copy.

The item emits one of `documentLoaded(QSizeF, QRectF)`,
`documentFailed(QString)`, or `documentCleared()` after an observable source
change. Reassigning the same bounded source is a no-op. Painting never emits
callbacks. There are no network requests, animation timers, or background
tasks to cancel; clear/unmount immediately release the owned renderer.

The React entry uses the Fabric component registered as `QnxSvgDocument`. The
Fabric adapter owns conversion of the serialized native error to the
public component's `onDocumentError` event. This C++ slice does not add a Nitro module,
change the pinned Nitro v1 ABI, or invent generated bindings.

```tsx
import { QtSvgDocument } from '@rn-qnx/react-native-qnx/qt';

<QtSvgDocument
  xml={'<svg viewBox="0 0 24 24"><circle cx="12" cy="12" r="10" fill="#2563eb"/></svg>'}
  resizeMode="contain"
  style={{ width: 48, height: 48 }}
  onDocumentError={(event) => reportAssetError(event.nativeEvent.message)}
/>;
```

```tsx
// Failure is observable and removes the previous image.
<QtSvgDocument
  xml={'<svg viewBox="0 0 24 24"><image href="https://example.com/image.png"/></svg>'}
  style={{ width: 48, height: 48 }}
  onDocumentError={(event) => setAssetError(event.nativeEvent.message)}
/>
```

```tsx
// Empty source clears; unmount releases the renderer.
{
  showLogo ? <QtSvgDocument xml={logoXml ?? ''} style={{ width: 120, height: 80 }} /> : null;
}
```

## Choose sizing

`Contain` centers the full view box inside the item. `Cover` fills the item and
clips overflow. `Stretch` maps the view box to the full item bounds. Yoga remains
the layout authority; the intrinsic size is metadata, not a second layout pass.
The outer item controls sizing; root SVG `preserveAspectRatio` is not accepted.

The renderer uses `QSvgRenderer` and a `QQuickPaintedItem` raster texture. It
rerenders when source or geometry changes, then Qt composites the texture.
The texture is capped at 4096 physical pixels per side, preserving aspect ratio;
larger views use that bounded raster and may look softer. This is native
document rendering, not a retained GPU path renderer or an animation guarantee.

## Prepare supported input

The source must be UTF-8 XML with one `<svg>` root. Supply a `viewBox` or
`width` and `height`. Root dimensions use user units or `px` and range from
0.000001 to 16384 per axis. View-box origins, numeric values, and composed
transform coefficients are finite and bounded to an absolute value of 1000000.
Empty source clears the item.

The document budget is 64 KiB, 512 elements, 32 nesting levels, 32 attributes
per element, and 16384 UTF-16 code units per attribute. A path has at most 4096
command/number tokens, and a transform list has at most 32 operations.
Source limits are checked before loading
Qt SVG. These are input limits, not measured rendering-time guarantees.

Supported elements: `svg`, `g`, `defs`, `path`, `rect`, `circle`, `ellipse`,
`line`, `polyline`, `polygon`, `linearGradient`, `radialGradient`, `stop`,
`title`, and `desc`. Use presentation attributes for fill, stroke, opacity,
transforms, and gradient stops. Paint may refer to a declared local gradient
with `url(#gradientId)`. Colors use Qt-recognized named or hexadecimal colors,
`currentColor`, and `none` for fill/stroke. Use numeric opacity from 0 to 1.

Unsupported elements or attributes fail the whole replacement with a diagnostic.
In particular, expand text to paths and remove CSS/style blocks, `style`
attributes, scripts, animation, `image`, `use`, external references, filters,
masks, `clipPath`, event handlers, DTDs, and custom namespaces before loading.
There is no implicit network or filesystem access. An accepted XML structure
is still interpreted by Qt SVG; this is not a browser SVG conformance layer.

## Native integration

Link `rnqnx_qt_svg` only behind `RNQNX_ENABLE_QT_SVG`, which defaults OFF. Its
sources depend on public `Qt6::Core`, `Qt6::Gui`, `Qt6::Quick`, and `Qt6::Svg`.
Connect source signals before applying props. Forward `xml` as UTF-8 bytes,
map the three resize modes, and call `clearDocument()` when recycling an item.
Do not emit application events from `paint()` or create a competing renderer.

The standalone host test is in `tests/native/qt-svg`. It validates actual
rendered pixel colors, crop/fit/stretch, metadata, resizing, replacement,
clear, invalid input, limits, and ownership. Its result does not establish
QNX, Wayland, Marathon-shell, or physical-input support.

With a qualified Qt 6.11.1 SDK on `CMAKE_PREFIX_PATH`, run the isolated test:

```bash
cmake -S tests/native/qt-svg -B build/qt-svg \
  -DRNQNX_ENABLE_QT_SVG=ON -DRNQNX_QT_SVG_SANITIZERS=ON \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build build/qt-svg
ctest --test-dir build/qt-svg --output-on-failure
```

The sanitizer option requires Linux with GCC or Clang. The CTest environment
uses offscreen Qt Quick software rendering; ASan and UBSan instrument the new
library and test executable. Qt SDK binaries are not instrumented. Leak
detection is disabled because this lane uses a prebuilt Qt process runtime;
ownership assertions and address/undefined-behavior checks remain enabled.

Native validation on 2026-11-10: the isolated Debug build passed with Qt 6.11.1
and GCC 14.2.0, including actual offscreen scene pixels and the adversarial
thread-affinity cases. The opt-in component still needs its separately recorded
Hermes/Fabric integration run and target device qualification. No frame-rate,
parse-time, GPU, or memory-budget performance measurement is claimed.

## Source and skills

Qt API sources: [QSvgRenderer](https://doc.qt.io/qt-6/qsvgrenderer.html),
[QQuickPaintedItem](https://doc.qt.io/qt-6/qquickpainteditem.html),
[SVG limits](https://doc.qt.io/qt-6.8/svgextensions.html), and
[Qt SVG module terms](https://doc.qt.io/qt-6.8/qtsvg-index.html).
Qt SVG is an optional dependency under its own LGPL/GPL/commercial terms;
repository-authored source remains MIT. No Qt SDK binaries are added here.

| Read source                                            | Revision                                   | Applied decision                                                                         |
| ------------------------------------------------------ | ------------------------------------------ | ---------------------------------------------------------------------------------------- |
| Margelo `api-design`                                   | `1e9e17be6f41c838db472d6a4b943fc26355a3be` | Define errors, replacement, clear, metadata, and call sites before C++                   |
| Margelo `build-nitro-modules`                          | same                                       | Keep resource ownership native; do not claim this Qt item is a generated Nitro v1 module |
| Margelo `cpp`                                          | same                                       | Separate validation from the owned renderer; RAII, typed states, owner-thread mutation   |
| Callstack `react-native-best-practices`                | `59e9d17446a248b21d86f078fcd0bd2d36db1ca4` | Test visible rendering; report performance as unmeasured                                 |
| Callstack `writing-user-docs`                          | same                                       | Put supported input, failure behavior, and usage before integration internals            |
| Repository `qnx-native-api`, `qnx-platform-validation` | working branch                             | Preserve Qt/Fabric, default-OFF rollout, separate host and device evidence               |

The skill source was read from pinned checkouts. CLI skill installation and
upstream QNX package support are not claimed. Qt Quick/Fabric engineering owns
this slice; independent runtime/Qt review and target validation remain required.

Rollback: disable the platform SVG option and remove SVG components from the
consuming app. The existing Image and Qt Quick renderer paths remain unchanged.
