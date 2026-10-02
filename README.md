# react-native-qnx

Foundation for a production-grade React Native out-of-tree platform for QNX.
The current JavaScript/tooling line is Expo SDK 58.0.0, React Native
0.88.0-rc.3, React 19.3.0, Metro 0.87.1, and Nitro Modules/Nitrogen 0.37.1.
Current host validation is green; QNX SDP 8.0.2 compile/VM results are retained
as historical evidence while current SDP 8.0.5/QSTI physical-target validation
remains pending.

## Goal

Build an upstream-conscious React Native platform that runs on QNX Neutrino,
using Qt Quick component views over Qt's QNX QPA/Screen integration, Fabric for
mounting, Yoga for layout, and Hermes as the JavaScript engine. The direct-Screen
executable is a window-system probe, not the component backend. Nitro Modules
are the module architecture for application-facing APIs; Expo is a selected
integration target. Neither upstream package provides QNX support today, so
those seams are implemented and validated here.

## Current scope

**SDK-58 host/tooling validation is green.** The merged `main` branch passes
frozen dependency installation, Prettier, dependency/source contracts,
TypeScript 7, peer checks, a production Metro bundle for `--platform qnx`,
repository architecture/codegen checks, Linux C++ host smoke tests, and the
dependency security audit.

**The full Qt/Hermes host renderer has historical end-to-end evidence.** The
JSX → Metro → Hermes → React/Fabric → Qt Quick pipeline was proven on the prior
React Native 0.86 / Expo 57 host line. The current SDK-58 CI does not install
the optional Qt/Hermes host stack, so that complete renderer path still needs a
deliberate RN 0.88 rerun rather than inheriting the old result.

**QNX proof is split by layer.** The native Screen/C++20 probes cross-built for
AArch64 and ran in a QNX x86_64 VM on the historical SDP 8.0.2 line. The full
React Native + Hermes + Qt Quick runtime has not run on QNX. The repository now
has a physical Pi 4/5 install/evidence lane for the current SDP 8.0.5 / QSTI
0.5.0 build-50 baseline, but that physical run has not yet been captured.

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

- Expo 58.0.0 / SDK 58
- React Native 0.88.0-rc.3 and React 19.3.0
- Metro 0.87.1 and `@react-native/metro-config` 0.88.0-rc.3
- `babel-preset-expo` 58.0.4
- Nitro Modules and Nitrogen 0.37.1
- TypeScript 7.0.2
- Node.js 26+ with `@types/node` 26.6.3
- pnpm 12.8.1

The refreshed SDK-58 graph resolves `expo-modules-core` 58.0.9,
`@expo/metro` 58.0.0-rc.0, and `@expo/metro-config` 58.0.6. React Native
0.88.0-rc.3 resolves `hermes-compiler` 260318099.0.4 while its bundled Hermes
source tag is `hermes-v260318099.0.2`. Yoga remains the source snapshot bundled
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

## Raspberry Pi 4/5 quick install

For the current QNX Everywhere QSTI lane, use QNX SDP 8.0.5 with QSTI 0.5.0
build 50 on Raspberry Pi 4 or Raspberry Pi 5. The stock target defaults are
`qnxuser@qnxpi.local`.

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

See `docs/getting-started/raspberry-pi-today.md` for flashing, first login,
SSH details, evidence layout, and troubleshooting.

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

## License

MIT
