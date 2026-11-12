# ADR-0016: Client-drawn window chrome on QNX Screen

## Status

Proposed, 2026-11-12. Host tests exist; no QNX device run has exercised this
chrome yet. The v1 `window-chrome` gate stays `pending`.

## Context

The v1 `window-chrome` gate asks for a native title bar outside the Fabric root
with `macos` and `windows` control layouts, `titleBarStyle`
`default`/`hidden`/`none`, an initial window size, and an ADR that defines
minimize on QNX Screen. PR #122 adds the configuration fields
`extra.rnqnx.window = { controls, titleBarStyle, initialSize }` and writes the
resolved `window` object into `native-app.json` (default controls `macos`).

QNX Screen does not draw window decorations for an application. Screen is a
compositor with an optional window-manager context
(`SCREEN_WINDOW_MANAGER_CONTEXT`); the stock target images run either a
fullscreen launcher or `screenwm`, and neither draws a title bar inside the
app's buffer. Anything the user sees above the React content has to come from
the application.

The Qt 6.12.0 QNX platform plugin
([`qqnxwindow.cpp`](https://github.com/qt/qtbase/blob/v6.12.0/src/plugins/platforms/qnx/qqnxwindow.cpp))
sets these limits:

| Operation                                   | Qt 6.12.0 QNX QPA behaviour                                                                                                        |
| ------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------- |
| `QWindow::close()`                          | Destroys the Screen window; with the last window gone the event loop quits.                                                        |
| `showMinimized()`                           | `QQnxWindow::minimize()` only logs `Qt::WindowMinimized is not supported by this OS version`, then restores the previous geometry. |
| `showMaximized()` / `showNormal()`          | `applyWindowState()` switches between `availableGeometry()` and the saved unmaximized geometry.                                    |
| Top-level geometry without `no-fullscreen`  | `shouldMakeFullScreen()` forces the root window to the screen geometry, because `FullScreenApplication` is on by default.          |
| `startSystemMove()` / `startSystemResize()` | Not overridden; the `QPlatformWindow` base returns `false`.                                                                        |

## Decision

1. **Chrome is client-side Qt Quick.** A QML title bar is created as a sibling
   of a new content item inside the `QQuickWindow`. The Fabric root mounts into
   the content item, which starts below the bar, so the bar is never part of
   the React tree and Yoga never lays it out. The chrome is compiled only with
   `RNQNX_ENABLE_WINDOW_CHROME=ON` (default `OFF`), following the repository
   rule that new native behaviour stays opt-in until a device run proves it.
   With the switch off, the host behaves as before this change.

2. **Controls map to `QWindow` calls, and a control that cannot work is not
   drawn.** There are no disabled or dead buttons. Each hidden control is
   recorded with a reason that the host prints at startup
   (`RNQNX_WINDOW_CHROME hidden=<action> reason=<text>`), so a receipt can quote
   it.

   | Control  | Action                             | QNX (`qnx` QPA)                                                                                                     | macOS / Windows / X11 / Wayland host preview |
   | -------- | ---------------------------------- | ------------------------------------------------------------------------------------------------------------------- | -------------------------------------------- |
   | Close    | `QWindow::close()`; the app quits  | Live                                                                                                                | Live                                         |
   | Minimize | `QWindow::showMinimized()`         | Hidden. The QPA stub does nothing and the image has no restore path (see item 3).                                   | Live                                         |
   | Maximize | `showMaximized()` / `showNormal()` | Live only when the QPA runs with `no-fullscreen`; otherwise hidden, because the window already fills the screen.    | Live                                         |
   | Drag     | `QWindow::startSystemMove()`       | Off. The QPA returns `false`, and moving a window by hand is not done without a window manager that owns placement. | On where the QPA reports support             |

   `offscreen` and `minimal` QPAs, which the host tests use, behave like QNX for
   minimize and drag. An unknown QPA gets close only.

3. **Minimize on QNX Screen** means: the app's Screen window becomes invisible
   (`SCREEN_PROPERTY_VISIBLE` 0), React Native `AppState` moves to `background`,
   and a window-manager-context client such as the launcher or `screenwm` can
   make the window visible again, after which `AppState` returns to `active`.
   An app must never hide itself without a restore path. Qt 6.12.0's QNX
   plugin does not implement this, and this repository does not ship a
   window-manager client, so the minimize control stays hidden on QNX until both
   exist and a device receipt shows minimize, restore and the `AppState`
   transitions. That receipt is the condition for turning it on.

4. **Title bar styles.** `default` draws the bar (title and controls) and moves
   the content down by its height. `hidden` draws no bar background and no
   title; the controls float over the top of the content and the content
   starts at the window's top edge. `none` draws nothing and offers no
   on-screen close, which suits kiosk apps that own their exit path.

5. **Layouts.** `macos` places close, minimize, maximize at the left as round
   glyphs and centres the title in the bar, never overlapping the controls.
   `windows` places minimize, maximize, close at the right and left-aligns the
   title. Hidden controls are removed from the row and the remaining ones close
   up. The window title is the app name (`expo.name`, through the existing
   application identity).

6. **Sizes.** The bar is 44 device-independent pixels tall and every control's
   hit target is at least 44 × 44, so a finger can hit it on a touch panel; the
   drawn glyph is smaller than the hit target. Qt multiplies these by the
   device pixel ratio, so a 2× display gets an 88-pixel bar without extra code.

7. **Keyboard and accessibility.** Each control has `activeFocusOnTab`, an
   accessible role of button, an accessible name (`Close window`,
   `Minimize window`, `Maximize window` or `Restore window`) and a press action.
   Enter and Space activate the focused control. A focus ring is drawn only
   while the control has keyboard focus; pointer presses do not take focus.

8. **Initial size.** `initialSize` is the outer window size, title bar
   included. It is clamped to the screen's available geometry before the window
   is first shown and the window is centred there. `--fullscreen` overrides it.
   On QNX without `no-fullscreen`, the QPA overrides it with the screen size.

## Consequences

- QNX apps get a title and a working close control without a window manager.
- Minimize is unavailable on QNX until a window-manager client exists. The gate
  item "minimize and restore with the AppState transition logged" cannot be
  satisfied on the current image and is recorded as blocked, not passed.
- The React surface is shorter by the bar height in the `default` style. The
  inset is not yet reported to JavaScript or `SafeAreaView`, so `hidden` style
  content can sit under the floating controls. That is the next slice.
- The chrome never calls into JavaScript, but `QtReactHost` runs JavaScript on
  the Qt GUI thread today. A JS call that blocks therefore blocks the title bar
  too. The gate's "chrome still operable while the JS thread is deliberately
  blocked" needs the host to move JavaScript to its own thread first; this
  ADR does not claim it.
- Drag-to-move and edge resize are off on QNX. They can be enabled per target
  once a receipt shows the QPA supports them.

## Alternatives considered

- **Qt Quick Controls `ToolButton` frame (draft PR #110, `QtDesktopWindow`).**
  That frame adds menus, resize handles and full screen, and pulls in the
  Controls/Basic QML closure on the target. This ADR keeps to `QtQuick` only,
  which the renderer already ships. The two can merge later; the capability
  table in item 2 applies to both.
- **Hand-written window move with `setPosition()` on QNX.** PR #110 does this
  when `startSystemMove()` fails. Without a window manager deciding placement,
  the app can drag itself partly off screen with no way back, so this ADR does
  not do it.
- **Disabled minimize button with a tooltip.** Rejected: a control that never
  works is a dead button, and the gate forbids them.
- **Server-side decorations through `screenwm`.** Possible on images that run
  it (Qt's `desktop` QPA option asks for `Frame=Y`), but not available on the
  fullscreen launcher image, and the app could not choose its control layout.

## Related files

- `native/window-chrome/` (model, Qt Quick chrome, host tests)
- `cmake/RnqnxWindowChrome.cmake`
- `cmake/RnqnxConsumerApp.cmake`, `cmake/ConsumerWindowChrome.h.in`
- `vnext/probes/playground/main.cpp`
- `packages/react-native-qnx-cli/src/window-descriptor.mjs`

## Notes

The windows API (Nitro v1 HybridClass), close guards, runtime layout changes,
the title-bar inset in JavaScript, the window icon in the bar (PR #125's
`ApplicationIcon.h`) and edge resize are outside this decision's first slice
and remain open gate items.
