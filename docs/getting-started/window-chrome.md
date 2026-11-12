# Window chrome

A consumer app can draw its own title bar with window controls. QNX Screen
draws no decorations, so the bar is Qt Quick inside the app window, above the
React Native surface. The reasoning, and what each control does on QNX, is in
[ADR-0016](../adr/0016-window-chrome-on-qnx-screen.md).

This is experimental and off by default. One Pi 400 run is recorded below.

## Configure

The title bar is optional and set per app in `extra.rnqnx.window`.
`project prepare-native` writes it into `build/<platform>/native-app.json`,
and the native build compiles it in.

`app.json`:

```json
{
  "expo": {
    "name": "QNX Components",
    "icon": "./assets/icon.png",
    "extra": {
      "rnqnx": {
        "appId": "org.rnqnx.showcase",
        "componentName": "RNQnxPlayground",
        "window": {
          "controls": "macos",
          "titleBarStyle": "default",
          "initialSize": { "width": 1280, "height": 800 }
        }
      }
    }
  }
}
```

`app.config.ts`, through the Expo config plugin (plugin options win over
`extra.rnqnx`):

```ts
import type { ExpoConfig } from 'expo/config';

const config: ExpoConfig = {
  name: 'My App',
  slug: 'my-app',
  icon: './assets/icon.png',
  plugins: [
    [
      '@rn-qnx/expo-qnx-config-plugins',
      {
        appId: 'org.example.myapp',
        componentName: 'main',
        window: {
          controls: 'windows',
          titleBarStyle: 'default',
          initialSize: { width: 1024, height: 600 },
        },
      },
    ],
  ],
};

export default config;
```

| Field                 | Values                                   | Default   |
| --------------------- | ---------------------------------------- | --------- |
| `controls`            | `macos` (left, round), `windows` (right) | `macos`   |
| `titleBarStyle`       | `default`, `hidden`, `none`              | `default` |
| `initialSize`         | `{ width, height }`, integers 1 to 16384 | none      |
| `unavailableControls` | `hide`, `disable`                        | `hide`    |

- `default` draws the bar and starts the React content below it.
- `hidden` draws the controls only, floating over the top of the content.
  The content starts at the top edge, and the controls cover part of it.
- `none` draws nothing: no bar, no controls, the React content fills the
  window. The app owns its exit path.
- `initialSize` is the whole window, title bar included, clamped to the
  screen. With it the app opens as a normal window; without it the app fills
  the screen on QNX.
- `unavailableControls` decides what happens to a control the platform cannot
  run, for example expand on a full-screen QNX app: `hide` leaves it out,
  `disable` draws it grey and inert. Prefer `hide`; a grey control that never
  works is a dead button (ADR-0016 item 2).

The window title is `expo.name` and the bar shows the app's generated icon.
A `native-app.json` written before the `window` field existed keeps the old
window without chrome. Each value is validated by the config plugin,
`prepare-native`, CMake and the C++ reader, so a typo fails at prebuild.

## Build

```bash
pnpm qnx project build-native --project my-app --window-chrome
```

`--window-chrome` adds `-DRNQNX_ENABLE_WINDOW_CHROME=ON`. Without it the
`window` object is validated and otherwise ignored. It fails if the descriptor
has nothing to draw (no `window`, or `titleBarStyle: "none"`).

The playground host takes the same settings on the command line when it is
built with the option on:

```bash
rnqnx_playground_host --bundle app.bundle.js \
  --window-controls windows --title-bar-style default --window-size 1024x600
```

These flags also override a consumer's compiled settings, so one executable
can show both layouts on a device.

## What runs on QNX

Under the stock `fullscreen-winmgr` desktop:

| Control  | QNX                                                                                                      |
| -------- | -------------------------------------------------------------------------------------------------------- |
| Close    | Closes the window; the app exits.                                                                        |
| Minimize | Releases the native window so the launcher shows; the app keeps running. Launching it again restores it. |
| Maximize | Windowed apps only: toggles between the full display and `initialSize`. Double-click on the bar too.     |
| Drag     | Off. The QNX platform plugin has no system move.                                                         |

The [Pi 400 desktop guide](pi400-desktop-app.md) explains each one, how a
minimized app comes back, and how windowed mode works with
`fullscreen-winmgr`.

At startup the host prints the chrome line and one line per control it left
out, for example on a full-screen app:

```text
RNQNX_WINDOW_CHROME platform=qnx controls=macos style=default inset=44 window=1920x1080
RNQNX_WINDOW_CHROME platform=qnx hidden=maximize reason=the qnx QPA keeps the root window at screen size unless it runs with no-fullscreen
```

Each activation prints `RNQNX_WINDOW_ACTION <action>`; a disabled control
prints `RNQNX_WINDOW_ACTION_UNAVAILABLE <action> reason=...`. The playground
flag `--unavailable-controls hide|disable` overrides the app's setting.

The React surface follows the window: a resize, maximize or restore lays the
app out again at the new size and fires `onLayout` on the root view.

## Appearance

The bar is dark (`#13202f`, light title text) unless the platform reports a
light colour scheme, which macOS and Windows do. QNX reports none, so QNX apps
get the dark bar, matching the QNX launcher and dark app themes. The traffic
lights keep their macOS colours, with `×`, `−` and `↗` drawn on them so the
controls are not told apart by colour alone.

## Controls

The bar is 44 device-independent pixels tall and every hit target is at least
44 × 44. Tab moves through the controls, Enter or Space activates the focused
one, and a focus ring shows only for keyboard focus. Each control is an
accessible button named `Close window`, `Minimize window`, `Maximize window`
or `Restore window`.

## Known gaps

- The title-bar inset is not reported to JavaScript or `SafeAreaView`.
- JavaScript runs on the Qt GUI thread, so a long JS call freezes the title
  bar along with the rest of the window.
- With `hidden`, a press on a floating control also reaches the React view
  under it when the input-lifecycle dispatcher is on.
- With the input-lifecycle dispatcher on, touch `pageX`/`pageY` include the
  44 px bar in the `default` style.
- On QNX, maximize needs a windowed app (`initialSize`); a full-screen app has
  nothing to expand to, so the control is left out.
- A minimized QNX app is not in the Alt+Tab list; it comes back by launching
  it again.
- Wayland, including Marathon, gets close only, and the chrome build is
  rejected for Marathon until its shell's behaviour is defined.
- No runtime layout change, close guard or edge resize yet.
- `Dimensions` reports the screen, not the window; use `onLayout` for the
  window's content size.

## Host tests

```bash
cmake -S native/window-chrome -B build/window-chrome -G Ninja \
  -DRNQNX_BUILD_WINDOW_CHROME_TESTS=ON -DRNQNX_WINDOW_CHROME_SANITIZERS=ON
cmake --build build/window-chrome
ctest --test-dir build/window-chrome --output-on-failure
node tests/js/window-chrome-descriptor.test.mjs
```

Host frames rendered by `rendersReferenceFrames` at a 2× device pixel ratio
(offscreen QPA, Qt 6.12.0, macOS host; not QNX output) are in
[`docs/evidence/window-chrome-host-2026-11-12/`](../evidence/window-chrome-host-2026-11-12/).
The `qnx-*` frames use the QNX capability table on the host, so they show the
layout QNX would get, not a QNX render.
