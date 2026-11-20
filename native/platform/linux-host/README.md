# Linux/Wayland MarathonOS adapter

The React Native Fabric/Hermes/Qt Quick host is in `vnext` and is shared with the QNX build. The Linux Marathon target, `rnqnx_marathon_host`, compiles `vnext/probes/playground/main.cpp` with `RNQNX_MARATHON_HOST=1` and links the existing `rnqnx_react_host`. It has no QNX Screen dependency. Configure it with `-DRNQNX_BUILD_MARATHON_HOST=ON`; `cmake/RnqnxMarathonHost.cmake` defines the target and the Metro bundle command.

## Nitro

`nitro/` holds the Marathon Nitro v1 adapter, built as `rnqnx_nitro_marathon`:

- `MarathonPlatformInfo` implements the shared `PlatformInfo` spec: `platform` returns `marathon`, `architecture` returns the CPU, `add(a, b)` adds. The generated binding is in `nitro/generated/`.
- `installMarathonNitroRuntime(runtime)` registers the class and installs `NitroModulesProxy` without a dispatcher. `installMarathonNitroRuntime(runtime, dispatcher)` also installs the dispatcher Nitro uses for Promises and callbacks, and throws on a null dispatcher.
- `MarathonNitroProof` holds the JavaScript and the native check behind `--nitro-proof`.

The host uses the adapter when configured with `-DRNQNX_MARATHON_HOST_NITRO=ON`, which also needs `-DRNQNX_ENABLE_NITRO_QT_DISPATCHER=ON` and `RNQNX_NITRO_CORE_ROOT` pointing at an isolated copy of `nitro-core` (the dispatcher patches edit those sources; see `cmake/RnqnxNitroQt.cmake`). With that set, `main.cpp` installs the adapter with a `QtNitroDispatcher` over the `QtReactHost` task queue, the same dispatcher the QNX Qt path uses. The queue runs JS work on the owner thread and drains accepted work when the host is destroyed. `rnqnx_nitro_qt_test` covers that behavior and is built and run in the same CI job.

`rnqnx_marathon_host --nitro-proof` evaluates JavaScript in the host's Hermes runtime that constructs `MarathonPlatformInfo` through `NitroModulesProxy`. The host then checks the stored values in C++ (`platform` is `marathon`, `add(2, 3)` is 5, `architecture` matches the native class) and prints `RNQNX_MARATHON_NITRO_OK`. It exits with status 1 and prints `RNQNX_MARATHON_NITRO_FAILED` otherwise.

`tests/native/nitro-v1-host` still compiles the adapter alone, without Qt or Hermes, as a quick check.

## CI

`.github/workflows/marathon-qt-host.yml` builds the host and runs it twice on `ubuntu-24.04`:

1. `-platform offscreen` with `--nitro-proof` and a screenshot, then the Nitro dispatcher tests.
2. `QT_QPA_PLATFORM=wayland` against a headless Weston, with `WAYLAND_DEBUG=client`. `tests/native/marathon-wayland/check-wayland-client-log.py` requires an `xdg_toplevel` created with `set_app_id("org.rnqnx.marathon.playground")`, an acknowledged configure, and a committed buffer.

The Wayland run is a protocol check against a generic compositor. It is not a MarathonOS device run, and it does not exercise the Marathon shell, sandbox or app launcher.

## Not implemented here

MarathonOS system services such as shell lifecycle and back, keyboard and IME, safe areas and insets, permissioned D-Bus, background tasks, audio, sensors, network, signing and packaging are not part of this directory yet. They wait on the upstream contracts.

Do not use direct unrestricted D-Bus or turn off bwrap as a workaround. Do not call the host an official `.marathon` app because a Wayland window opens.

See `docs/marathon/NATIVE_HOST.md` and upstream `docs/SECURITY_MODEL.md`, `docs/NATIVE_APP_LIFECYCLE.md` and `docs/APP_MANIFEST_SCHEMA.md`.
