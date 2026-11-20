# Native Qt Wayland host: phase M2 (experimental)

PR #35 builds a distinct Linux native executable from the existing `vnext/probes/playground/main.cpp`, linking to the same `rnqnx_react_host` (Hermes + Fabric + Qt Quick) as the host playground. The executable is named `rnqnx_marathon_host`. Its app identity (`org.rnqnx.marathon.playground`) and window title differ; the QNX target and `rnqnx_playground_host` are unchanged.

## Prerequisites

- The Marathon Metro platform (PR #34, merged 2026-11-02) supplies `metro.marathon.config.js`, the JS platform singleton and the workspace packages. Nothing needs merging first.
- Linux development host, Qt 6.11.1+ Core/Gui/Qml/Quick/Network and, for a real compositor, the Qt Wayland QPA client plugin, CMake 3.31.9+, Ninja, clang, and the repository's pinned pnpm/Node/RN/Hermes dependencies.
- For actual MarathonOS, build with its Linux toolchain and a runtime-compatible Qt/stdlib/libc. A glibc desktop build does not run on Alpine/musl.
- QNX SDP is not a dependency of this executable.

## Build and run

The offscreen steps below run in CI on every change to the host files. The Wayland step has not been run anywhere.

```bash
pnpm install --frozen-lockfile
cmake -S . -B build/marathon-host -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_TESTING=ON \
  -DCMAKE_C_COMPILER=clang \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DRNQNX_BUILD_HERMES=ON \
  -DRNQNX_FETCH_REACT_NATIVE_DEPS=ON \
  -DRNQNX_REQUIRE_FULL_HOST_RENDERER=ON \
  -DRNQNX_BUILD_MARATHON_HOST=ON
cmake --build build/marathon-host --target rnqnx_marathon_host

# Headless Qt rendering on a Linux build host. Verified in CI. NOT a Marathon device proof:
QT_QPA_PLATFORM=offscreen ./build/marathon-host/bin/rnqnx_marathon_host \
  -platform offscreen --screenshot marathon-frame.png

# Under a Wayland compositor. Not yet run: needs the Qt Wayland client plugin and a compositor socket.
export QT_QPA_PLATFORM=wayland
export WAYLAND_DISPLAY=<compositor socket name>
./build/marathon-host/bin/rnqnx_marathon_host
```

The same executable starts through `pnpm qnx marathon run` (see the [development launcher guide](../getting-started/marathon-development-launcher.md)), which checks the socket and the runtime directory first.

Desktop entry `apps/rn-playground/rnqnx-marathon-playground.desktop` is for development only, not signed `.marathon` distribution. Marathon's runner accepts QML entry points only (checked against upstream `1f845160` on 2026-11-19), so native-executable packaging needs an upstream decision first.

## Verified on development-host CI

All runs below succeeded and are on the head of PR #149 (`cbe27bf7`, merged 2026-11-19):

| Workflow           | Run                                                                                    | What it proves                                                                                                                                                               |
| ------------------ | -------------------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `marathon-qt-host` | [44492351468](https://github.com/mikevocalz/react-native-qnx/actions/runs/44492351468) | Qt 6.11.1 + Hermes + Fabric host compiles and links as `rnqnx_marathon_host`, renders an offscreen screenshot, and rebuilds the Marathon bundle when `qnx-asset.png` changes |
| `marathon-metro`   | [44492351488](https://github.com/mikevocalz/react-native-qnx/actions/runs/44492351488) | `pnpm test:js`, typecheck and dependency validation pass; QNX and Marathon production bundles build and are hashed                                                           |
| `nitro-v1-host`    | [44492351495](https://github.com/mikevocalz/react-native-qnx/actions/runs/44492351495) | Shared Nitro v1 core and the QNX and Marathon adapters compile on Linux (no runtime)                                                                                         |

The first successful native host job, on PR #35 before merge, is [run 41519519981](https://github.com/mikevocalz/react-native-qnx/actions/runs/41519519981/job/125531620418). These runs verify host build integration only. They say nothing about a MarathonOS device or its sandboxed application runner.

## Validation gates

| #   | Gate                                                                                                           | Status                                                                                        |
| --- | -------------------------------------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------- |
| 1   | Frozen install, JS tests and both Metro bundles succeed with the exact package graph                           | Verified in `marathon-metro` run above                                                        |
| 2   | Linux CMake target builds and links with no QNX headers or libscreen                                           | Verified: `tests/js/marathon-native-host-contract.test.mjs` asserts it, the CI build links    |
| 3   | Qt Wayland client plugin connects to a compositor socket and creates an `xdg_toplevel` with the desktop app id | Not run. Needs a compositor; command in the Build and run section                             |
| 4   | Hermes + Fabric mount content; touch, scroll and text work; log, screenshot and video saved                    | Mount and one offscreen frame verified. Touch, scroll and text on Wayland not run             |
| 5   | IME, back, lifecycle, cgroups, permissions                                                                     | Not implemented. Wait on upstream lifecycle and permission contracts                          |
| 6   | QNX SDP 8.0.5 probe and QNX Metro bundle keep passing                                                          | QNX bundle builds in `marathon-metro`. Pi 400 receipts are in `docs/evidence` (PR #149 run 1) |
| 7   | HackberryPi CM5 and a phone                                                                                    | Not run. Needed before any performance or "out of the box" claim                              |

## Open items needing owner or upstream

- Run gates 3, 4 (Wayland part) and 7. Owner action: provide a Linux machine with a Wayland compositor, then a Marathon VM image or device, and run the commands above. Save the log, `WAYLAND_DISPLAY`, compositor name and version, and a screenshot.
- Build against the device toolchain (Alpine/musl, Marathon's Qt). Record the Qt version and `ldd` output.
- Native lifecycle, back, IME, insets and permission mediation need the upstream contract described in [UPSTREAM_RFC](UPSTREAM_RFC.md). Do not work around bubblewrap or call unrestricted D-Bus.
- Signed `.marathon` packaging is blocked on upstream: the manifest schema requires a `.qml` `entryPoint`.

## Limitations and rollback

This is an opt-in experimental host. It does not package signed Marathon apps, guarantee sandbox behavior, or establish Android/Expo/Nitro module parity. Remove the option, the CMake include, the conditional host metadata, docs and tests to roll back. The QNX path stays as it is.
