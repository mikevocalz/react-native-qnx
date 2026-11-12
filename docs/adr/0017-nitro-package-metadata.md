# ADR-0017: QNX native build metadata for Nitro packages

## Status

Accepted, 2026-11-12. The project owner delegated this decision. Discovery,
the descriptor, the build plan and the CMake reader have host tests, and the
generated registration header compiles and runs on the host. No third-party
Nitro module has been cross-built with the QNX SDK or run on a device under
this format yet.

## Context

An app outside this repository can depend on a Nitro module that is also
published outside it. The consumer build (`project prepare-native`, then
`project build-native`) had no way to learn that such a module has QNX C++
code, where its CMake project is, or how to register its hybrid classes. The
only Nitro code in a consumer executable was the platform's own
`rnqnx_nitro_qnx` runtime, linked when that target happened to exist.

What already exists:

- Expo modules declare QNX support in `expo-module.config.json` as
  `platforms: ["qnx"]` plus a `qnx` object with `target`, `cmakeListsPath` and
  `modules`. `@rn-qnx/expo-qnx-autolinking` reads it through Expo's
  discovery and emits `add_subdirectory` lines.
- Nitro's `nitro.json` belongs to nitrogen. Its schema at
  `1.0.0-alpha.nightly.136` (`nitrogen/lib/config/NitroUserConfig.js`) is a
  closed object: unknown keys are not part of the contract, and
  `autolinking` is now a `z.never()` field.
- nitrogen already reads one tool-specific field from a package's own
  `package.json`: `swiftpmConfig` (`checkSwiftPMConfig.js`).
- nitrogen's Android output registers classes from one generated C++ function,
  `<cxxLibName>OnLoad.cpp`'s `void registerAllNatives()`, which calls
  `HybridClassRegistry::registerHybridClass(...)` per class. This repo's own
  `installQnxNitroRuntime` registers `QnxPlatformInfo` the same way before
  `margelo::nitro::install(runtime)`.
- React Native CLI autolinking reads `react-native.config.js`, which is code.
  `project doctor` promises not to import config or package code.

## Decision

A Nitro module declares its QNX build under a `react-native-qnx` key in its
own `package.json`:

```json
{
  "name": "react-native-qnx-battery",
  "version": "2.1.0",
  "react-native-qnx": {
    "nitro": {
      "platforms": ["qnx"],
      "cmakeListsPath": "qnx/CMakeLists.txt",
      "target": "QnxBattery",
      "initializer": "margelo::nitro::battery::registerQnxHybridClasses"
    }
  }
}
```

| Field            | Rule                                                                                                                                                   |
| ---------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------ |
| `platforms`      | Nonempty array without repeats. Only `"qnx"` is accepted today.                                                                                        |
| `cmakeListsPath` | Package-relative path whose last segment is `CMakeLists.txt`. No `..`, `.`, absolute paths, backslashes, symlinks or `;`, `"`, `$`.                    |
| `target`         | The CMake target that `CMakeLists.txt` defines, 1-128 characters of `[A-Za-z0-9_.+-]`, starting with a letter or `_`. The `rnqnx_` prefix is reserved. |
| `initializer`    | A C++ function `void()` named with `::` separators and no leading `::`. It registers the package's hybrid classes.                                     |

All four fields are required. Any other key under `react-native-qnx` or
`react-native-qnx.nitro` is an error, so a typo fails instead of being ignored.

The pipeline:

1. `prepare-native` reads the app's `package.json` `dependencies` (not
   `devDependencies`). For each name it walks `node_modules/<name>` up the
   parent directories, as Node's lookup does, without reading `exports` or
   running a resolver. pnpm symlinks are followed with `realpath`. Packages
   without the key, and dependencies that are not installed, are skipped. A
   package with the key must have a matching `name`, a `version`, a valid
   declaration and a regular `CMakeLists.txt` inside the package. Two
   packages may not share a `target` or an `initializer`. Only JSON is read.
2. Matching packages go into `native-app.json` as `nitroModules`:
   `{package, version, root, cmakeLists, target, initializer}`, sorted by
   package name. The key is left out when nothing matches, so descriptors of
   apps without Nitro modules are unchanged.
3. `build-native` recomputes the descriptor, so adding or removing a Nitro
   dependency makes it stale. With modules present it passes
   `-DRNQNX_BUILD_NITRO_QNX=ON`. `--host-preview` is rejected, because the
   preview configuration does not build the Nitro runtime.
4. `cmake/RnqnxConsumerApp.cmake` checks every entry again, requires the
   `rnqnx_nitro_qnx` target, calls `add_subdirectory` on each package's
   directory, fails if the declared target is missing afterwards, links it,
   and writes `NitroModules.generated.h`. That header declares each
   initializer and defines `rnqnx::consumer::registerNitroModules()`, which
   calls them in descriptor order.
5. The consumer `main.cpp` calls `registerNitroModules()` before
   `installQnxNitroRuntime`.
6. `project doctor` runs the same discovery and reports a `nitro-modules`
   check.

## Consequences

- A module author adds one `package.json` key, a CMake project and one
  registration function. Nothing in this repository changes per module.
- Discovery and doctor never execute package code. The package's CMake and
  C++ do run at build time, as for any native dependency.
- The initializer is the author's responsibility. nitrogen does not generate
  a QNX initializer; its Android `registerAllNatives()` shows the shape but
  also calls JNI `registerNatives()`, so it cannot be reused unchanged.
- ABI agreement is not checked. A module must build against the same
  `react-native-nitro-modules` and `nitro-core` that the platform pins
  (`1.0.0-alpha.nightly.136.gdb81de61bd32`); doctor's package checks cover the
  app side only.
- Marathon apps get no Nitro modules: the declaration rejects `"marathon"`
  until the Marathon consumer build links a Nitro runtime. The list form lets
  that value be added later without changing the shape.
- Only direct dependencies are considered. A module pulled in transitively
  must be listed in the app's `dependencies` to be linked.

## Alternatives considered

- **A key inside `nitro.json`.** nitrogen owns that schema and validates it.
  A QNX key there depends on nitrogen tolerating unknown keys across nightly
  versions, and nitrogen recently turned `autolinking` into a hard error.
- **`expo-module.config.json`.** It is the Expo modules format and is read
  through Expo's discovery. A Nitro module is not an Expo module and should
  not need an Expo config file or the `expo` package to be found.
- **`react-native.config.js`.** Reading it means running it, which doctor and
  discovery must not do.
- **A separate `qnx.json` file.** It adds a file with no benefit over a key in
  `package.json`, which every package already has and which already carries
  nitrogen's `swiftpmConfig`.
- **Registration by static initializers in the package's library.** Objects
  in a static archive that nothing references are dropped by the linker, so
  registration would depend on whole-archive flags per toolchain. A named
  function is called explicitly and fails to link if it is missing.
- **Generating registration from `.nitro.ts` specs.** nitrogen at this
  version only emits `apple` and `android` platform specs. The repo's own
  QNX generator covers `PlatformInfo` only and is not a nitrogen backend
  ([PLATFORM_CODEGEN_POC.md](../nitro/PLATFORM_CODEGEN_POC.md)).

## Related files

- `packages/react-native-qnx-cli/src/nitro-modules.mjs`
- `packages/react-native-qnx-cli/src/prepare-native.mjs`
- `packages/react-native-qnx-cli/src/build-native.mjs`
- `packages/react-native-qnx-cli/src/doctor.mjs`
- `cmake/RnqnxConsumerApp.cmake`
- `vnext/probes/playground/main.cpp`
- `tests/js/qnx-nitro-package-metadata.test.mjs`
- `docs/getting-started/native-consumer-app.md`

## Notes

To advance this from host tests: publish or `pnpm link` a real Nitro module
with this key, run `prepare-native --bundle build/qnx/app.hbc` and
`build-native` with SDP 8.0.5, and call one of its hybrid objects from the app
on the Pi. Record the result in `docs/handoff/QNX_VALIDATION_QUEUE.md` under
QNX-NITRO-002.
