# Marathon Metro proof — experimental Linux/Qt platform identity

Status: JS build configuration added; native MarathonOS runtime/device has NOT been demonstrated.

The production code already has a Qt/Fabric/Hermes renderer for QNX, with source-level portability to Linux. This PR adds a separate Metro **marathon** platform that prefers Marathon-specific React Native JS overrides, then falls back to source-compatible QNX JS component overrides. It does not import QNX binary libraries and does not modify the production QNX bundler.

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

Only after a successful native host can the Marathon bundle be evaluated. The selected JS OS name is `marathon`; `Platform.select({marathon: ...})` prefers the Marathon key. A host must install real `__RNQNX_PLATFORM_CONSTANTS` before the bundle runs; do not hard-code OS version. JS components inherited from QNX are temporary **source** reuse, and any OS-specific behavior (StatusBar, KeyboardAvoidingView, Focus, SafeArea, etc.) must be audited for Marathon.

## Known open items

- No Linux Qt/Wayland native runner, Marathon shell bridge, typed native module ABI or signed .marathon native-executable package in this PR.
- QNX-specific no-op StatusBar override is NOT acceptable as final Marathon system bar behavior.
- RN 0.88 rc baseline is inherited until a separate tested SDK58/RN0.88 stable upgrade with regenerated pnpm lockfile is merged.
- Expo Router, autolinking, Nitro 1 alpha, Expo Modules API v2 beta, offscreen Qt render and hardware tests follow in separate reviewed slices.
- Evidence required: production bundle compile and output hash on a runnable npm environment, cross-platform JS tests, Linux Qt native runtime screenshot and actual QNX regression run.

## Rollback

Remove the new packages, Marathon example Metro config and test; QNX Metro config and source remain untouched.
