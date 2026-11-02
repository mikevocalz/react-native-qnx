# Expo Modules API 2.0 beta and QNX/Marathon integration

**Date:** November 2, 2026. **Status:** SDK 58 native-JSI compatibility gate; Expo Modules 2.0 for QNX or Marathon is **NOT implemented** in this PR.

## Verified upstream contract

Expo's [SDK 58 release](https://expo.dev/changelog/sdk-58) describes
**Expo Modules API 2.0** as a beta annotation/binding approach for native
**Swift and Kotlin** classes. The package `expo-modules-core` remains on
its SDK-aligned `58.x` version line, **not** `2.x`.

The public `requireNativeModule` function in
`expo-modules-core/src/requireNativeModule.ts` first reads registered
`globalThis.expo.modules` via `requireOptionalNativeModule` and throws
when the module is absent. That is the supported native module acquisition
boundary for React Native JavaScript clients. See
https://github.com/expo/expo/blob/main/packages/expo-modules-core/src/requireNativeModule.ts .

Our QNX host already builds an independent C++ JSI installer
`installExpoModulesQnx`, which registers `ExpoQnxPlatform` and sets
`globalThis.expo.modules` before React Native bundle evaluation.
This is an **existing QNX-specific native implementation**, not
automatically a Swift/Kotlin API 2.0 generated module.

Expo `expo-module` agent skill:
https://github.com/expo/skills/blob/main/plugins/expo/skills/expo-module/SKILL.md .
The skill explicitly routes migration of existing Swift DSL modules to
a separate `expo-migrate-module` experimental skill; neither is a valid
substitute for writing a QNX/Marathon C++ binding generator.

## Public API choice: preserve optional; add strict require

```ts
import {
  getQnxExpoPlatformModule,
  requireQnxExpoPlatformModule,
} from '@rn-qnx/expo-modules-core-qnx';

// Optional capabilities (e.g., consumer intentionally checks availability)
const optional = getQnxExpoPlatformModule(); // ExpoQnxPlatformModule | null

// Required QNX platform feature: informative error if missing.
const required = requireQnxExpoPlatformModule();
console.log(required.platform, required.architecture);
```

The new strict helper is source-backed, typed and fail-closed. The original
nullable `getQnxExpoPlatformModule()` is retained for backwards compatibility;
no change is made to the JSI native module installer ABI or registry behavior.

## Current capability matrix

| Surface                                               | QNX                                               | Marathon Linux/Wayland                            | Android / Apple SDK 58                 |
| ----------------------------------------------------- | ------------------------------------------------- | ------------------------------------------------- | -------------------------------------- |
| Expo Modules SDK 58 JavaScript imports                | existing QNX JSI bootstrap; runtime proof pending | no native Expo installer yet                      | supported SDK 58                       |
| Legacy Expo definition DSL binding                    | custom QNX C++ registry only                      | not implemented                                   | supported                              |
| Expo Modules API **2.0** annotation binding generator | **unsupported**, needs upstream/compiler design   | **unsupported**, needs Linux/Wayland integration  | beta Swift/Kotlin                      |
| Synchronous `ExpoQnxPlatform` getter                  | existing C++ module, physical run pending         | not implemented                                   | QNX-only API not applicable            |
| Async calls / Events / SharedObject                   | no general QNX binding proof                      | not implemented                                   | SDK-dependent API; verify individually |
| QNX/Marathon Expo autolinking                         | QNX-specific resolver already exists              | must be added as separate target, not spoof `qnx` | native Apple/Android                   |

## Implementation sequencing

1. Keep Expo SDK 58 and _verified installed_ RN 0.88 and
   `expo-modules-core@58.x` patch aligned. PR #36 has exact proposed pins
   but the lockfile and native ABI remain pending.
2. Add required-vs-optional public API as in this PR without modifying the
   existing QNX installer or QNX Screen backend.
3. Run installed source contract checks and the original
   `pnpm validate:qnx-expo-modules` and Hermes/Expo JSI probe.
4. For actual API 2.0 native bindings, inspect Expo Swift macros/Kotlin
   annotations, generated metadata and runtime entrypoint. Design a QNX
   C++ codegen/backend only from actual source and after upstream review;
   do not port Swift/Kotlin annotations by name alone.
5. Propose a distinct MarathonOS Linux/Wayland Expo bridge that reuses
   shared JSI contracts but respects Marathon bwrap, Wayland app identity,
   D-Bus permissions and lifecycle. Do not reuse QNX binaries.
6. Test real `Function`, `AsyncFunction`, event emitter,
   typed shared-object and error propagation across Hermes and Qt lifecycle.
   Keep native async support disabled until shutdown/cancellation tests pass.
7. Ship native test app, documentation, CI/artifacts and physical
   QNX/Marathon tests before claiming out-of-box native Expo compatibility.

## Verification, errors and release gate

```sh
corepack pnpm install --frozen-lockfile
node scripts/validate-expo-modules2.mjs
pnpm test:js
pnpm validate:qnx-expo-modules
pnpm validate:dependencies
pnpm typecheck
pnpm audit:dependencies
```

Run Hermes/QNX runtime probe after successful C++ compile on licensed SDP
8.0.5, QSTI 0.5.0 build 50. Capture distinct real Pi and Marathon
QEMU/Wayland evidence; an SDK source-test does not prove runtime support.

## API skills and ownership

- `margelo/react-native-skills:api-design`: strict vs optional semantics,
  call examples and accurate typed results.
- `expo/skills:expo-module`: actual Expo module initialization/JSI source,
  autolinking and native lifecycle.
- `margelo/react-native-skills:cpp`: JSI C++ registry lifetime and ABI.
- `callstackincubator/agent-skills:react-native-testing`:
  required and missing module behavior.
- `callstackincubator/agent-skills:upgrading-react-native`:
  RN 0.88/Expo SDK 58 compatibility.
- Roles: Expo native module owner, C++/JSI owner, platform/toolchain owners,
  external-review and device QA. Actual installed skill invocation/test results
  require separate evidence rows.

See PR #33 for shared rules; PR #38 for Nitro v1 class migration and codegen
constraints. Do not label this work Expo Modules API 2 native support.

## Rollback

Remove the new strict helper, declarations and compatibility checks.
The existing optional accessor and Qt/JSI QNX module registration remain.
