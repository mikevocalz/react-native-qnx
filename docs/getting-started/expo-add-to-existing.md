# Add QNX to an existing Expo app

`rnqnx project add-qnx` adds the QNX target to an Expo app that already builds
for iOS, Android or web. It adds files and entries and never replaces one.

```bash
node /path/to/react-native-qnx/packages/react-native-qnx-cli/src/bin.mjs \
  project add-qnx --project /path/to/your-app --app-id org.example.notes
```

What it changes:

| File                    | Change                                                                                                                                                |
| ----------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------- |
| `metro.qnx.config.js`   | New. `project bundle --platform qnx` uses it; `metro.config.js` stays Expo's config for iOS, Android and web.                                         |
| `package.json`          | Adds `scripts.qnx` and `file:` links to this checkout's `@rn-qnx/react-native-qnx`, `@rn-qnx/metro-config-qnx` and `@rn-qnx/expo-qnx-config-plugins`. |
| `app.json`              | Appends `["@rn-qnx/expo-qnx-config-plugins", {"appId", "componentName"}]` to `plugins`.                                                               |
| `app.config.js` / `.ts` | Not edited. The command prints the plugin entry to add, because a dynamic config is your code.                                                        |

It leaves `package.json` `main`, existing scripts, `ios/`, `android/`,
`metro.config.js` and the `expo.platforms`, `expo.ios`, `expo.android` and
`expo.web` settings alone. `componentName` defaults to `main`, the name
`expo/registerRootComponent` and `expo-router/entry` register. A rerun with
the same arguments changes nothing; a conflicting value already in
`package.json` or the plugin entry stops the command instead of being
overwritten. Use `--dry-run` to see the plan first.

The rewritten `package.json` and `app.json` keep their existing indentation
(tabs or spaces). add-qnx reads the Expo config without running config
plugins, so it works and reruns cleanly before the first install. For
`app.config.js`/`.ts`, an existing plugin entry is only matched by name until
dependencies are installed; its options are checked after install.

Then install dependencies with your package manager.

Limitation: the `file:` links and the `qnx` script use absolute paths to this
platform checkout. Moving or deleting the checkout, or sharing the app with a
teammate whose checkout lives elsewhere, breaks the install and the script
until you rerun add-qnx (after removing the old entries) or edit the paths.
They become normal package versions once the `@rn-qnx` packages are published.

## Develop and run

Expo CLI keeps owning Metro, logs and reload: `npx expo start`.

`node --run qnx` (or `pnpm qnx`) runs `rnqnx project run`, which walks
config, bundle, native descriptor, native build, deploy and launch. It runs
each step the host can do and stops with exit code 3 at the first one that
needs something missing, naming the command or prerequisite:

```text
config             done          Notes from app.json via @expo/config 58.0.10
bundle             done          build/qnx/app.bundle.js
native-descriptor  done          build/qnx/native-app.json for org.example.notes
native-build       needs SDP     QNX_HOST and QNX_TARGET are not set. ...
deploy             not run       waits for native-build
launch             not run       waits for native-build
```

Steps marked `not run` did not happen. When two bundles exist, `project run`
uses whichever of `app.bundle.hbc` and `app.bundle.js` is newer. After a
rebundle it refreshes the `native-app.json` it generated if only the bundle
path or hash changed; any other difference, such as a changed app ID, still
stops with "Generated native configuration differs" so you can review it. Deploying a consumer app to the board
from `project run` is not implemented yet, so a run with `--device` still
stops before copying anything.

`tests/js/qnx-add-to-existing-expo.test.mjs` runs this against an Expo app
fixture outside the checkout with `ios/`, `android/`, web settings and an
`expo-router/entry` main, and checks those stay byte-identical.

To create a new Expo app for QNX instead, see
[expo-project-create.md](expo-project-create.md).
