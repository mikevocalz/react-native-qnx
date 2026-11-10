# Experimental Expo Constants on Qt

`RNQNX_ENABLE_EXPO_CONSTANTS=ON` (default OFF) registers the actual
`ExponentConstants` native provider before the app bundle. The public
`expo-constants@58.0.14` implementation reads it through Expo Modules Core.
This is an existing Expo framework contract, not a new application Nitro API.

## Configuration and use

Pass `-DRNQNX_EXPO_PUBLIC_CONFIG=/absolute/path/public-expo-config.json` to
embed a reviewed **public** resolved Expo configuration object. Do not pass
private configuration, credentials, or the `app.json` `{ "expo": ... }` wrapper.
The file is limited to 1 MiB and validated during CMake configuration; the bytes
are embedded as hex so quotes and Unicode cannot inject C++ source. No runtime
file, environment or network lookup occurs. Changing the file triggers CMake
reconfiguration. A single URI `scheme` string is supported by this slice; an
array or invalid scheme fails explicitly. A scheme here does not register the
application with the OS or grant permission to open URLs.

```ts
import Constants from 'expo-constants';

const configuredScheme = Constants.expoConfig?.scheme;
if (!configuredScheme) {
  throw new Error('This application has no embedded linking scheme');
}
const routerOptions = Constants.expoConfig?.extra?.router;
// Cheap metadata reads need no Promise, listener or cleanup.
```

The configuration is optional: without it, `Constants.expoConfig` is `null`,
not a fabricated default. Each Hermes runtime owns its own manifest and native
module object. A process-session UUID is generated from Qt; it is not a stable
device identifier. Qt supplies the actual system font inventory. `__DEV__`
determines the debug snapshot at first access. The environment is `bare`, with
no Expo Go/Updates claim. Device name, status-bar height and WebView user-agent
APIs are intentionally absent until supported by real providers. No Camera,
Sensors, Notifications or permission behavior is implied.

## Evidence and boundaries

Native Linux Clang19 AddressSanitizer tests use real Qt6.11.1 and the pinned
Hermes source selected by RN0.88.1:

- enabled with public config: native behavior and official SDK source tests;
- enabled without config: null configuration and official SDK behavior;
- disabled: no Constants provider;
- per-runtime isolation, reload/disposal and owner-thread rejection;
- six fail-closed CMake configuration cases, plus reserved module-name check.

The SDK fixture transpiles the actual installed `Constants.ts`, types, native
acquisition and Expo core acquisition source. Small framework shims isolate
this metadata test, including an explicitly absent ExpoUpdates provider. It is
not an Expo Router render test. The CI native-runtime matrix also compiles
startup integration. QNX cross-build/physical qualification remains unrun.
Sanitizers instrument the changed provider/test, not the entire Qt/Hermes build.

Source-guidance read: Margelo api-design → build-nitro-modules → cpp at
`1e9e17be6f41c838db472d6a4b943fc26355a3be`, Expo expo-module at
`a9e5bd3fa00b4e61f632e43f85da73aee1942582`, and repository native API/validation
skills. Decisions: preserve the existing audited Expo contract instead of
inventing a Nitro façade; immutable native configuration and runtime-local JSI
values; actual Qt owner checking; no invented hardware values. Independent
services worker reviewed ownership, configuration injection and unavailable
capabilities, with no source blocker.

Rollback: disable the flag and remove the optional public-config path. The
existing QNX bundle and default native startup are unchanged. Core does not
bundle premium Forge components.
