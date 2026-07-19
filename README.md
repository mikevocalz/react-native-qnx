# react-native-qnx

Production-grade React Native out-of-tree platform for QNX, targeting QNX SDP 8.0.5, React Native 0.86.0, Expo SDK 57, and Nitro Modules 0.36.1.

## Goal

Build a real, upstream-conscious React Native platform that runs on QNX Neutrino using QNX Screen for display/input, Skia for rendering, Fabric for the renderer, Yoga for layout, and Hermes as the JavaScript engine. Application native APIs use Nitro Modules; Expo SDK 57 is supported through a first-class `qnx` platform layer.

## Repository layout

- `apps/` – example and test applications
- `packages/` – TypeScript/Node packages (`react-native-qnx`, `expo-qnx`, `metro-config-qnx`, etc.)
- `native/` – C++ platform runtime, Fabric renderer, QNX Screen integration, graphics, input, text, Hermes/JSI, Nitro, networking, storage, updates, diagnostics
- `cmake/` – CMake toolchain files and modules
- `template/qnx/` – QNX native project template
- `scripts/` – host bootstrap, doctor, configure, build, deploy, run, log
- `tests/` – C++ unit, integration, conformance, visual, performance, and stress tests
- `docs/` – architecture, ADRs, getting-started, component matrices, native-module guides, security, real-time, performance, troubleshooting, release

## Current milestone

**Milestone 0: Compatibility and Toolchain**

Deliver repository structure, dependency compatibility matrix, QNX CMake toolchain, host doctor, empty native executable built for QNX, target deployment, and CI foundation. Exit when a clean checkout can reproduce the native executable.

See `docs/handoff/CURRENT_STATE.md` and `.project-state.json` for the latest state.

## Quick start

```bash
# 1. Bootstrap the Ubuntu 24.04 host (must run on the build workstation)
./scripts/bootstrap-host.sh

# 2. Verify the QNX SDP license and environment
./scripts/verify-qnx-license.sh

# 3. Run the doctor to validate the environment
./scripts/doctor.sh

# 4. Configure the QNX build
./scripts/configure-qnx.sh aarch64 Debug

# 5. Build
./scripts/build-qnx.sh aarch64 Debug

# 6. Deploy to a target
./scripts/deploy-qnx.sh aarch64 apps/rn-playground
```

For detailed instructions see `docs/getting-started/build-and-deploy.md`.

## License

TBD – project license to be selected by maintainers.
