# Native Qt Wayland host: phase M2 (experimental)

This PR **builds a distinct Linux native executable** from the existing `vnext/probes/playground/main.cpp`, linking to the same `rnqnx_react_host` (Hermes + Fabric + Qt Quick) as the host playground. The executable is named `rnqnx_marathon_host`. Its app identity/window title differ; the original QNX branch and `rnqnx_playground_host` remain unchanged.

## Prerequisites

- First merge the Marathon Metro platform PR (which supplies `metro.marathon.config.js`, the JS platform singleton, and the new workspace packages).
- Linux development host, Qt 6.11.1+ Core/Gui/Qml/Quick/Network and Qt Wayland QPA client plugin, CMake 3.31.9+, Ninja, the exact repository pnpm/Node/RN/Hermes dependencies.
- For actual MarathonOS, build with its Linux toolchain and runtime compatible Qt/stdlib/libc; do not transplant a glibc desktop build into Alpine/musl.
- QNX SDP is **not** a dependency of this executable.

## Build and run — commands to be validated in CI/device

```bash
pnpm install --frozen-lockfile
cmake -S . -B build/marathon-host -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_TESTING=ON \
  -DRNQNX_BUILD_HERMES=ON \
  -DRNQNX_FETCH_REACT_NATIVE_DEPS=ON \
  -DRNQNX_BUILD_MARATHON_HOST=ON
cmake --build build/marathon-host --target rnqnx_marathon_host

# Run under an actual Marathon Wayland compositor:
export QT_QPA_PLATFORM=wayland
export WAYLAND_DISPLAY=marathon-wayland-0
./build/marathon-host/bin/rnqnx_marathon_host

# Alternative: headless Qt rendering on a Linux build host, NOT a Marathon device proof:
./build/marathon-host/bin/rnqnx_marathon_host -platform offscreen --screenshot marathon-frame.png
```

Desktop entry `apps/rn-playground/rnqnx-marathon-playground.desktop` is for **development only**, not native signed `.marathon` distribution. Marathon's runner currently documents QML/Keel entrypoints; schema/runner work must be approved upstream first.

## Verified on development-host CI

The Linux Qt 6.11 / Hermes / Fabric host was compiled, linked, and run with
an offscreen screenshot in the successful [native host CI job](https://github.com/mikevocalz/react-native-qnx/actions/runs/41519519981/job/125531620418).
This verifies host build integration **only**, not a MarathonOS device or its
sandboxed application runner. The CMake dependency list includes the imported
`qnx-asset.png`, and CI verifies its timestamp invalidates the Marathon bundle
in an incremental Ninja build. No content change to the asset is needed.

## Validation gates (some host-only, target work pending)

1. Frozen install/JS tests and both Metro bundles succeed with exact installed package graph.
2. Linux CMake target builds and links with zero QNX headers/libscreen dependencies.
3. Qt with actual Wayland client plugin connects to Marathon socket and creates xdg_toplevel matching the desktop app id.
4. React Native bundle executes Hermes + Fabric, mounts content, touch/scroll/text works; save log/screenshot/video.
5. IME, back, lifecycle, cgroups and permissions are separately tested; this PR does NOT implement them.
6. QNX SDP 8.0.5 probe and existing QNX Metro bundle continue to pass independently.
7. Physical HackberryPi CM5 and target phone proofs are needed before performance or 'out of box' claims.

## Limitations and rollback

This is an opt-in experimental host. It does NOT package signed Marathon apps, guarantee sandbox behavior, or establish Android/Expo/Nitro module parity. Remove the option/CMake include, conditional host metadata, docs and tests to roll back. Preserve source for the existing QNX path.
