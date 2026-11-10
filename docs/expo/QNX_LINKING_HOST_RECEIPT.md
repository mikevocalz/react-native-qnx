# Navigation host runtime receipt

Date: 2026-11-10. Source: `fb06ffb` (includes queued delivery, Babel forwarding and import-safe OFF fixes).

Actual native execution, not a VM substitute: Clang 19.1.7, Qt 6.11.1,
React Native 0.88.1 and its pinned Hermes. Both real production Metro platform
bundles were evaluated by QtReactHost. Tests used the offscreen Qt platform and
software Quick backend.

| Feature     | Flag | qnx  | marathon |
| ----------- | ---- | ---- | -------- |
| Linking     | ON   | PASS | PASS     |
| Linking     | OFF  | PASS | PASS     |
| BackHandler | ON   | PASS | PASS     |
| BackHandler | OFF  | PASS | PASS     |

Each process exited 0 with `RNQNX_LINKING_RUNTIME_OK` or
`RNQNX_BACK_HANDLER_RUNTIME_OK`. Tests include pre-pump non-delivery and throwing
JS listeners reaching host error reporting without escaping the Qt event
filter. Linking also executes real Promise settlement, subscriptions, URL
validation, queue bounds, invalidation and worker-release checks.

Host/common runtime objects used ASan; navigation adapters and test objects used
ASan+UBSan, with halt-on-error and leak detection disabled. Prebuilt Qt and the
shared RN/Hermes dependency archives were not fully instrumented. The manual
link reused the supported Clang dependency graph while compiling our own host,
common runtime and test objects separately; it did not require the experimental
Worklets adapter or mutate its build graph. Full integrated CI remains separate.

An initial exploratory host-wide UBSan build stopped during Hermes creation:
its instrumented `std::__shared_count::operator=` interposed into Hermes's
RTTI-disabled StorageProvider code. The successful runs use the intended
adapter/test UBSan boundary instead; no UBSan runtime suppressions were added.

This is **synthetic Qt input/activation evidence only**, not physical QNX or
Marathon qualification, OS URL registration, outbound URL dispatch, or full
Expo Router acceptance.

## Exact local artifacts (SHA-256)

- `linking1`: `af411e4a183c3aa55c1bb931da8bcd8144bde381fabde585ca17fe857506ff93`
- `linking0`: `fbe226035a061cff36db3588ed18b57a58dc4154106a2638376ba53a82fceb65`
- `backhandler1`: `817d05d52026194f9115eb7a83d489261dfba811fa66f1d97daa3979e08dd71b`
- `backhandler0`: `05c950fdb13888dbb2e5933a52521f2fe15368bfc76936ad240358073ccc9ae8`
- `linking-qnx.bundle.js`: `491f744ed727c051b93d85dc20f7abf7e361f27dcbe28138274b76fba9dafd64`
- `linking-marathon.bundle.js`: `126227133bd5640a9dab4855a2912e7247defe761d1b7613aa44fe00ae260545`
- `backhandler-qnx.bundle.js`: `ebb5bc9c324b4af7c091023ef25fe807b4c048bac968579fb4424c1444bf2b03`
- `backhandler-marathon.bundle.js`: `6d0ae76a5455aba413e80a0925066777ab82b01bf73eb7a5d2bbf37494a1770e`

## Default-OFF rendering regression check

The initial eager BackHandler availability error also blocked upstream
`renderApplication`'s transitive import. The corrected disabled exports are
safe to import, lazy-load no upstream native listener, and explicitly reject
operations rather than pretend success. Linking follows the same contract.

After the correction, both default-config qnx and marathon `index.js` production
bundles ran in the actual native playground with both navigation flags OFF.
Each mounted, captured a 640×480 image, cleaned up its surface and exited 0
with `RNQNX_PLAYGROUND_SCREENSHOT`. The Marathon image was inspected and contains
the real playground's views and text, not a blank frame. All eight navigation
runtime combinations above were then rerun successfully with refreshed bundles.

- `playground0`: `241ce6bf38f44bfe80420e9a97c0c7913a0894436806bf19e30a8f7441cf2511`
- `app-qnx.bundle.js`: `d5e3b0179e7aef92790af8348addedaf90107e4265fa910f66da8d78795c700c`
- `app-marathon.bundle.js`: `179abbd357eb1e44601a2e49edcbc7b3246cb97227ab9b92256638e990736409`
- `app-qnx.png`: `3dbe1f0f9155ff6616894efccd510788ce2b19e104df06d17a5a2308a8292567`
- `app-marathon.png`: `3dbe1f0f9155ff6616894efccd510788ce2b19e104df06d17a5a2308a8292567`
