# QNX Router example

A small Expo Router application using the existing QNX tab adapter and normal
public imports. The Draft screen edits a note and increments a counter. Review
shows the same state and can reset it. A per-application Zustand store retains
state across screen remounts and is released when the application root closes.
The tab bar follows the measured window width: bottom on narrow windows and a
left rail on wider windows.

## Build the JavaScript bundle

From the repository root, with Node 26+ and the pinned pnpm installed:

```sh
pnpm install --frozen-lockfile
node packages/react-native-qnx-cli/src/bin.mjs project inspect --project apps/expo-router-example
node packages/react-native-qnx-cli/src/bin.mjs project bundle --project apps/expo-router-example
```

The output is `apps/expo-router-example/build/qnx/app.bundle.js`. Bundling does
not launch an app. The app registers `RNQnxRouterExample` and configures its own
identity, `org.rnqnx.routerexample`, in `app.json`. Use that configuration with
[the native app workflow](../../docs/getting-started/expo-native-build.md) and a
matching runner/build. The default native window contract is retained; changing
the tab bar does not resize or rotate the app window.

## Host checks

```sh
node tests/js/qnx-router-example.test.mjs
```

This check mounts the actual screen components with native leaf and Router host
stand-ins. It exercises edit/reset handlers, state across route remounts,
separate-root isolation, root teardown and narrow/wide tab options. It does not
prove navigation event delivery, native rendering, keyboard input or hardware
behavior. A production Metro bundle checks the real Router and QNX resolver
module graph separately.

## Scope

This is a source-checkout example. The workspace packages have not been
published as part of this change. It uses the supported Tabs and Link APIs;
native Stack, native tabs, OS deep-link activation, modal navigation and
persistent storage are outside this example. The scheme is application
configuration, not proof of OS deep-link delivery. Physical QNX acceptance is
still required before this app can close the Expo release gate.
