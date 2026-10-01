# react-native-qnx

Foundation for a production-grade React Native out-of-tree platform for QNX.
The current development line is QNX SDP 8.0.5 for the September 2026 Pi QSTI,
Expo SDK 58.0.0, React Native 0.88.0-rc.3, React 19.3.0, Metro 0.87.1, and
Nitro Modules/Nitrogen 0.37.1. Existing QNX cross-build evidence from SDP 8.0.2
is retained as historical evidence and is not silently promoted to 8.0.5 proof.

## Goal

Build an upstream-conscious React Native platform that runs on QNX Neutrino,
using Qt Quick component views over Qt's QNX QPA/Screen integration, Fabric for
mounting, Yoga for layout, and Hermes as the JavaScript engine. The direct-Screen
executable is a window-system probe, not the component backend. Nitro Modules
are the module architecture for application-facing APIs; Expo is a selected
integration target. Neither upstream package provides QNX support today, so
those seams are implemented and validated here.

## Current scope

**The React Native host renderer exists and has rendered end to end.** The
implementation bundles `apps/rn-playground` for the `qnx` platform, runs
Hermes/React/Fabric, and paints Qt Quick items using Yoga layout. The complete
render path was established on the previous dependency baseline. Expo SDK 58 /
React Native 0.88 source-contract and host-build revalidation is required before
that earlier host result is reported as an SDK-58 validation result.

What exists: React Native's renderer built from source, a Qt Quick mounting
layer, the `ShadowViewMutation` adapter, a Hermes runtime host with the
scheduler, event beat, component registry and TurboModule adapter React Native
requires, the JavaScript platform package, and a Metro configuration.
`docs/component-support.md` is the honest table of what works — today that is
`View` with layout, background, radius, border and opacity, and little else.

**None of this has run on QNX.** Every result is from the development host
(`docs/adr/0010-macos-qt-development-host.md`). A real QNX configure, compile,
deploy, or runtime result requires a licensed QNX SDP on a supported host and a
target; see `docs/handoff/BLOCKERS.md`. Host tests are not QNX proof.

See `docs/handoff/CURRENT_STATE.md`, `docs/handoff/QNX_VALIDATION_QUEUE.md`, and `.project-state.json` for the latest state.

## Repository layout

- `apps/` – example and conformance application scaffolds
- `packages/` – out-of-tree platform, Expo, Metro, Nitro, CLI, codegen, and developer-tool package scaffolds
- `native/` – shared native code and QNX/Linux/macOS platform directories
- `cmake/` – CMake toolchain files and modules
- `template/qnx/` – QNX native project template
- `scripts/` – bootstrap, doctor, validation, configure, build, deploy, run, and log tooling
- `tests/` – host C++ tests plus future integration, conformance, visual, performance, and stress suites
- `docs/` – architecture, ADRs, getting-started material, validation queues, and handoff state

## Dependency baseline

Workspace dependencies are exact pins, not ranges:

- Expo 58.0.0 / SDK 58
- React Native 0.88.0-rc.3 and React 19.3.0
- Metro 0.87.1 and `@react-native/metro-config` 0.88.0-rc.3
- `babel-preset-expo` 58.0.4
- Nitro Modules and Nitrogen 0.37.1
- TypeScript 7.0.2
- Node.js 26+ with `@types/node` 26.6.3
- pnpm 12.8.1

The refreshed lockfile resolves `expo-modules-core` 58.0.9,
`@expo/metro` 58.0.0-rc.0, and `@expo/metro-config` 58.0.6. React Native 0.88.0-rc.3 resolves `hermes-compiler` 260318099.0.4 while its bundled Hermes source tag is `hermes-v260318099.0.2`. Yoga remains the source snapshot bundled by React Native and
is not assigned an invented standalone version. Run
`pnpm validate:dependencies` to verify installed versions and audited upstream
source symbols.

The complete provenance and compatibility record is in `docs/getting-started/compatibility-matrix.md`.

The lockfile also scopes `xcode` to `uuid` 11.1.1 and validates the installed
`uuid.v4()` call site. Security status is re-audited against the refreshed SDK
58 lockfile rather than carrying forward August 2026 audit conclusions.

## Validate on macOS

macOS validation does not require QNX SDP:

```bash
./scripts/bootstrap-macos.sh
pnpm validate:macos
```

This validates formatting, TypeScript, installed dependency/source contracts, JavaScript checks, and the QNX Screen wrapper against the test-only host fake. It does not compile against QNX headers or run on a QNX target.

Install Qt 6.11.1 or newer to build the Qt Quick host targets as well
(`brew install qt`). They are optional: without Qt the suite configures, builds,
and passes everything else, and CMake reports the skip. Set `CMAKE_PREFIX_PATH`
if Qt is installed somewhere CMake does not search by default. Host Qt is a
development dependency under `docs/adr/0010-macos-qt-development-host.md`; the
QNX target still builds the pinned Qt LTS from source.

## Run the playground

The playground is a React Native app rendered by this platform. Building it
requires Qt and Hermes:

```bash
cmake -S . -B build/playground -G Ninja -DCMAKE_BUILD_TYPE=Debug -DRNQNX_BUILD_HERMES=ON
cmake --build build/playground --target rnqnx_playground_host
./build/playground/bin/rnqnx_playground_host
```

Metro bundles `apps/rn-playground` for the `qnx` platform as part of the build.
On a machine with no display, render one frame to a file instead:

```bash
./build/playground/bin/rnqnx_playground_host -platform offscreen --screenshot frame.png
```

This runs on the development host, not on QNX. See
`docs/adr/0010-macos-qt-development-host.md` for what that does and does not
prove, and `docs/component-support.md` for what actually works.

## Raspberry Pi 4/5 quick install

For a physical Pi running the current QNX Everywhere QSTI, the repository now has a dedicated same-day lane. It defaults to `qnxuser@qnxpi.local`, validates the AArch64/QNX Screen target, builds a Release binary, deploys it atomically, runs it, and captures stdout, a Screen screenshot when available, and recent slog2 output:

```bash
# Source your licensed QNX SDP environment first, or point at it:
export QNX_SDP_ENV=/path/to/qnxsdp-env.sh

./scripts/install-pi.sh
# or: pnpm pi:install
```

As of October 1, 2026, QNX's current Raspberry Pi 4/5 QSTI is the 0.5.0 build-50 line for QNX SDP 8.0.5. The repository's prior cross-compile evidence was captured on SDP 8.0.2, so the new preflight records the actual compiler and target identity instead of silently treating those versions as interchangeable.

This lane proves the native QNX Screen/device path. The complete React Native + Hermes + Qt Quick renderer is still a separate target-integration milestone and is not represented as Pi-ready yet.

See `docs/getting-started/raspberry-pi-today.md` for the flash, first-login, SSH, troubleshooting, and Pi 4/Pi 5 details.

## Build and deploy on QNX

Run the QNX path on a supported x86-64 development host with a licensed QNX
SDP installation. Use SDP 8.0.5 for the current Pi QSTI lane:

```bash
# Install freely redistributable host prerequisites.
./scripts/bootstrap-host.sh

# Source the licensed SDP environment first, then verify it.
./scripts/verify-qnx-license.sh
./scripts/doctor.sh

# First pass G0-A with the explicit Pi host/user environment configured.
./scripts/validate-g0-a.sh

# Configure and build the AArch64 Debug target.
./scripts/configure-qnx.sh aarch64 Debug
./scripts/build-qnx.sh aarch64 Debug

# Deploy and run the actual CMake target name.
./scripts/deploy-qnx.sh aarch64 rnqnx_playground Debug
./scripts/run-qnx.sh aarch64 rnqnx_playground
```

The Pi workflow defaults to the stock QSTI identity
`qnxuser@qnxpi.local`; override `QNX_TARGET_IP`, `QNX_TARGET_USER`, or
`QNX_SSH_KEY` only when your target differs. The scripts never assume root
SSH. Detailed prerequisites and commands are in
`docs/getting-started/build-and-deploy.md`.

## License

MIT
