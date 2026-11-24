# Expo CLI serves platform=qnx (2026-11-24)

Scope: host only (macOS arm64, Node 26.13.0, pnpm 12.21.0, Expo CLI 58.1.21,
`expo-modules-autolinking` 58.1.0). No QNX VM, Pi 400 or board was used, nothing was
published, and no release or tag was created.

## Problem

On a clean project from `rnqnx project create`, `expo start --offline` answered
`GET /index.bundle?platform=qnx` with HTTP 500:
`No support package is known for platform "qnx"`
(`docs/evidence/expo-runner-pack-2026-11-24/expo-cli-qnx-bundle-error.txt`).
`rnqnx project start` served the same project.

## Root cause

1. The template's Metro config declares `qnx` in `resolver.platforms`. Expo CLI
   (`withMetroMultiPlatform.js`) keeps every platform in that list.
2. On each resolve, Expo CLI calls `getReactNativeHostPackage(platform)` (lines 133 and 145,
   used at 396 and 601), which calls
   `getSupportPackageForPlatform` from `expo-modules-autolinking/exports`.
3. `expo-modules-autolinking/build/platforms/index.js` answers with a closed `switch`:
   `ios`, `android`, `tvos`, `macos`, `windows`, `apple`, `web`, `devtools`. Anything else
   throws. The exports are getter-only and non-configurable, `@expo/platforms` lists only
   `android ios web tvos macos`, and Expo CLI has no registration hook, so a Metro config, a
   config plugin or `app.json` `platforms` cannot reach this code. The autolinking resolver in
   Expo CLI skips unknown platforms (`KNOWN_PLATFORMS` filter), which is why only the
   host-package lookup fails.

## Fix

`patches/expo-modules-autolinking@58.1.0.patch` adds `case 'qnx'` and `case 'marathon'`, both
returning `'react-native'`, the host package (the platform layer is `@rn-qnx/react-native-qnx`,
not a react-native fork). `pnpm-workspace.yaml` registers the patch and pins
`expo-modules-autolinking` to 58.1.0 by override, because pnpm rejects a patch whose version
is not installed. `rnqnx project create` copies the workspace policy, so new projects carry
both; its patch-inventory guard now expects three patches. The lockfile gained the patch hash.
Removal condition: drop the patch once an Expo SDK ships a platform registry that accepts
`qnx` (not available in 58.1.0 or in the `next` tag, 58.0.13, checked 2026-11-24).

## Regression test (red then green)

`tests/js/expo-cli-qnx-platform.test.mjs` calls the same export Expo CLI uses.

- `test-red.txt`: installed without the patch, exits 1 with
  `No support package is known for platform "qnx"`.
- `test-green.txt`: installed with the patch, prints `expo-cli-qnx-platform: ok`, exit 0.
  It also checks that `ios`, `android` and `web` are unchanged and an unknown name still throws.
- `tests/js/qnx-project-create.test.mjs` now checks that a created project carries the patch,
  the override and the patch file.
- `test-js-all.txt`: `node scripts/run-js-tests.mjs`, full host JavaScript suite.

## End to end on a clean temporary project

`node scripts/verify-expo-cli-qnx.mjs` (`pnpm verify:expo-cli-qnx`) creates a project with
`rnqnx project create`, runs `pnpm install --ignore-scripts`, starts `expo start --offline` and
requests the bundle. Logs are in `e2e/`.

| Request                                             | Result              |
| --------------------------------------------------- | ------------------- |
| `/index.bundle?platform=qnx&dev=true`               | HTTP 200, 5574276 B |
| `/index.bundle?platform=qnx&dev=false&minify=false` | HTTP 200, 3512469 B |

Expo CLI printed `qnx Bundled ... index.js (1149 modules)` for the dev bundle and 1013 modules
for the production bundle. Metro, not `rnqnx project start`, served both.

## Not shown

Device execution, Fast Refresh on a board, the downloaded runner and the standalone package.
The `expo` gate stays pending; this closes the "Expo CLI owning Metro" item for the QNX bundle on
the host only.
