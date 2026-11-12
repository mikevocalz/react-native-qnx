# Window chrome

A consumer app can draw its own title bar with window controls. QNX Screen
draws no decorations, so the bar is Qt Quick inside the app window, above the
React Native surface. The reasoning, and what each control does on QNX, is in
[ADR-0016](../adr/0016-window-chrome-on-qnx-screen.md).

This is experimental and off by default. No QNX device has run it yet.

## Configure

The window comes from the `window` object that `project prepare-native` writes
into `build/<platform>/native-app.json`. With the Expo config plugin from
PR #122 it is set in app config:

```json
{
  "expo": {
    "name": "My App",
    "plugins": [
      [
        "@rn-qnx/expo-qnx-config-plugins",
        {
          "appId": "org.example.myapp",
          "componentName": "main",
          "window": {
            "controls": "windows",
            "titleBarStyle": "default",
            "initialSize": { "width": 1024, "height": 600 }
          }
        }
      ]
    ]
  }
}
```

| Field           | Values                                   | Default   |
| --------------- | ---------------------------------------- | --------- |
| `controls`      | `macos` (left, round), `windows` (right) | `macos`   |
| `titleBarStyle` | `default`, `hidden`, `none`              | `default` |
| `initialSize`   | `{ width, height }`, integers 1 to 16384 | none      |

- `default` draws the bar and starts the React content below it.
- `hidden` draws the controls only, floating over the top of the content.
  The content starts at the top edge, and the controls cover part of it.
- `none` draws nothing, including no close control.

`initialSize` is the whole window, title bar included, clamped to the screen.
The window title is `expo.name`. A `native-app.json` written before the
`window` field existed keeps the old window without chrome.

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

| Control  | QNX                                                               |
| -------- | ----------------------------------------------------------------- |
| Close    | Shown. Closes the window; the app exits.                          |
| Minimize | Not shown. Qt 6.12.0 cannot minimize on QNX and nothing restores. |
| Maximize | Shown only with `-platform qnx:no-fullscreen`.                    |
| Drag     | Off. The QNX platform plugin has no system move.                  |

At startup the host prints one line per hidden control, for example:

```text
RNQNX_WINDOW_CHROME platform=qnx controls=macos style=default inset=44 window=1920x1080
RNQNX_WINDOW_CHROME platform=qnx hidden=minimize reason=Qt 6.12.0 QQnxWindow::minimize() ...
```

Each activation prints `RNQNX_WINDOW_ACTION <action>`.

## Controls

The bar is 44 device-independent pixels tall and every hit target is at least
44 × 44. Tab moves through the controls, Enter or Space activates the focused
one, and a focus ring shows only for keyboard focus. Each control is an
accessible button named `Close window`, `Minimize window`, `Maximize window`
or `Restore window`.

## Known gaps

- The title-bar inset is not reported to JavaScript or `SafeAreaView`.
- With `hidden`, a press on a floating control also reaches the React view
  under it when the input-lifecycle dispatcher is on.
- No runtime layout change, close guard, window icon or edge resize yet.
- The surface keeps its first size when the window is resized, as before.

## Host tests

```bash
cmake -S native/window-chrome -B build/window-chrome -G Ninja \
  -DRNQNX_BUILD_WINDOW_CHROME_TESTS=ON -DRNQNX_WINDOW_CHROME_SANITIZERS=ON
cmake --build build/window-chrome
ctest --test-dir build/window-chrome --output-on-failure
node tests/js/window-chrome-descriptor.test.mjs
```
