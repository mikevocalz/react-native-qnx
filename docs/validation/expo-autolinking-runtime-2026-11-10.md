# Explicit Expo provider startup wiring

Default OFF. `RNQNX_ENABLE_EXPO_MODULES=ON` plus an absolute
`RNQNX_EXPO_MODULES_CMAKE` connects reviewed generated module targets to the
existing QtReactHost startup installer, independently of ExpoApplication.
No consumer executable needs a manual registration call. Existing preparation
without provider metadata retains its original generated output.

This is only for **explicitly ported QNX/Marathon providers**, not automatic
compatibility with Android/iOS Expo modules. Provider metadata adds:

```json
{
  "platforms": ["qnx"],
  "qnx": {
    "target": "example_provider",
    "modules": ["Example"],
    "registrationFunction": "registerExampleProvider"
  }
}
```

The target implements `extern "C" void registerExampleProvider(rnqnx::expo::ExpoQnxModuleRegistry&)`
using the existing `ExpoQnxModuleRegistry.hpp`. Generated C++ directly references
each entrypoint, preventing static archive dead stripping without global static
initializers. Missing symbols fail linking. Duplicate entrypoints/module names,
reserved builtin names, unsafe symbols, and missing target definitions fail
preparation/configuration. Runtime mode refuses old generation or a mixed list
containing named modules without registration entrypoints. Providers must list
all their module names; build inputs are trusted source, not a security sandbox. Name checks apply to
declared metadata, not arbitrary registration code; existing registry semantics
allow a native provider to replace a factory. This is not runtime collision
isolation.

## Internal framework boundary

This entrypoint is build-time wiring for the existing Expo framework registry,
not a new general-purpose application native API. It uses the existing registry
and ModuleFactory types. C linkage makes the symbol predictable; its C++ registry
reference is not a portable C ABI. New application-native APIs still follow Nitro v1.
No Android/iOS bridge fallback, fake provider, or unsupported API success is added.

Registration runs before bundle evaluation on each host startup. A provider must
register process-valid factories only, must be idempotent, and must not retain a JSI Runtime or
JS objects globally. Factories produce runtime-local objects; the existing host
object caches those only for its own runtime. Exceptions propagate and prevent
successful host creation. Native registration functions must not throw across
foreign-language callers; the generated caller here is same-toolchain C++.

## Host evidence and reproduction

Generate the test provider configuration:

```sh
node tests/native/expo-autolinking/generate.mjs "$PWD/build/expo-test-modules.cmake" marathon
```

Configure the normal full Qt/Hermes host with
`RNQNX_ENABLE_EXPO_MODULES=ON`,
`RNQNX_EXPO_MODULES_CMAKE=<absolute generated file>`,
`RNQNX_ENABLE_EXPO_APPLICATION=OFF`, and
`RNQNX_BUILD_EXPO_AUTOLINK_TESTS=ON`. Build `expo_autolink_host_test` and run
`ctest -R '^expo_autolink_host$' --output-on-failure`. Repeat with modules OFF.

Local validation used the production generated CMake to compile actual static
provider/registry libraries, then strict Clang19 to compile this QtReactHost and
test source and link them ahead of the existing pinned full host dependency
archives (Qt6.11.1, RN0.88.1, actual Hermes). This reuse is not a clean full-tree
build. The test provider is explicitly test-only, not a supported Expo SDK
module: its native function reads real QCoreApplication metadata. ON checks
invocation, per-runtime mutable state, two simultaneous hosts, disposal and
recreation, with ExpoApplication disabled. OFF checks no `expo` global.

No QNX execution, physical device, consumer release, or general Expo services
compatibility is claimed. Existing readiness gates remain pending.
