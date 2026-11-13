# Image loader: host-verified, device-verified on a Pi 400

The core React Native `ImageLoader` adapter now performs real asynchronous
HTTP(S) work through Qt Network. It is an upstream core-module adapter, not a
new application TurboModule architecture.

## On a QNX device

Run on a Raspberry Pi 400 (QNX SDP 8.0.5, public Qt 6.12.0) on 2026-11-13,
with the gallery's "Image from a URL" card (`apps/component-gallery/RemoteImageCard.js`).
Each line is the card's own log output; the board reached the Mac over USB
Ethernet and the internet through the Mac.

| Source                                                         | Result on the Pi                                                                |
| -------------------------------------------------------------- | ------------------------------------------------------------------------------- |
| `https://images.unsplash.com/…?w=640` (public, JPEG)           | loaded in 338-688 ms; `Image.getSize` 640×427                                   |
| `http://httpbin.org/image/jpeg` (public, plain HTTP)           | loaded in 68-161 ms; 239×178                                                    |
| `http://192.168.2.1:8090/mac-served.jpg` (served from the Mac) | loaded in 72-82 ms; 640×360                                                     |
| `https://192.168.2.1:8443/…` (Mac, self-signed certificate)    | rejected: `SSL handshake failed: The certificate is self-signed, and untrusted` |
| `https://rnqnx-missing.invalid/photo.jpg`                      | rejected: `Host rnqnx-missing.invalid not found`                                |

What the device needed, beyond the Linux-tested loader:

- **Qt's TLS backend plugin.** Qt 6 loads TLS from `plugins/tls/`; the staged
  runtime had none, so every `https://` request failed. The OpenSSL backend
  (`libqopensslbackend.so`) is built from the public Qt source and loads the
  board's OpenSSL 3 (`/usr/lib/libssl.so.3`) at run time. Certificates come
  from the system store in `/etc/ssl/certs` (hashed), so public HTTPS works and
  untrusted certificates are refused.
- **Qt's JPEG and GIF image plugins.** Without `plugins/imageformats/` Qt Quick
  decodes only PNG; most photos on the web are JPEG.
- **Qt's WebP plugin** (and TIFF, TGA, ICNS, WBMP) from the `qtimageformats`
  module, built by `scripts/qt-public/build-imageformats-qnx.sh`. Without
  `libqwebp.so`, `QImageReader` reports no `webp` format and an animated or
  still WebP fails to load. An app carries `libqwebp.so` in
  `plugins/imageformats/` like the JPEG and GIF plugins.
- `scripts/qt-public/configure-qnx.sh` now builds all three and
  `stage-runtime.py` stages them. An app can also carry them in its own
  `plugins/` directory (`QT_PLUGIN_PATH=<app>/plugins:<runtime>/plugins`), which
  is how the Pi build here ships them.
- DNS: the board's `/etc/resolv.conf` points at the Mac (192.168.2.1), which
  forwards to the internet through macOS Internet Sharing. Nothing in the app
  is DNS-specific.
- Error text: `onError` now carries `Could not load <url>` (QML's Image has no
  error string), and the loader module reports Qt Network's reason instead of
  `HTTP 0` (for example `Image request failed: Host … not found`).

## Use the public Image API

In an app configured with the [public React Native entry](getting-started/qnx-cli.md),
import `Image` from `react-native`. Size lookups return an object with pixel
dimensions. A failed lookup rejects with an `Error`:

```js
import { Image } from 'react-native';

const uri =
  'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR4nGMwTpv5HwAENAIyWy0K4AAAAABJRU5ErkJggg==';

try {
  const { width, height } = await Image.getSize(uri);
  console.log(width, height); // 1 1

  await Image.prefetch(uri);
  const cached = await Image.queryCache([uri]);
  console.log(cached[uri]); // "memory" while the decoded image is retained
} catch (error) {
  console.error('Could not load the image', error);
}
```

For an HTTP(S) endpoint that needs headers, use
`Image.getSizeWithHeaders(uri, { 'X-Image-Variant': 'thumbnail' })`. Header values
must be strings. These requests do not reuse or populate the URI-only cache.

The React Native 0.88.1 callback overloads are also supported:
`Image.getSize(uri, (width, height) => { /* use dimensions */ }, onError)` and
`Image.getSizeWithHeaders(uri, headers, onSize, onError)`. Passing a success
callback selects callback mode and returns `undefined`; omit it to receive the
Promise. Supply the failure callback to handle errors in callback mode.

`Image.prefetchWithMetadata(uri, queryRootName, rootTag)` forwards tracing
metadata; an omitted root tag is `0`. Prefetch uses the bounded process-memory
cache described below. Cached entries can be evicted, and no disk persistence
or cancellation-by-request-ID API is provided.

## Internal loader contract

- `getSize(uri)` resolves `[width, height]`; `getSizeWithHeaders(uri, headers)`
  resolves `{width, height}` and sends the supplied string-valued headers.
- `prefetchImage` and `prefetchImageWithMetadata` resolve `true` only after a
  successful fetch and complete image decode. Metadata is tracing-only.
- `queryCache` returns `"memory"` only for actual retained decoded entries.
  Failures and evicted entries are absent. There is no disk-cache claim.
- The 32 MiB decoded LRU is shared with `QnxImageProvider` through
  `DecodedImageCache`. Qt Quick source resolution routes an already-prefetched
  URI to `image://rnqnx/cache/...`; it does not download that image again.
- A response that cannot be retained (larger than the cache budget or marked
  private/no-store) may load successfully but is absent from `queryCache`.
- HTTP header-bearing requests bypass and do not populate the URI-only cache.
  Automatic cookie load/save and cached authentication reuse are disabled.
  Uncredentialed requests respect private/no-store response directives.
- Redirects are rejected rather than forwarding caller credentials or silently
  crossing origins. Callers may explicitly request the destination. TLS errors
  are not ignored. HTTP errors, malformed payloads, oversized images, missing
  files and unsupported schemes reject the Promise.
- Requests have a 30-second wall-clock timeout and a 16 MiB encoded limit.
  Decode checks dimensions before allocation and rejects over 16 million
  pixels. All supported formats depend on installed Qt image plugins.
- File URLs, absolute paths, qrc URLs and strict base64 data URIs also decode
  into this cache. Percent-encoded data URIs are explicitly unsupported.
- Invalid arguments reject Promises. Native completions use React Native's
  `AsyncPromise`/CallInvoker rather than retaining raw runtime pointers.

## The ExpoImage view

`expo-image` renders through the Fabric component `ViewManagerAdapter_ExpoImage`
(`RNQNX_ENABLE_QT_EXPO_IMAGE`). Its item, `ExpoImageItem`, picks a source from the
`source` array and loads it itself: from the decoded memory cache
(`DecodedImageCache`, `cacheType: "memory"`), the on-disk cache (`"disk"`), the
network with the source's headers, or a file or base64 data URI. It decodes on a
worker thread and paints with `QPainter`. It shares the decoded cache with
`Image`, so `Image.prefetch` from React Native and `expo-image`'s own `prefetch`
fill the cache the view reads. A `source` that is an `ImageRef` (from
`Image.loadAsync`) skips loading: the picture is read from `ImageRefRegistry`.
The prop and limit table is in
[`@rn-qnx/expo-image-qnx`](../packages/expo-image-qnx/README.md).

HTTP(S) loads from every view, `prefetch` and `loadAsync` go through one
`ImageFetchQueue`: 6 at a time, and expo-image's `priority` (`low`, `normal`,
`high`) decides which waiting load starts next. Cancelling a waiting load removes
it; cancelling a running load frees its slot.

The loader module serves the expo-image functions the same way: `loadImage`,
`releaseImage`, `generateBlurhash`, `generateThumbhash`, `writeToCache`,
`readFromCache` and `configureCache` are methods of `ImageLoader` next to
`prefetchImageWithOptions`, `clearMemoryCache`, `clearDiskCache` and
`getCachePath`. `@rn-qnx/expo-image-qnx` wraps them as the `ExpoImage` module.

## Boundaries

This is a process-memory decoded cache, not an HTTP revalidation cache. Entries
remain until LRU eviction/process exit; changing resources should use versioned
URLs. Qt Quick can also maintain its own image cache. A query is a snapshot;
eviction can occur before a later image request. Request cancellation by ID,
cross-process caching and source-header props on React Native's own visual
`Image` are not implemented. The `ExpoImage` view and `expo-image`'s prefetch
use a 128 MiB on-disk cache (`ImageDiskCache`, least recently used first) and
key requests by URI plus headers. The host currently runs JavaScript
on the Qt event-loop thread; the loader shares that thread-affinity contract.

## Deterministic host validation

Check the public adapter and build both QNX and Marathon public-entry bundles:

```sh
node tests/js/qnx-public-entry.test.mjs
node tests/js/qnx-public-entry-metro.test.mjs
```

The JavaScript test calls the real public `Image` getter with a controlled
native loader. It covers Promise and callback modes, errors, header forwarding,
metadata defaults and cache results. It does not decode images or run a native
renderer. The Metro probe calls public `Image.getSize` on a local PNG and only
emits its existing ready marker after the dimensions check succeeds; rendering
that probe remains a separate host or target gate.

Run the native loader fixture with the selected Qt installation:

```sh
cmake -S tests/image-loader -B build/image-loader -G Ninja \
  -DCMAKE_PREFIX_PATH=/path/to/Qt/6.11.1/gcc_64
cmake --build build/image-loader
ctest --test-dir build/image-loader --output-on-failure
```

The fixture uses a loopback `QTcpServer`, not an external service: successful
PNG dimensions, cache reuse without a second request, actual headers and
header-cache isolation, 404/403, malformed/truncated bodies, redirects, byte
limits, timeouts, LRU eviction, private/no-store, local/base64 inputs and
teardown while a request is pending. It also resolves and reads a prefetched
entry through the real Qt Quick provider. CI runs this test with exact Qt
6.11.1 and uploads its CTest evidence.

The expo-image functions and the view run on real Hermes in two more host tests,
`rnqnx_expo_image_module_host_test` (JavaScript calls the module: prefetch, cache
functions, `loadAsync`, the hash generators, cache read and write,
`configureCache`, the view-ref functions through Fabric commands) and
`rnqnx_expo_image_fabric_host_test` (`opacity`, `transform`, `onLayout`, the image
events as Fabric events, `ImageRef` sources, commands). The queue, the hash
encoders against reference vectors and the view's lock and `ImageRef` handling are
Qt tests in `tests/native/qt-native-ui`. The first-draft module forwarding used
`__turboModuleProxy`, which this bridgeless host does not install; the module test
is what found it, and the module now reads `nativeModuleProxy`.

The full `QtReactHostHostTest` also contains `imageLoaderPromiseContract`, which
uses the real bridgeless `nativeModuleProxy.ImageLoader` in Hermes after loading
the Metro bundle. It checks Promise resolution shapes, metadata prefetch,
truthful cache entries, actual Error rejections and invalid arguments. This case
and existing `imageGetSize` passed locally under ASan using the full-host graph
build repairs from draft PR #28. The target adapter and its allocation site were
rebuilt together; no QNX execution or whole-renderer-suite pass is implied.

The `imageGetSize` playground fixture uses the QNX `Image` override, so the
full-host test also checks its public result shape instead of bypassing it
through `Image.ios`. The earlier ASan result above belongs to its cited baseline;
the repaired public-adapter candidate `6b528c17967177f92b5894565fb0b93eac226dd2`
subsequently passed the [full host renderer](https://github.com/mikevocalz/react-native-qnx/actions/runs/42652105343)
and [public-import native host](https://github.com/mikevocalz/react-native-qnx/actions/runs/42652106074)
workflows. The [candidate receipt](validation/pr106-merge-readiness-2026-11-10.md)
records those runs separately from fresh local wrapper/Metro checks. Later
integrated revisions still require their own validation.

These checks are host evidence. QNX SDP 8.0.5 cross-compilation, Qt Network
module size/build-time measurements, physical Pi networking/TLS/plugin behavior
and on-glass rendering remain pending. No physical-target gate is promoted.
