# Expo Application on the Qt/QNX host

The opt-in native `ExpoApplication` module implements the metadata constants
used by `expo-application@58.1.2`, the version selected by Expo SDK 58. Enable
`RNQNX_ENABLE_EXPO_APPLICATION=ON` when configuring the Qt React host. It is OFF
by default pending physical QNX qualification. This option links the Expo native
registry and installs it in QtReactHost immediately after Hermes creation; no
additional Expo-bootstrap flag is required.

```ts
import * as Application from 'expo-application';

console.log(Application.applicationName, Application.applicationId);
console.log(Application.nativeApplicationVersion);
// Native build numbering is unavailable: nativeBuildVersion is null.
```

| Public constant            | Native source                                                                       |
| -------------------------- | ----------------------------------------------------------------------------------- |
| `applicationId`            | `QGuiApplication::desktopFileName()`, set by the existing application-identity path |
| `applicationName`          | `QGuiApplication::applicationDisplayName()`                                         |
| `nativeApplicationVersion` | `QCoreApplication::applicationVersion()`                                            |
| `nativeBuildVersion`       | `null`; Qt supplies no separate build number                                        |

Empty native values become `null`, matching Expo's metadata contract. These
values describe process configuration, not verified package ownership,
permissions, signing, installation identity or OS entitlements. The existing
consumer identity path sets the first two values. A host that has not supplied
an application version returns `null`, not an inferred platform/package version.

Use the normal Expo API for unsupported operations; Android IDs, install
referrers, installation timestamps and Apple notification/store environment
methods remain absent. Expo rejects unavailable methods. This adapter does not
invent platform behavior or register Camera, Sensors or Notifications.

The native installer registers the module in the existing Expo registry before
application JavaScript runs. It acquires a small metadata snapshot on the Qt
application owner thread and the existing host object caches it per Hermes
runtime. No worker, Promise, timer, subscription or native resource is introduced.
A new runtime acquires a new snapshot; destroying another runtime cannot
invalidate it. Missing QGuiApplication or off-owner metadata acquisition fails.

## Native contract test

The standalone test compiles the actual Expo registry/installer/module and
matched Hermes runtime, not a mock native provider. It uses an offscreen
QGuiApplication and checks actual process metadata, UTF-8, unknown values,
per-runtime snapshots, runtime destruction and owner-thread rejection.

The additional `expo_application_public_sdk` test transpiles the locked official
`expo-application@58.1.2` public source and actual Expo Modules Core acquisition,
native installer guard and error classes, then executes them in Hermes against
the real native module. Explicit framework shims isolate unrelated UI/bootstrap
side effects; fallback native acquisition throws if unexpectedly reached. It
checks all four public constants and the official unsupported Android-ID error.
This is not a full Metro application or full `expo` startup integration test.

Local Linux/GCC14 + Qt6.11.1 + pinned Hermes: native ON (2/2, including official SDK fixture) and OFF (1/1) contracts passed.
Use `RNQNX_BUILD_EXPO_APPLICATION_TESTS=ON` in a full host build to reuse its
Hermes dependency rather than rebuilding a separate graph.

```sh
cmake -S tests/native/expo-application -B build/expo-application-on -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/path/to/Qt/6.11.1/gcc_64 \
  -DRNQNX_ENABLE_EXPO_APPLICATION=ON
cmake --build build/expo-application-on --target expo_application_runtime_test
ctest --test-dir build/expo-application-on --output-on-failure
# Repeat with OFF to prove the module is absent when the experiment is disabled.
```

The matching RN dependency graph must be installed first. Hermes is fetched at
the repository's exact audited commit; Linux hosts need normal Qt development
and ICU dependencies. No premium component is required. This is host runtime
proof, not a QNX cross-build, physical-device proof or full Expo API 2.0 parity.

Source audit: Expo SDK58 bundled-native-modules pins `expo-application ~58.1.2`.
The inspected npm58.1.2 artifact has SHA1
`6ce5f512bbaaa912e83d02efa821bf557ba79c92`; public `src/Application.ts` reads the
four constants from `ExpoApplication`, and `src/ExpoApplication.ts` uses Expo's
normal native-module acquisition. The existing JSI registry implements that
framework contract; no alternate Nitro facade or generated-binding claim is made.

API/lifetime guidance: Margelo api-design → build-nitro-modules → cpp at
`1e9e17be6f41c838db472d6a4b943fc26355a3be` was read. Decisions: preserve the
established public Expo contract, keep cheap metadata synchronous on its existing
owner, represent unknown values honestly, and cache no cross-runtime JSI objects.
No Nitro ABI, code generation or native-resource API changed.

Rollback: rebuild with the default-OFF option. No stored data, security settings,
permissions or deployed applications are changed by enabling this build option.
