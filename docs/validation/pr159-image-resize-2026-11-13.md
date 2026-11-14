# Expo Image decode resolution after view growth

Base: `63e6bd05d2989aeaf6943171f13ef851bc47a7de` on PR 159.
The preceding component-table repair passes its hosted checks, including both
full renderer configurations. This continuation needs its own exact-head checks.

## Behavior and boundary

`geometryChange` only invalidated the paint cache, so a still image decoded at
20 pixels remained at that resolution after its view grew to 240 pixels.
`ensureResolution` now coalesces a source reload when the selected URI changes
or a reduced decode no longer covers its target. It also responds to a device
pixel ratio change, natural-size/fill content fit, or disabling downscaling.
Resource locks defer this work until unlock. Shrinking a fully decoded view
keeps its existing image. The current layer stays visible during fetch/decode.

Changed paths: the ExpoImageItem header/implementation, its native host tests,
and these evidence/handoff records. No app window contract, public JS API,
renderer, dependency, feature default or serialized React state changes.
The shared decoded cache still accepts only full-resolution still images.

## Validation

Commands on macOS, Apple Clang, Homebrew Qt 6.12.0 Release:

```sh
cmake -S tests/native/qt-native-ui -B /tmp/rnqnx-image-resize-unit -G Ninja \
  -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/rnqnx-image-resize-unit --target rnqnx_expo_image_item_test --parallel 2
ctest --test-dir /tmp/rnqnx-image-resize-unit -R '^rnqnx_expo_image_item_test$' --output-on-failure
```

- Passed: 30 Qt Test cases, zero failures/skips. New cases check a full 400-pixel
  decoded cache entry after growth, resource-lock deferral, disabling
  downscaling, natural-size fit, and avoiding another load on shrink.
- Negative control: disabling the new reload path fails both growth cases
  (`loaded.count()` stays 1 instead of 2). Restored candidate passes the full
  image-item executable. Independent review also caught growth before the
  first layer exists. A gated-worker regression fails before the completion
  recheck and passes after `show` rechecks live geometry/options. Logs in `docs/evidence/expo-image-resize-2026-11-13/`
  replace temporary checkout/build paths with placeholders.
- Host-only Qt Quick evidence; no Fabric, Hermes, sanitizer or device result
  follows from this standalone graph. QNX cross-build, Pi and Marathon checks
  are not run for this continuation. Exact-head CI remains pending.
- Lock SHA-256: `4a4695a15a04a2ec9dcfca49b8fa1cf29c624feaf8281f91b3881309000439f2`.

## Skills, ownership and remaining work

Read and applied repository qnx-native-api/qnx-platform-validation and the
pinned Margelo api-design, build-nitro-modules, cpp sources at
`1e9e17be6f41c838db472d6a4b943fc26355a3be`; their bytes were verified against
skills.lock.json. Decision: preserve the view/decoder boundary, validate actual
decoded resolution and retain resource-lock behavior. Callstack
react-native-best-practices at `59e9d17446a248b21d86f078fcd0bd2d36db1ca4`
informed separating paint invalidation from decode work. The installed
no-ai-slop skill was applied to this receipt.

DRI: current integration agent, ExpoImageItem paths. Independent review:
CodeRabbit CLI 0.7.6 reviewed the staged slice and found the initial-decode
race above; it is fixed and regression-tested. No fresh review of that fix is
claimed. Security/licensing: no new permissions, dependency or premium payload.
Performance: decode limits remain bounded by the existing pixel cap; no target
latency, resize-storm or memory budget is qualified. Existing GUI-thread file/
disk I/O, cache-corruption fallback, redirects, per-view network managers and
navigation gaps remain separate handoff work. Rollback: revert this helper,
setter and regression slice together; the original component-table repair can
remain. Next: review, exact-head gates and target resize trial, then loader I/O.
