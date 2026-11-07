# react-native-qnx

Foundation for a production-grade React Native out-of-tree platform for QNX.
The current line is Expo 58.0.22 / SDK 58, React Native 0.88.1, React 19.3.0,
Metro 0.87.1 and pinned Nitro v1 alpha 105 (core, RN adapter and Nitrogen).
The physical Pi 400 runs fullscreen React Native Hello World on QNX SDP 8.0.5;
optional Qt/Nitro callback and Promise tests also pass. This remains experimental
evidence from a private evaluation; graphics autostart and production gates remain open.

## Goal

Build an upstream-conscious React Native platform that runs on QNX Neutrino,
using Qt Quick component views over Qt's QNX QPA/Screen integration, Fabric for
mounting, Yoga for layout, and Hermes as the JavaScript engine. The direct-Screen
executable is a window-system probe, not the component backend. Nitro Modules
are the module architecture for application-facing APIs; Expo is a selected
integration target. Neither upstream package provides QNX support today, so
those seams are implemented and validated here.

## Current scope

**Current local validation:** dependency/source contracts, TypeScript, JS tests,
platform codegen and formatting pass. The full RN 0.88/Hermes/Qt 6.12 host suite
passes 13/13 CTests, including the optional owned Nitro dispatcher and source
patch contract. Hosted jobs are currently refused before runner startup because
of GitHub account billing/spending limits; local evidence is separate from CI.

**Physical QNX proof:** Pi 400/QNX 8.0.5 as qnxuser renders the exact Hello World
title at 1920×1080 through Fabric/Hermes/Qt Quick. The optional
[Qt dispatcher](docs/nitro/QT_DISPATCHER.md) passes callbacks, typed/void Promises,
errors, checkpoints and teardown races. See the
[device receipt](docs/evidence/pi400-nitro-qt-2026-11-07.md).

Historical SDP 8.0.2 Screen/C++20 compile/VM results remain separately labeled.
Broader Pi 4/5 QSTI, generated/third-party Nitro modules, Forge bindings, graphics
cold boot and production packaging are not established by this Hello World slice.
Qt uses the private evaluation through 17 November 2026, without a distribution claim.

See `docs/handoff/CURRENT_STATE.md`, `docs/handoff/QNX_VALIDATION_QUEUE.md`,
and `.project-state.json` for the exact evidence boundaries.

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

- Expo 58.0.22 / SDK 58
- React Native 0.88.1 and React 19.3.0
- Metro 0.87.1 and `@react-native/metro-config` 0.88.1
- `babel-preset-expo` 58.0.23
- Nitro core, RN adapter and Nitrogen `1.0.0-alpha.nightly.105.g69d365fc9b18`
- TypeScript 7.0.2
- Node.js 26+ with `@types/node` 26.6.6
- pnpm 12.8.1

The refreshed SDK-58 graph resolves `expo-modules-core` 58.0.29,
`@expo/metro` 58.0.0, and `@expo/metro-config` 58.0.20. React Native
0.88.1 resolves `hermes-compiler` 260318099.0.5 while its bundled Hermes
source tag is `hermes-v260318099.0.5`. Yoga remains the source snapshot bundled
by React Native and is not assigned an invented standalone version. Run
`pnpm validate:dependencies` to verify installed versions and audited upstream
source symbols.

The complete provenance and compatibility record is in `docs/getting-started/compatibility-matrix.md`.

The lockfile also scopes `xcode` to `uuid` 11.1.1 and validates the installed
`uuid.v4()` call site. Security findings are re-audited against the refreshed
SDK-58 lockfile rather than carrying forward the August 2026 audit result.

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

## Raspberry Pi 4/5 + experimental Pi 400 quick install

For the current QNX Everywhere QSTI lane, use QNX SDP 8.0.5 with QSTI 0.5.0
build 50 on Raspberry Pi 4 or Raspberry Pi 5. The stock target defaults are
`qnxuser@qnxpi.local`.

Raspberry Pi 400 is an **experimental custom-image target**, not part of
the stock Pi 4/5 QSTI baseline. QNX's Pi 400 recipe adapts the BCM2711 Pi 4
custom image with the Pi 400 DTB and Wi-Fi firmware. Follow
`docs/getting-started/raspberry-pi-400.md` before flashing, then use
`RNQNX_PI_BOARD=pi400` to label evidence from the same deployment lane.
Physical Pi 400 Hello World/Nitro evidence is recorded above; broader QSTI and component support remain separate gates.

```bash
export QNX_SDP_ENV=/path/to/qnxsdp-env.sh

pnpm pi:preflight
pnpm pi:install

# after the Screen lane is green:
pnpm qnx:runtime:validate
```

The install lane verifies AArch64le + Screen, builds the QNX AArch64 target,
deploys atomically, runs three clean Screen launch/exit cycles by default, stores
stdout/slog2/screenshot evidence, and automatically verifies + hashes the run
into `manifest.json`.

See `docs/getting-started/raspberry-pi-today.md` for stock Pi 4/5
flashing, login, and evidence collection. Use
`docs/getting-started/raspberry-pi-400.md` for the separate Pi 400 recipe.

## Build and deploy on QNX

For the current Pi/QSTI lane, run the QNX path from a supported x86-64
development host with a licensed QNX SDP 8.0.5 installation:

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

The Pi helpers default to `qnxuser@qnxpi.local`; set `QNX_TARGET_IP`,
`QNX_TARGET_USER`, or `QNX_SSH_KEY` only when your target differs. The
scripts never assume root SSH. Detailed prerequisites and commands are in
`docs/getting-started/build-and-deploy.md`.

## Optional premium Forge app compatibility

The opt-in Forge native slice reuses Nitro Forge's Rust engines and per-document
C++ adapter. Host and physical Pi400/QNX8.0.5 probes verify real Tally editing,
undo/redo, saved-document reopening, FIFO and owner teardown. The target build
uses an explicit experimental Rust std/dependency compatibility patch; Nitro
JavaScript bindings remain pending. See [the shared engine guide](docs/native-modules/forge-rust-qnx.md)
for build commands, exact target evidence and remaining ABI/lifecycle boundaries.

## License

MIT

## Premium component boundary — owner decision, November 7, 2026

Nitro Forge is a separately sold premium component product. React Native QNX provides generic native runtime, rendering, Nitro v1 module integration, package discovery/linking, lifecycle and platform services so licensed developers can install selected Forge packages in their own apps with minimal setup. Nitro Modules is infrastructure; Nitro Forge is a separate commercial product.

Do not make Forge a built-in platform/system component, default dependency, bundled sample requirement or prerequisite for platform build, boot, tests or release. Do not vendor premium components, engines, assets, binaries or entitlement code into platform distributions. Forge-specific production adapters and package configuration belong with Forge or the consuming app; generic reusable hooks belong here. Preserve existing product licensing and distribution terms.

The current default-OFF external-checkout adapter is a compatibility experiment, not permission to redistribute Forge or proof of seamless installation. Keep historical evidence; do not duplicate engines or remove working probes merely to change ownership boundaries. New integration must support install selected licensed package → import → build/run through documented tooling, without patching platform sources or hand-wiring each component. Verify ABI alignment, linkage, input, lifecycle, update and uninstall in an authorized consuming app. Core CI uses non-premium fixtures; commercial integration tests are explicitly opt-in and separately authorized. Removing Forge must leave the platform usable. Packaging must exclude external premium source/build outputs unless producing an expressly licensed consuming-app artifact.

These rules apply to QNX and the Marathon native target and override earlier roadmap language implying Forge is a platform component or required conformance workload. Compatibility remains a measured acceptance goal, not a shipped feature claim.
