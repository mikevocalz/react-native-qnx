# Installed native packages on Qt hosts

Applications can link optional Nitro libraries from their installed production
dependency graph. The platform supplies a single native runtime and its owned
Qt JavaScript dispatcher. Each selected package supplies its C++ target and
class registration function. Product libraries remain application dependencies.

## Package metadata

A package can declare `rnqnxNative` in `package.json` with `schemaVersion: 1` and
a `platforms` object containing a `qnx` and/or `marathon` descriptor.

| Descriptor field       | Meaning                                                                            |
| ---------------------- | ---------------------------------------------------------------------------------- |
| `cmakeListsPath`       | Package-relative path ending in `CMakeLists.txt`                                   |
| `target`               | CMake target the package entry creates                                             |
| `registrationHeader`   | Header exposed by that target's public include directories                         |
| `registrationFunction` | C++ function called after native runtime installation and before bundle evaluation |
| `nitroCoreVersions`    | Exact supported core versions, checked against the host target                     |
| `requiresDispatcher`   | Whether asynchronous methods require the real Qt JavaScript dispatcher             |

Discovery reads JSON without evaluating package entry points or configuration
JavaScript. It follows installed production and optional dependencies, including
nested and pnpm-linked installs. Missing optional dependencies are ignored;
missing required dependencies fail with an installation diagnostic. Packages
without metadata remain ordinary JavaScript dependencies. Expo modules keep
their separate Expo autolinking contract.

The prepared native configuration records package identity, canonical package
root, manifest hash, build entry, requirements and registration. CMake rechecks
the descriptor, manifest identity, path containment, duplicate targets and exact
native core version before linking. Changed package manifests require preparing
the app again. Metadata is a build declaration, not a device acceptance claim.

## Consumer flow

1. Install the selected package and its declared peer versions in the application.
2. Bundle with the selected QNX or Marathon Metro configuration.
3. Run `project prepare-native` to record the bundle and installed native packages.
4. Run `project build-native` with the selected platform SDK and any package-specific
   native build inputs. Packages needing Nitro enable the shared core and Qt
   dispatcher. A source-built Rust library can require its own target archive.
5. The native executable installs the selected platform runtime, registers the
   selected package classes and evaluates the bundle.

QNX consumers use `installQnxNitroRuntime`. Marathon consumers use
`installMarathonNitroRuntime`, including the owned-dispatcher overload for async
packages. The latter never installs `QnxPlatformInfo`. The native platform is
selected by the app configuration, not inferred from a shared Qt implementation.

The current host pins Nitro
`1.0.0-alpha.nightly.105.g69d365fc9b18`. `build-native` verifies the app's installed
core matches that pin, copies it under its own `native-dependencies` build
directory and applies the existing reviewed dispatcher patches there. It never
upgrades the host or edits `node_modules`. Reused copies reject redirected parent
paths, aliases to the installed core and any symlink in the native source tree.
The resulting native target advertises `NITRO_CORE_VERSION` and the explicit
`rnqnx-qt-alpha105` source compatibility profile.

## Integration checks

`tests/js/native-package-discovery.test.mjs` verifies actual filesystem discovery,
transitive and pnpm installs, metadata validation, CMake version/dispatcher gates,
changed manifests and build-copy containment. Existing external consumer
configuration/build-plan checks still apply.

`tests/native-package-consumer` is an opt-in native integration harness for a
prepared external app. It compiles the selected package's actual C++ target,
uses the existing Qt task queue and Nitro dispatcher with a real Hermes runtime,
and checks that only the selected native platform class is installed. Configure
it with the selected installed source roots and matching native build artifacts:

```bash
cmake -S tests/native-package-consumer -B build/native-package-consumer \
  -DRNQNX_NITRO_CORE_ROOT=/absolute/path/to/owned-nitro-core-copy \
  -DRNQNX_REACT_NATIVE_DIR=/absolute/path/to/react-native \
  -DRNQNX_NITRO_ROOT=/absolute/path/to/react-native-nitro-modules \
  -DHERMES_SOURCE_DIR=/absolute/path/to/pinned-hermes-source \
  -DHERMES_BUILD_DIR=/absolute/path/to/matching-hermes-build \
  -DNATIVE_PACKAGE_CONFIG=/absolute/path/to/app/build/marathon/native-app.json \
  -DCMAKE_PREFIX_PATH=/absolute/path/to/qt
cmake --build build/native-package-consumer --target native_package_consumer
ctest --test-dir build/native-package-consumer --output-on-failure
```

The harness uses `nativePackageDone` and `nativePackageResult` globals for an
asynchronous package scenario. Product integration can supply its own actual
public-API bundle; no product library is vendored or required by the platform.
The recorded Ignition consumer opens/reopens eleven real engines and exercises
tasks, media cache, binary results, FIFO, undo/redo and error/close behavior.

Linux host execution of a QNX-configured bundle is a host preview. A successful
Marathon adapter probe does not establish Wayland compositor/session behavior.
This harness does not run React/Fabric UI, native accessibility or the device
hardware matrix; retain those independent acceptance gates.
