# Render an inline SVG document

`QtSvgDocument` is an experimental Fabric component backed by Qt SVG. Build the
host with `RNQNX_ENABLE_QT_SVG=ON` and install Qt 6.11.1's SVG module. Import the
component from `@rn-qnx/react-native-qnx/qt`.

```tsx
import { QtSvgDocument } from '@rn-qnx/react-native-qnx/qt';

const badge =
  '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 32 32"><circle cx="16" cy="16" r="14" fill="#2563eb"/></svg>';

<QtSvgDocument xml={badge} resizeMode="contain" style={{ width: 64, height: 64 }} />;
```

Yoga owns the view's geometry. Set its size with `style`; the document's intrinsic
size is reported by `onDocumentLoad` but does not change Yoga layout. Resizing the
view redraws the document. `contain` preserves aspect ratio with letterboxing,
`cover` preserves aspect ratio and clips, and `stretch` fills the view.

## Handle a rejected replacement

```tsx
<QtSvgDocument
  xml={selectedDocument}
  style={{ width: 160, height: 120 }}
  onDocumentLoad={({ nativeEvent }) => setSourceSize([nativeEvent.width, nativeEvent.height])}
  onDocumentError={({ nativeEvent }) => setError(nativeEvent.message)}
/>
```

Invalid or unsupported XML clears the previous image and emits
`onDocumentError`. See [the accepted document subset](svg-documents.md). No network
request or external file read is performed. Passing an empty string clears the
document without an error. Updating only layout or `resizeMode` does not reload
an unchanged source or repeat its load event.

Unmount the component to release its native document. There is no separate
resource handle to dispose. Importing the package does not turn on the native
feature: rendering it in a host built without SVG support throws an explicit
availability error.

This API renders a complete static SVG document. `react-native-svg` element
components, element hit testing, animated path properties and SVG animation need
their own adapter. Qt's painted-item path rasterizes into a texture; this API does
not promise GPU vector tessellation.

## Reproduce its Fabric declarations

Run `pnpm qt:svg:generate` after changing the native component's TypeScript spec,
then `pnpm qt:svg:check`. Both use the installed, pinned React Native codegen
0.88.1, with deterministic trailing-whitespace removal. The generated C++ lives under
`vnext/generated/react/renderer/components/RnqnxQtSvg`. This is a Fabric component
contract; application service modules continue to require Nitro v1.
