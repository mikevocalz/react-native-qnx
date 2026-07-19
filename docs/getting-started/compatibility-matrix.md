# Dependency Compatibility Matrix

This matrix records exact versions used by `react-native-qnx`. Versions are pinned in the workspace lockfile and CMake dependency manifests. Do not change a pinned version without updating this matrix and the relevant ADR.

## Legend

- **Pin:** exact version used in the project
- **Verify:** version to be confirmed against upstream release notes before use
- **Target:** component is provided by the target QNX BSP or SDP and is validated at configure time

## Platform and toolchain

| Component | Pin / Verify | Notes |
| --- | --- | --- |
| Host operating system | Ubuntu 24.04 LTS (x86-64) | Primary build host. macOS is supported only as a remote-development client. |
| QNX SDP | 8.0.5 | Required target platform. |
| QNX Software Center | 2.0.4 or newer | Used for SDP installation and license management. |
| QNX compiler (`qcc`/`q++`) | Target-specific | Provided by SDP 8.0.5; doctor validates `qcc -V` output. |
| CMake | 3.28.0 or newer | Required for modern CMake features and toolchain support. |
| Ninja | 1.11.0 or newer | Primary generator. |
| Node.js | 24.x LTS | pnpm and React Native tooling baseline. |
| pnpm | 9.x or newer | Workspace package manager. |
| Python | 3.11+ | Used by React Native codegen scripts. |

## React Native runtime

| Component | Pin / Verify | Notes |
| --- | --- | --- |
| React Native | 0.86.0 | Target out-of-tree platform release. |
| React | 19.2.7 | Specified baseline. |
| Yoga | Bundled with RN 0.86.0 | Verify exact commit/tag in RN source. |
| Hermes | Bundled with RN 0.86.0 | Verify exact Hermes release used by RN 0.86.0; cross-compiled for QNX. |
| Folly | Bundled with RN 0.86.0 | Verify exact commit/tag in RN source. |
| glog | Bundled with RN 0.86.0 | Verify exact version in RN source. |
| Boost (headers) | Bundled with RN 0.86.0 | Verify exact version in RN source. |
| React Native CLI | Compatible with RN 0.86.0 | Verify CLI major version. |

## Expo and Metro

| Component | Pin / Verify | Notes |
| --- | --- | --- |
| Expo SDK | 57 | Target Expo baseline. |
| expo-modules-core | Compatible with Expo SDK 57 | Verify exact version. |
| Expo Router | Compatible with Expo SDK 57 | Verify exact version. |
| Metro | Compatible with RN 0.86.0 | Verify exact Metro release. |
| Babel | Compatible with RN 0.86.0/Expo SDK 57 | Verify exact preset versions. |
| TypeScript | 5.6 or newer | Required for modern React/Expo types. |

## Native interop

| Component | Pin / Verify | Notes |
| --- | --- | --- |
| Nitro Modules | 0.36.1 | Public native API layer. |
| Nitrogen (code generator) | Bundled with Nitro 0.36.1 | Generates HybridObject bindings. |

## Graphics and text

| Component | Pin / Verify | Notes |
| --- | --- | --- |
| Skia | TBD – verify RN 0.86.0 Skia commit | Used as initial renderer backend. |
| ICU | TBD | Text boundary analysis and locale data. |
| HarfBuzz | TBD | Complex-script shaping. |
| FreeType | TBD | Font rasterization. |
| libpng | TBD | PNG decode. |
| libjpeg-turbo | TBD | JPEG decode. |
| libwebp | TBD | WebP decode (BSP-dependent). |
| zlib | TBD | Compression. |

## QNX target libraries

| Component | Source | Notes |
| --- | --- | --- |
| QNX Screen | QNX SDP / BSP | Native display, window, buffer, and input integration. |
| EGL | QNX SDP / BSP | Graphics surface management. |
| OpenGL ES | QNX SDP / BSP | Initial GPU backend for Skia. |
| Vulkan / Vulkan SC | QNX SDP / BSP | Future backend; not required for Milestone 0. |
| TLS provider | OpenSSL or QNX-approved stack | Determined by BSP and security policy. |

## Verification commands

```bash
# QNX toolchain
qcc -V
q++ -V

# CMake
cmake --version

# Ninja
ninja --version

# Node.js
node --version

# pnpm
pnpm --version

# React Native CLI
npx @react-native-community/cli --version
```

## Updating this matrix

1. Propose the change in an ADR or decision log entry.
2. Update `docs/getting-started/compatibility-matrix.md`.
3. Update lockfiles (`pnpm-lock.yaml`, CMake dependency manifests).
4. Run `scripts/doctor.sh` and the affected build/test targets.
5. Record results in `docs/handoff/TEST_STATUS.md`.
