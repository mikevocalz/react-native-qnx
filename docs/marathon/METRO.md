# Marathon Metro proof - experimental Linux/Qt platform identity

Status: merged (PR #34, 2026-11-02; package export conditions fixed in PR #138, 2026-11-12). The Marathon and QNX production bundles build in CI. No MarathonOS runtime or device run exists.

The production code already has a Qt/Fabric/Hermes renderer for QNX, with source-level portability to Linux. The `@rn-qnx/metro-config-marathon` and `@rn-qnx/react-native-marathon` packages add a separate Metro **marathon** platform that prefers Marathon-specific React Native JS overrides, then falls back to source-compatible QNX JS component overrides. It does not import QNX binary libraries and does not modify the production QNX bundler.

## Commands after frozen pnpm installation

From repository root:

```sh
pnpm install --frozen-lockfile
pnpm test:js
cd apps/rn-playground
../../node_modules/.bin/metro build index.js --config metro.marathon.config.js --platform marathon --dev false --out ../../artifacts/marathon-playground.bundle.js
# Compare against existing QNX build separately, without changing metro.config.js:
../../node_modules/.bin/metro build index.js --platform qnx --dev false --out ../../artifacts/qnx-playground.bundle.js
```

The Marathon bundle is evaluated by the Linux Qt host (`rnqnx_marathon_host`, see [NATIVE_HOST](NATIVE_HOST.md)) in the offscreen CI job. It has not been evaluated under a Wayland compositor or on MarathonOS. The selected JS OS name is `marathon`; `Platform.select({marathon: ...})` prefers the Marathon key. A host must install real `__RNQNX_PLATFORM_CONSTANTS` before the bundle runs; do not hard-code OS version. JS components inherited from QNX are temporary **source** reuse, and any OS-specific behavior (StatusBar, KeyboardAvoidingView, Focus, SafeArea, etc.) must be audited for Marathon.

## Verified

CI workflow `marathon-metro.yml`, [run 44492351488](https://github.com/mikevocalz/react-native-qnx/actions/runs/44492351488) (PR #149 head `cbe27bf7`): frozen install, `pnpm format:check`, `pnpm test:js`, `pnpm typecheck`, `pnpm validate:dependencies`, then production QNX and Marathon bundles with `SHA256SUMS` in the `marathon-metro-<sha>` artifact. The baseline is `react-native` 0.88.1 and `expo` 58.0.24, not an RC. The CI job builds the QNX bundle but does not compare it to a pre-change hash.

## Open items needing owner or upstream

- The JS overrides inherited from QNX are source reuse. The QNX no-op StatusBar is not acceptable final Marathon system-bar behavior. KeyboardAvoidingView, focus and safe-area behavior need a Marathon audit against the shell's lifecycle and inset contract, which upstream has not defined for native clients. Owner action: run the Wayland host on a Marathon device and record each behavior.
- Expo Router, autolinking, Nitro 1 alpha and Expo Modules API v2 beta capability on Marathon follow in separate slices. The Nitro adapter compiles in CI ([NATIVE_HOST](NATIVE_HOST.md)); runtime proof on Marathon is open.
- A bundle hash comparison of the QNX output before and after any shared-override change is a regression check not yet automated.

## Rollback

Remove the new packages, Marathon example Metro config and test; QNX Metro config and source remain untouched.
