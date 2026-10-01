# Dependency Compatibility Matrix

This matrix records the installed compatibility line used by `react-native-qnx`. Direct workspace dependencies use exact versions in `package.json` and `pnpm-lock.yaml`. Resolved entries are dependencies selected by that locked graph; bundled entries are source snapshots shipped by React Native rather than independently versioned project dependencies.

No entry in this matrix implies that an upstream library already supports QNX. QNX integration remains repository work and requires validation against the licensed QNX SDK and target hardware.

## Registry audit snapshot

The npm registry was queried on 2026-08-13. The audit distinguished an
individually newest release from the newest line whose installed manifests and
source contracts agree:

| Component      | Registry `latest` | Selected | Selection reason                                                          |
| -------------- | ----------------- | -------- | ------------------------------------------------------------------------- |
| React Native   | 0.87.0            | 0.86.2   | Expo 57.0.12's installed development baseline is React Native 0.86.2.     |
| RN Jest preset | 0.87.0            | 0.86.2   | Must stay on the same release line as React Native.                       |
| React          | 19.2.8            | 19.2.3   | Matches RN 0.86.2's bundled Fabric reconciler and Expo's tested baseline. |
| Expo           | 57.0.12           | 57.0.12  | Current registry release and selected SDK line.                           |
| Metro          | 0.87.0            | 0.84.4   | Resolved `@expo/metro` 56.0.0 pins the Metro family to 0.84.4.            |
| Nitro Modules  | 0.36.5            | 0.36.5   | Current registry release; Nitrogen is pinned to the same release line.    |
| Nitrogen       | 0.36.5            | 0.36.5   | Current registry release and matches the Nitro runtime.                   |
| TypeScript     | 7.0.2             | 6.0.3    | Current Expo 57 template/tooling line uses TypeScript 6.0.3.              |
| Node.js types  | 26.2.0            | 24.13.3  | Models Node 24 LTS so code cannot silently depend on Node 26-only APIs.   |
| Prettier       | 3.9.6             | 3.9.6    | Current registry release.                                                 |
| pnpm           | 11.21.0           | 11.21.0  | Current registry release and exact package-manager identity.              |

React Native 0.87, Metro 0.87, and TypeScript 7 are therefore known newer
releases, not overlooked updates. Advancing to them is a coupled compatibility
upgrade rather than a safe patch refresh. Registry tags are time-sensitive; the
date above must be refreshed during the next dependency update.

## Legend

- **Pin:** exact direct workspace version
- **Resolved:** exact transitive version in the current lockfile
- **Bundled:** identity read from the installed React Native source/package metadata
- **Target:** component supplied by the installed QNX SDP or BSP and validated at configure/runtime
- **Not pinned:** future dependency; no compatibility or QNX-support claim has been made

## Platform and toolchain

| Component                  | Status                      | Version / requirement       | Notes                                                                                                                                                                       |
| -------------------------- | --------------------------- | --------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| macOS host                 | Supported host validation   | Darwin with Apple Clang     | Runs authoring checks and sanitizer-backed C++ host tests. It does not provide QNX compile or runtime proof.                                                                |
| Linux host                 | Supported host validation   | Linux with Clang or GCC     | Runs the same non-QNX host test path.                                                                                                                                       |
| QNX build host             | Required for QNX validation | x86-64 QNX-supported host   | Current repository cross-build evidence is Windows x86-64 with QNX SDP 8.0.2. The same scripts are intended for a licensed Linux host; Windows uses Git Bash for the checked-in shell workflow. |
| QNX SDP                    | Target                      | 8.0.5 current / 8.0.2 proven | QNX Everywhere's September 2026 Pi QSTI supports SDP 8.0.5. Existing repo cross-build evidence was captured with 8.0.2, so the Pi preflight records the actual installed compiler before target evidence is accepted. |
| QNX Software Center        | Host prerequisite           | Licensed compatible release | Installs and licenses SDP/QSTI; the project does not pin or redistribute this proprietary installer.                                                                        |
| Raspberry Pi QSTI          | Target image baseline        | 0.5.0 build 50              | Current September 2026 QSTI for both Pi 4 and Pi 5 (`com.qnx.qnx800.quickstart.rpi4` / `.rpi5`), supporting QNX SDP 8.0.5. See `raspberry-pi-today.md`. |
| QNX compiler (`qcc`/`q++`) | Target                      | SDP-provided                | `scripts/doctor.sh` validates availability after the SDP environment is sourced.                                                                                            |
| CMake                      | Host prerequisite           | 3.31.9 or newer             | Matches `cmake_minimum_required(VERSION 3.31.9)` and `scripts/doctor.sh`.                                                                                                    |
| Ninja                      | Host prerequisite           | 1.11.0 or newer             | Generator used by the checked-in presets.                                                                                                                                   |
| Node.js                    | Host prerequisite           | 24.3.0 or newer             | Matches the workspace engine constraint and the selected React Native line.                                                                                                 |
| pnpm                       | Pin                         | 11.21.0                     | Exact workspace package-manager identity.                                                                                                                                   |
| Python                     | Host prerequisite           | 3.11 or newer               | Required by current/future React Native tooling and codegen.                                                                                                                |

## React Native runtime

| Component                   | Status             | Version / identity                  | Evidence and scope                                                                                                                                                                           |
| --------------------------- | ------------------ | ----------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| React Native                | Pin                | 0.86.2                              | Installed manifest and lockfile; the project must supply the out-of-tree QNX platform.                                                                                                       |
| React                       | Pin                | 19.2.3                              | Matches the React Native peer baseline and Expo's tested development version.                                                                                                                |
| Yoga                        | Bundled            | React Native 0.86.2 source snapshot | No standalone Yoga version is inferred. The installed public declaration `YGNodeCalculateLayout` is audited in `ReactCommon/yoga/yoga/YGNode.h`.                                             |
| Hermes                      | Bundled            | `hermes-v250829098.0.16`            | Read from React Native's installed `sdks/.hermesv1version`; the version component is `v250829098.0.16`. This identifies source only—Hermes has not been cross-compiled or validated for QNX. |
| Hermes compiler package     | Bundled dependency | 250829098.0.16                      | React Native 0.86.2 package metadata; not proof of QNX portability.                                                                                                                          |
| Fabric scheduler            | Bundled            | React Native 0.86.2 source snapshot | Installed `ReactCommon/react/renderer/scheduler/Scheduler.h` contains `class Scheduler final`. No QNX renderer seam is claimed yet.                                                          |
| Fabric mounting coordinator | Bundled            | React Native 0.86.2 source snapshot | Installed `ReactCommon/react/renderer/mounting/MountingCoordinator.h` contains `class MountingCoordinator final`.                                                                            |

## Expo, Metro, and TypeScript

| Component               | Status     | Version          | Evidence and scope                                                                                                                                                                                             |
| ----------------------- | ---------- | ---------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Expo package / SDK line | Pin        | 57.0.12 / SDK 57 | Expo's installed manifest tests against React Native 0.86.2 and React 19.2.3. A QNX Expo platform has not been implemented or validated.                                                                       |
| Metro                   | Pin        | 0.84.4           | Direct workspace Metro pin and the upstream Metro version required by resolved `@expo/metro`. No QNX Metro resolver exists yet.                                                                                |
| `expo-modules-core`     | Resolved   | 57.0.10          | Resolved through Expo's installed dependency graph. `android/src/main/cpp/ExpoModulesHostObject.h` contains `class ExpoModulesHostObject`; this Android source is contract evidence, not a QNX implementation. |
| `@expo/metro`           | Resolved   | 56.0.0           | Expo's Metro fork/wrapper; its installed manifest resolves upstream `metro` 0.84.4.                                                                                                                            |
| `@expo/metro-config`    | Resolved   | 57.0.8           | Expo Metro configuration selected by the locked Expo graph.                                                                                                                                                    |
| `xcode` → `uuid`        | Override   | 3.0.1 → 11.1.1   | Scoped override replaces vulnerable uuid 7.0.3. Installed `xcode/lib/pbxProject.js` uses `uuid.v4()`, and the validator checks that exact CommonJS symbol.                                                     |
| Expo Router             | Not pinned | —                | No direct workspace package or QNX compatibility claim exists yet.                                                                                                                                             |
| TypeScript              | Pin        | 6.0.3            | Exact workspace compiler pin.                                                                                                                                                                                  |

## Native interop

| Component                  | Status | Version | Installed source contract and scope                                                                                                                                                     |
| -------------------------- | ------ | ------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| React Native Nitro Modules | Pin    | 0.36.5  | `cpp/registry/HybridObjectRegistry.hpp` contains `registerHybridObjectConstructor`; `cpp/entrypoint/InstallNitro.hpp` contains `void install(`. Neither symbol establishes QNX support. |
| Nitrogen                   | Pin    | 0.36.5  | Exact code-generator pin on the same release line as Nitro Modules. No `nitro.json` contract or generated QNX output exists yet.                                                        |

## Graphics and text

| Component       | Status               | Notes                                                                                                                                                                                                                                                                    |
| --------------- | -------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| Qt 6 / Qt Quick | Locked, not pinned   | Component backend per ADR-0001. Current release is 6.11.1 with 6.12 in beta as of 2026-08-14; the macOS development host builds against 6.11.1 per ADR-0010. The exact LTS tag for the target, QNX source seams, build result, and licensing decision remain G0-B gates. |
| Skia            | Superseded direction | Not the component backend. Any future leaf/canvas use requires its own decision and QNX proof.                                                                                                                                                                           |
| Text stack      | Qt-tag dependent     | HarfBuzz/FreeType and other third-party identities must be recorded from the exact Qt source/build rather than invented.                                                                                                                                                 |
| Image codecs    | Not pinned           | Codec choices and provenance follow the proven Qt build or a later explicit dependency decision.                                                                                                                                                                         |

## QNX target libraries

| Component          | Status           | Source             | Scope                                                                                                                     |
| ------------------ | ---------------- | ------------------ | ------------------------------------------------------------------------------------------------------------------------- |
| QNX Screen         | Target           | QNX SDP / BSP      | Window system beneath Qt QPA plus a direct G0/bare-Screen probe; not the Fabric component backend. Must be target-tested. |
| EGL                | Target           | QNX SDP / BSP      | Graphics integration used only as required by the proven Qt/QPA build; not established by host tests.                     |
| OpenGL ES          | Target           | QNX SDP / BSP      | Capability is target/BSP dependent and must be established during Qt G0-B.                                                |
| Vulkan / Vulkan SC | Target-dependent | QNX SDP / BSP      | Future backend; not required for Milestone 0.                                                                             |
| TLS provider       | Not pinned       | QNX-approved stack | Must be selected against BSP capabilities and security policy.                                                            |

## Automated dependency and source audit

After `pnpm install`, run:

```bash
pnpm validate:dependencies
```

The validator checks exact direct versions, React/React Native/Expo compatibility declarations, the resolved Expo packages above, the Hermes source tag, and the named installed source files/symbols. Those checks protect against upstream source-shape drift. They do not replace QNX compilation, linkage, or target testing.

### Security audit status

Run `pnpm audit:dependencies` with registry access. The 2026-08-13 audit found
and removed the `uuid` advisory with the scoped, source-checked override above.
Two high-severity `image-size` denial-of-service advisories remain in Metro's
development-only dependency graph. The registry has no fixed release: latest is
2.0.2 while the advisories affect versions through 2.0.2, and even Metro 0.87.0
still declares `image-size ^1.0.2`. This is recorded as `UPSTREAM-DEP-001` in
`docs/handoff/BLOCKERS.md`; do not process untrusted assets with Metro until it
is resolved.

For the QNX toolchain and target path, also run on the licensed Ubuntu host:

```bash
qcc -V
q++ -V
cmake --version
ninja --version
node --version
pnpm --version
./scripts/doctor.sh
```

## Updating this matrix

1. Audit registry releases and choose the newest mutually compatible React Native/React/Expo/Metro line; do not independently select incompatible latest versions.
2. Confirm compatibility declarations and required native symbols in installed package source.
3. Record direct pins, resolved graph versions, and bundled source identities separately.
4. Update `package.json`, `pnpm-lock.yaml`, this matrix, and ADR-0008.
5. Run `pnpm validate:macos` (or the Linux host equivalent) and record results in `docs/handoff/TEST_STATUS.md`.
6. Queue every QNX-specific change in `docs/handoff/QNX_VALIDATION_QUEUE.md`, then validate it on the licensed QNX build host/target.
