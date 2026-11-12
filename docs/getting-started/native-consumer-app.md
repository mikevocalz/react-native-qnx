# Give an external application its own native identity

This experimental source-build path reuses the existing Fabric/Hermes/Qt Quick renderer. It adds a consumer executable with its own app ID, display name and React registration. The original playground defaults are unchanged. QNX remains the primary/default target; Marathon is a separately compiled Linux target.

Use the repository's pinned Node/pnpm dependencies and [native host prerequisites](../toolchain.md). The application source may live outside this checkout, but the native build still uses this platform source tree and its qualified dependencies. This is not a published standalone installer, an Expo Go build or a managed Marathon package.

## Configure the application

Put the identity in the consumer's static `app.json`:

```json
{
  "expo": {
    "name": "Field Notes",
    "extra": {
      "rnqnx": {
        "appId": "org.example.fieldnotes",
        "componentName": "FieldNotes"
      }
    }
  }
}
```

Register the same component name using the [opt-in public entry](public-react-native-entry.md):

```js
import { AppRegistry } from 'react-native';
import App from './App';

AppRegistry.registerComponent('FieldNotes', () => App);
```

The app ID is a reverse-DNS identifier, not a permission grant or proof of a signed OS identity. The display name may contain Unicode. The component name is an ASCII identifier and must match the application's registration.

## Prepare the native descriptor

From the platform checkout, use the [external bundle workflow](external-project-bundles.md), then prepare its native descriptor:

```bash
pnpm qnx project bundle --project /absolute/path/to/field-notes
pnpm qnx project prepare-native --project /absolute/path/to/field-notes
```

Preparation resolves the app's Expo config through `@expo/config`, the same loader `expo start` and `expo prebuild` use, so `app.json`, `app.config.js` and `app.config.ts` all work and config plugins listed in `plugins` run. It then hashes the selected bundle and exclusively creates `build/qnx/native-app.json` inside the consumer project. Preparation executes code on the host: a dynamic `app.config.*` file and every config plugin listed in `plugins`, with your user's permissions, exactly as `expo start` and `expo prebuild` do. Only prepare projects you trust. The checks preparation adds (each config file is a regular file of at most 64 KiB, never a symlink) cover the config files themselves, not the modules they import or the plugins they load. If a plugin in `plugins` is not installed yet, for example right after `project add-qnx`, preparation stops with an error naming the plugin; install the app's dependencies and run it again. Preparation does not install dependencies, compile or contact a device. The project needs a `package.json` and an installed `expo` package; the platform checkout's `@expo/config` is the fallback when the app has none. Use `--dry-run` to inspect without writing. Identical reruns leave the file unchanged. After changing the app identity or bundle, review and remove only the generated `native-app.json` before preparing it again; changed generated output is not silently overwritten.

Preparation also needs an app icon and a tile label that fits the launcher, and it generates the icon set with the Qt host tool; see [App icon and tile name](app-identity.md). The plugin's `icon` and `shortName` options feed that check.

For Marathon, supply `--platform marathon` to both commands. Each target gets its own descriptor and bundle.

### Set native configuration through the config plugin

`@rn-qnx/expo-qnx-config-plugins` is the one place to set native app configuration. Add it to `plugins` instead of editing generated files:

```json
{
  "expo": {
    "name": "Field Notes",
    "plugins": [
      [
        "@rn-qnx/expo-qnx-config-plugins",
        {
          "appId": "org.example.fieldnotes",
          "componentName": "FieldNotes",
          "shortName": "Notes",
          "window": {
            "controls": "macos",
            "titleBarStyle": "default",
            "initialSize": { "width": 1024, "height": 600 }
          }
        }
      ]
    ]
  }
}
```

The plugin writes these values to `expo.extra.rnqnx`. Plugin options override values already in `extra.rnqnx`. `window.controls` is `macos` (default) or `windows`; `window.titleBarStyle` is `default` (default), `hidden` or `none`; `window.initialSize` takes integer `width` and `height` from 1 to 16384 and opens the app as a window of that size; `window.unavailableControls` is `hide` (default) or `disable`. See [window chrome](window-chrome.md) for what each option draws and [the Pi 400 desktop guide](pi400-desktop-app.md) for how the controls behave on QNX. Unknown window keys and values fail preparation, and so does `null` for `window` or any of its fields: leave a field out to get its default. The CMake and C++ readers of `native-app.json` reject `null` the same way. `native-app.json` always carries the resolved `window` object. The title bar that consumes it is tracked under the window-chrome gate; `shortName` and `icon` validation belongs to the app-identity gate.

Running preparation twice with unchanged config leaves `build/` byte-identical; `tests/js/expo-config-resolution.test.mjs` hashes the tree after each run to check it. Descriptors generated before the `window` field existed differ from new output, so remove that `native-app.json` once and prepare again.

## Build a Linux preview of the QNX application

The following is explicitly a **Linux preview**, not QNX device execution. From the platform root with native prerequisites installed:

```bash
cmake --preset linux-host-renderer \
  -DRNQNX_BUILD_CONSUMER_APP=ON \
  -DRNQNX_CONSUMER_HOST_PREVIEW=ON \
  -DRNQNX_CONSUMER_CONFIG=/absolute/path/to/field-notes/build/qnx/native-app.json

cmake --build build/linux-host-renderer --target rnqnx_consumer_app -j 2

build/linux-host-renderer/consumer/qnx/org.example.fieldnotes/org.example.fieldnotes --describe-app
build/linux-host-renderer/consumer/qnx/org.example.fieldnotes/org.example.fieldnotes
```

The build compiles the identity into the executable and stages its verified bundle beside it as `app.bundle.js`. The executable discovers that adjacent bundle, so the pair can move together without an absolute source-bundle path. Runtime libraries, plugins and external assets are separate requirements; moving these two files does not create a self-contained distribution. `--describe-app` inspects metadata without loading JavaScript and does not verify runtime behavior.

Each CMake build directory selects one consumer. Use separate build directories for simultaneous applications or platforms. With a Marathon descriptor, the consumer uses the existing Linux/Wayland identity path. Running it inside a real Marathon session can use the [development launcher](marathon-development-launcher.md); managed shell permissions, lifecycle and installation remain separate work.

For a real QNX build, use the existing documented QNX SDK/Qt toolchain, not the Linux preset. The selected lean QNX Hermes requires an `.hbc` bundle generated by the matching host compiler. Pass that project-local file with `prepare-native --bundle build/qnx/app.hbc`; the native configuration rejects a JavaScript bundle for a real QNX target. This change does not qualify the cross-build, device, SDK distribution or Qt plugin closure.

## Detect configuration errors and preserve old behavior

The native host can also receive `--app-id`, `--app-name` and `--component-name` together. Partial or repeated identity arguments fail. A configured consumer refuses arguments that change its compiled identity. A missing React registration fails the render rather than accepting a blank screenshot. Malformed identity, unsupported target and changed bundle hashes fail before a successful consumer build is claimed.

Generated descriptors and native build inputs are trusted local project files, not a sandbox for hostile CMake, plugins or native code. Do not change identity to bypass shell authorization. The optional consumer target is OFF by default; disabling it restores the original target graph. Existing playground launches retain their old registration and window defaults when custom identity is absent.

## Verification scope

The JavaScript tests cover file ownership, hash checks, no-overwrite behavior and actual CMake configuration. Separate native Qt tests cover identity validation and metadata under plain and sanitizer configurations. The full-host workflow builds and relocates configured executables for both platform bundles, tests wrong registrations and preserves screenshots/logs. Each result belongs to its exact source revision; Linux offscreen success is not physical QNX or Marathon shell qualification.

General native package registration, Router, full lifecycle integration, debugger/Fast Refresh, dynamic Expo configuration and deployment/distribution automation are not completed by this identity slice. Core builds still require no Nitro Forge package.
