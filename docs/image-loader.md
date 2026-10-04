# Image loader: host-verified implementation

The core React Native `ImageLoader` adapter now performs real asynchronous
HTTP(S) work through Qt Network. It is an upstream core-module adapter, not a
new application TurboModule architecture.

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

## Boundaries

This is a process-memory decoded cache, not an HTTP revalidation cache. Entries
remain until LRU eviction/process exit; changing resources should use versioned
URLs. Qt Quick can also maintain its own image cache. A query is a snapshot;
eviction can occur before a later image request. Request cancellation by ID,
disk persistence, cross-process caching and source-header props on visual
`Image` are not implemented by this change. The host currently runs JavaScript
on the Qt event-loop thread; the loader shares that thread-affinity contract.

## Deterministic host validation

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

These checks are host evidence. QNX SDP 8.0.5 cross-compilation, Qt Network
module size/build-time measurements, physical Pi networking/TLS/plugin behavior
and on-glass rendering remain pending. No physical-target gate is promoted.
