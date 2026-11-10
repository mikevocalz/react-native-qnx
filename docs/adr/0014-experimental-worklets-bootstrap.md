# ADR-0014: Experimental upstream Worklets bootstrap

## Status

Accepted for the default-OFF native Linux prototype only. Independent native
review approved this narrow exception; four actual host runtime tests pass.

## Context

Worklets 0.13.1 exposes its runtime through a private `WorkletsModule` TurboModule
bootstrap, then installs its own upstream JSI proxy. Replacing this with an
application Nitro façade would not satisfy upstream imports. ADR-0004 remains
the rule for new platform application APIs.

## Decision

Permit a narrowly scoped, default-OFF native Linux prototype to implement the
four exact methods from upstream `src/specs/NativeWorkletsModule.ts`. The adapter
creates the real upstream `WorkletsModuleProxyInitializer`, runtime, serializers
and schedulers. It does not implement another animation engine or public API.
Bundle Mode and slow animations fail explicitly. Frame callbacks use an attached
QQuickWindow frameSwapped observation, steady-clock milliseconds and owner queued
delivery. QQuickWindow::update requests the next real frame. There is no timer
fallback; replacing a window requires a new host.

The platform owner queue provides RN delivery. A Qt owner-thread scheduler
implements the upstream UI scheduler interface. Shutdown closes admission and
invalidates the proxy before the RN runtime is destroyed. Upstream license
notices remain in the supplied package; no premium Forge source is included.

## Scope and evidence

Pinned npm source: `react-native-worklets@0.13.1`, upstream release commit
`24e415d2c036a532cf7820ff0c400e49841bda50`. Native Common/cpp is consumed from an
explicit external package directory and checked by package name/version. Build-copy-only adaptations handle Linux's two-argument pthread naming API and
supply missing standard `<algorithm>`/`<iterator>` headers in EventLoop.cpp,
JSISerializer.cpp and WorkletsJSIUtils.cpp for libstdc++.
No `ANDROID` or Apple macros are forged.

The intended contract test imports the real public JS package and runs a
transformed worklet on the actual UI runtime, delivers its result to RN, then
reloads the host with queued work pending. A successful production Metro bundle
alone is not native runtime evidence. Native compilation/execution passed four host tests; see
[validation](../validation/worklets-host-prototype-2026-11-10.md).

## Exclusions and rollback

QNX is rejected by this prototype: its lean Hermes unpacker bytecode path,
compiler ABI and on-target scheduling still need independent qualification.
This is not Reanimated support, RNGH recognizer support, frame timing proof,
worker-runtime qualification, Wayland-device proof or physical-input evidence.

Disable `RNQNX_ENABLE_WORKLETS` (default OFF) to retain the existing runtime.
This proposal does not authorize replacing Nitro for application APIs.
