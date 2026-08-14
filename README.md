# react-native-qnx

Foundation for a production-grade React Native out-of-tree platform for QNX.
The current compatibility line is QNX SDP 8.0 (installed update/build still to
be captured), React Native 0.86.2, React 19.2.3, Expo 57.0.12, Metro 0.84.4, and
Nitro Modules/Nitrogen 0.36.5.

## Goal

Build an upstream-conscious React Native platform that runs on QNX Neutrino,
using Qt Quick component views over Qt's QNX QPA/Screen integration, Fabric for
mounting, Yoga for layout, and Hermes as the JavaScript engine. The current
direct-Screen executable is a window-system probe, not the component backend.
Nitro Modules and Expo are selected integration targets, but their installed
upstream packages do not currently provide QNX support; those seams must be
implemented and validated in this repository.

## Current scope

The repository is in **Milestone 0: Compatibility and Toolchain**. It currently provides the workspace structure, dependency/source-contract validation, CMake presets, QNX toolchain scripts, a concrete QNX Screen surface, and host-side tests for that surface.

macOS and Linux are supported for host authoring and validation. A real QNX
configure, compile, deploy, or runtime result still requires a licensed QNX SDP
8.0 installation on a supported host; x86-64 Ubuntu 22.04 is the canonical
QNX/Qt cross-build environment for this repository. Host tests do not
constitute QNX proof.

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

- React Native 0.86.2 and React 19.2.3
- Expo 57.0.12
- Metro 0.84.4
- Nitro Modules and Nitrogen 0.36.5
- TypeScript 6.0.3
- pnpm 11.21.0

The Expo dependency graph resolves `expo-modules-core` 57.0.10, `@expo/metro` 56.0.0, and `@expo/metro-config` 57.0.8. React Native 0.86.2 identifies its Hermes source as `hermes-v250829098.0.16`; Yoga is the bundled React Native source snapshot and is not assigned an invented standalone version. Run `pnpm validate:dependencies` to verify the installed versions and audited upstream source symbols.

The complete provenance and compatibility record is in `docs/getting-started/compatibility-matrix.md`.

The lockfile also scopes `xcode` to patched `uuid` 11.1.1 and validates its
installed `uuid.v4()` call site. Two unpatched `image-size` denial-of-service
advisories remain in Metro's development-only graph; no fixed package is
published as of 2026-08-13. See `docs/handoff/BLOCKERS.md` before enabling Metro
for untrusted assets.

## Validate on macOS

macOS validation does not require QNX SDP:

```bash
./scripts/bootstrap-macos.sh
pnpm validate:macos
```

This validates formatting, TypeScript, installed dependency/source contracts, JavaScript checks, and the QNX Screen wrapper against the test-only host fake. It does not compile against QNX headers or run on a QNX target.

Install Qt 6.8 or newer to build the Qt Quick host targets as well
(`brew install qt`). They are optional: without Qt the suite configures, builds,
and passes everything else, and CMake reports the skip. Set `CMAKE_PREFIX_PATH`
if Qt is installed somewhere CMake does not search by default. Host Qt is a
development dependency under `docs/adr/0010-macos-qt-development-host.md`; the
QNX target still builds the pinned Qt LTS from source.

## Build and deploy on QNX

Run the QNX path on the x86-64 Ubuntu 22.04 workstation with a licensed QNX SDP
8.0 installation:

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

Set both `QNX_TARGET_IP` and the verified `QNX_TARGET_USER` before deployment;
the scripts never assume root SSH. Detailed prerequisites and commands are in
`docs/getting-started/build-and-deploy.md`.

## License

MIT
