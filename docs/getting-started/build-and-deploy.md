# Host Validation, QNX Build, and Target Deployment

This document separates the freely reproducible host-validation path from the licensed QNX build and runtime path.

## Supported paths

- **macOS or Linux host validation:** formatting, TypeScript, dependency/source-contract audit, JavaScript checks, and sanitizer-backed C++ host tests. QNX SDP is not required.
- **QNX build and target validation:** canonical x86-64 Ubuntu 22.04 with a
  licensed QNX SDP 8.0 installation, followed by deployment to the QSTI Pi.

Host tests compile the concrete Screen wrapper against a test-only implementation of the documented Screen C API. They validate ownership and call sequencing, but they do not establish QNX header, ABI, driver, compositor, or runtime compatibility.

## Validate on macOS

From the repository root:

```bash
./scripts/bootstrap-macos.sh
pnpm validate:macos
```

The dependency audit verifies the exact workspace pins (including pnpm 11.21.0, React Native 0.86.2, React 19.2.3, Expo 57.0.12, Metro 0.84.4, Nitro/Nitrogen 0.36.5, and TypeScript 6.0.3), resolved Expo packages, Hermes source identity, and installed source symbols.

Do not record this result as a QNX compile or runtime pass. QNX-specific follow-up belongs in `docs/handoff/QNX_VALIDATION_QUEUE.md`.

## QNX host requirements

- x86-64 Ubuntu 22.04 LTS for the canonical QNX/Qt cross-build
- QNX SDP 8.0 installed through QNX Software Center, with its exact installed
  update/build recorded during G0-A
- An active QNX license
- BSP/graphics packages that provide the target Screen implementation
- Network access to the QNX target device

QNX SDP is licensed software and is not installed by this repository.

## Install freely redistributable Ubuntu tools

```bash
cd react-native-qnx
./scripts/bootstrap-host.sh
```

This installs the non-QNX host prerequisites, including CMake, Ninja, Node.js, pnpm, and Python. It does not install or license QNX SDP.

## Install and activate QNX SDP 8.0

1. Install QNX Software Center through your licensed QNX channel.
2. Install QNX SDP 8.0 and the current Raspberry Pi 4 QSTI/BSP/graphics
   packages for the target board.
3. Activate the license.
4. Source the SDP environment script in every build shell. For example:

```bash
source <qnx-installation>/qnxsdp-env.sh
```

The actual path is installation-specific. Do not copy proprietary SDK headers, libraries, or sources into this repository.

## Verify the QNX environment

```bash
./scripts/verify-qnx-license.sh
./scripts/doctor.sh
```

The doctor checks:

- host OS and architecture
- `QNX_HOST` and `QNX_TARGET`
- `qcc` / `q++`
- QNX Screen and EGL headers/libraries
- CMake, Ninja, Node.js, pnpm, and Python
- installed React Native/Expo/Nitro version and source contracts
- repository structure

Resolve every error before configuration. In particular, an unset SDP environment or missing Screen development files is a hard blocker, not a condition to bypass with a host stub.

## Prove G0-A on the Pi

The current QSTI documentation uses the `qnxuser` account, but verify the user
and key/password configuration on the installed image. Root is never assumed.

```bash
export QNX_TARGET_IP=qnxpi.local
export QNX_TARGET_USER=qnxuser
# Set QNX_SSH_KEY only when the key is not already selected by SSH config/agent.
export QNX_SSH_KEY=<private-key-path>

./scripts/validate-g0-a.sh
```

This is the first executable gate. It captures `qcc -V`, cross-builds the
concepts/ranges/designated-initializer/`std::jthread` target with preset
`qnx-pi4`, runs that exact binary on the Pi, checks its output, and measures the
deploy/run portion. A host build of the same source is only a source sanity
check.

## Configure

For an AArch64 QNX target in Debug configuration:

```bash
./scripts/configure-qnx.sh aarch64 Debug
```

For an x86-64 QNX target:

```bash
./scripts/configure-qnx.sh x86_64 Debug
```

`Release` is also accepted as the second argument. The scripts map the architecture/configuration pair to a checked-in CMake preset and recreate its configure state with `cmake --fresh`.

## Build

```bash
./scripts/build-qnx.sh aarch64 Debug
```

The AArch64 Debug executable is expected at:

```text
build/qnx-aarch64-debug/bin/rnqnx_playground
```

That artifact is not considered proven until this command succeeds with the licensed QNX toolchain and the resulting executable is tested on the target.

## Deploy

Set the target address and verified user:

```bash
export QNX_TARGET_IP=qnxpi.local
export QNX_TARGET_USER=qnxuser
```

Deploy the exact CMake target name and configuration:

```bash
./scripts/deploy-qnx.sh aarch64 rnqnx_playground Debug
```

The deploy script selects `build/qnx-aarch64-debug/bin/rnqnx_playground`, copies it to `/tmp/rnqnx_playground`, and makes it executable. Use `Release` only after building the matching Release preset.

## Run and inspect

```bash
./scripts/run-qnx.sh aarch64 rnqnx_playground
```

The current direct-Screen program is intended to create a 1280×720 application
window, alternate ARGB blue and black fills for up to 600 frames, present each
render buffer, and clean up Screen resources. It is a window-system probe below
the locked Qt Quick backend. Record the actual result—including failures—in
`docs/handoff/TEST_STATUS.md` and update the matching queue entries.

View target logs with:

```bash
./scripts/log-qnx.sh
```

## macOS remote-development option

macOS is a supported local authoring and host-validation environment. It may also be used as a client for the QNX build host:

1. Run `pnpm validate:macos` locally while editing.
2. Connect to the licensed Ubuntu/QNX host through SSH or an editor's remote-development feature.
3. Run the doctor, configure, build, deploy, and target-test commands on that Ubuntu host.

Do not attempt to treat the macOS host compiler or the test-only Screen fake as a substitute for the QNX toolchain.

## Troubleshooting

### `QNX_HOST` or `QNX_TARGET` is unset

Source the installation-specific SDP environment script, then rerun the license verifier and doctor.

### `qcc` is not found

Confirm `${QNX_HOST}/usr/bin` is on `PATH`. The official SDP environment script normally configures it.

### QNX Screen headers or libraries are missing

Install the appropriate graphics development package/BSP component through the licensed QNX Software Center channel. Stop if the installed SDK does not expose the APIs used by `native/platform/qnx/screen/QnxScreenSurface.cpp`; update the wrapper only from installed headers/documentation, never from a fabricated API shape.

### CMake cannot find the QNX toolchain

Use the repository script so the architecture and preset remain consistent:

```bash
./scripts/configure-qnx.sh aarch64 Debug
```

If it fails, preserve the exact configure error in the handoff rather than bypassing the toolchain checks.

## Next step

Pass G0-A, then G0-D, before beginning the Qt G0-B source build/on-glass proof.
Only after G0-B and G0-C may P1 Hermes begin. None of Qt Quick, Hermes, Fabric,
Nitro, or Expo currently has a demonstrated QNX implementation in this
repository.
