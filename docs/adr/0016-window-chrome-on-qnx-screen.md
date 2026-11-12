# ADR-0016: Client-drawn window chrome on QNX Screen

## Status

Proposed, 2026-11-12. Host tests exist. The chrome ran on the Pi 400 on
2026-11-12 (close only); the revised minimize, maximize and windowed start
(items 3, 8, 9) await their device run. The v1 `window-chrome` gate stays
`pending`.

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

   | Control  | Action                             | QNX (`qnx` QPA)                                                                                                     | macOS / Windows / X11 host preview |
   | -------- | ---------------------------------- | ------------------------------------------------------------------------------------------------------------------- | ---------------------------------- |
   | Close    | `QWindow::close()`; the app quits  | Live                                                                                                                | Live                               |
   | Minimize | `QWindow::showMinimized()`         | Live: the host releases the native window and restores it on relaunch (item 3, revised 2026-11-12).                 | Live                               |
   | Maximize | `showMaximized()` / `showNormal()` | Live with `no-fullscreen` (a windowed app); hidden for a full-screen app (item 9, revised 2026-11-12).              | Live                               |
   | Drag     | `QWindow::startSystemMove()`       | Off. The QPA returns `false`, and moving a window by hand is not done without a window manager that owns placement. | On where the QPA reports support   |

   `offscreen` and `minimal` QPAs, which the host tests use, behave like QNX for
   minimize and drag. Wayland gets close only until a compositor, Marathon's
   shell in particular, is verified; `RNQNX_ENABLE_WINDOW_CHROME` is rejected
   together with `RNQNX_BUILD_MARATHON_HOST`, and `build-native` rejects
   `--window-chrome` for Marathon. An unknown QPA gets close only.

3. **Minimize on QNX Screen** (revised 2026-11-12). The stock
   `fullscreen-winmgr` shows one window per display, the one used last, and
   shows the launcher again only when that window goes away. Setting
   `SCREEN_PROPERTY_VISIBLE` to 0 on the app's own window leaves the screen
   blank, because the window manager does not notice and keeps the launcher
   hidden. Minimize therefore hides and destroys the app's native window
   (`QWindow::destroy()`); the process, the `QQuickWindow`, its items and the
   React tree stay. The restore path is a relaunch: the host listens on a
   per-user local socket named after the app id, and a second launch (launcher
   tile or run script) asks the running copy to show its window and exits.
   A minimized app is not in the Alt+Tab list, since the window manager tracks
   windows only. `AppState` does not change yet. Evidence for the window
   manager's behaviour is in the notes below.

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
   The control set is fixed for the window's life and a resize only moves the
   controls, so keyboard focus stays on the focused control. Glyphs are always
   drawn so controls are not told apart by colour alone; only on `cocoa`, where
   hover exists and is the convention, do they appear on hover or focus.

8. **Initial size.** `initialSize` is the outer window size, title bar
   included. It is clamped to the screen's available geometry before the window
   is first shown and the window is centred there. `--fullscreen` overrides it.
   On QNX an app with `initialSize` opens windowed: before Qt loads the
   platform plugin, the host adds `no-fullscreen` to `QT_QPA_PLATFORM` (an
   explicit `-platform` argument is left alone). `fullscreen-winmgr` still
   resizes every new window to the display on its first post and ignores later
   size changes, so for two seconds after showing the window the host compares
   Screen's `SCREEN_PROPERTY_SIZE` with Qt's geometry and writes Qt's size,
   source size and position back (`RNQNX_WINDOW_GEOMETRY`). Without
   `initialSize` the app fills the screen as before.

9. **Maximize follows the surface** (revised 2026-11-12).
   `QtReactHost::resizeSurface` lays the running surface out again when the
   content item changes size, so the host passes `SurfaceResize::FollowsWindow`
   and maximize is live wherever the platform can run it. On QNX that means a
   windowed app (`no-fullscreen`); a full-screen app has nothing to expand to
   and the control is hidden. A double click on the bar toggles maximize.

10. **Appearance** (added 2026-11-12). The bar is dark unless the platform
    reports a light colour scheme (`Qt.styleHints.colorScheme`), which macOS
    and Windows do. QNX reports none, so QNX apps get the dark bar, matching
    the QNX launcher and the dark gallery theme. A per-app appearance option
    is not part of this decision.

## Consequences

- QNX apps get a title and a working close control without a window manager.
- Minimize works on QNX without a window-manager client of our own, at the
  cost of releasing the native window. The gate item "minimize and restore
  with the AppState transition logged" still needs the `AppState` part and a
  device receipt.
- The React surface is shorter by the bar height in the `default` style. The
  inset is not yet reported to JavaScript or `SafeAreaView`, so `hidden` style
  content can sit under the floating controls. That is the next slice.
- The chrome never calls into JavaScript, but `QtReactHost` runs JavaScript on
  the Qt GUI thread today. A JS call that blocks therefore blocks the title bar
  too. The gate's "chrome still operable while the JS thread is deliberately
  blocked" needs the host to move JavaScript to its own thread first; this
  ADR does not claim it.
- Touch coordinates: with the input-lifecycle dispatcher on, `pagePoint` and
  `screenPoint` are window coordinates (`QtTouchDispatcher.cpp`), so in the
  `default` style they include the 44 px bar. `offsetPoint` is item-local and
  correct. Fixing it means mapping through the Fabric root in the dispatcher,
  which is outside this slice.
- In `hidden` style the dispatcher, a window-level event filter that never
  consumes events, also delivers a press on a floating control to the Fabric
  view underneath. The chrome cannot consume it from QML; the dispatcher needs
  an exclusion for control rectangles. Not done here.
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
  works is a dead button, and the gate forbids them. `unavailableControls:
"disable"` exists for apps that want the macOS three-light look on a
  platform that cannot run a control; the default stays `hide`.
- **Minimize by `SCREEN_PROPERTY_VISIBLE` = 0, or by lowering the z-order.**
  Rejected for `fullscreen-winmgr`: it ignores both, so the screen goes blank
  and the launcher does not come back.
- **The QPA's `desktop` option (`Frame=Y`).** Rejected: this window manager
  rejects the `Pos=` and `Visible=` messages that option relies on.
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

fullscreen-winmgr, from disassembly of `/system/bin/fullscreen-winmgr` on the
Pi 400 image (QNX 8.0 screen demos 5.0.0.00343), 2026-11-12:

- On a window's first `SCREEN_EVENT_POST` it sets `SCREEN_PROPERTY_ZORDER` 0
  and `SCREEN_PROPERTY_SIZE` to the display size, with no exceptions. It never
  sets `POSITION`, and it handles `SCREEN_EVENT_PROPERTY` only for the manager
  string, so later size, position and visibility changes by the app stand.
- Per display, the most recently used window gets `VISIBLE` 1 and every other
  window 0. This runs on create, close and switch (Alt+Tab or `switch=<tag>`).
- Manager messages are `key=value` pairs: `can_close` (Y/N, whether Delete in
  the Alt+Tab bar may close it), `Frame` (Y/N; Y sends a close event, N sends
  SIGTERM), `Title` (ignored), `Launcher` (first one wins), `switch=<tag>`
  and `Tag=<tag>`. There is no message that brings the launcher forward.
- `demolauncher` sends `can_close=N=Launcher=Y`. For a `Singleton=Y` tile it
  sends `switch=<tag>` when the app runs, otherwise it starts it and sends
  `Tag=<tag>`.
- Options: `-d` (pause background apps), `-l <n>` (window limit, default 20),
  `-m` (no `closed=` messages to the launcher). `WINMGR_KEYMOD` and
  `WINMGR_HOTKEY` change Alt+Tab.

The same evidence is summarised for users in
[the Pi 400 desktop guide](../getting-started/pi400-desktop-app.md).

The window options are validated in four places: the config plugin (PR #122),
`window-descriptor.mjs`, `cmake/RnqnxConsumerApp.cmake` and
`parseWindowChromeConfig`. The native lane and the JS test share the fixtures
in `native/window-chrome/fixtures/`; a parity test against `resolveQnxWindow`
follows once PR #122 lands.

The windows API (Nitro v1 HybridClass), close guards, runtime layout changes,
the title-bar inset in JavaScript, the window icon in the bar (PR #125's
`ApplicationIcon.h`) and edge resize are outside this decision's first slice
and remain open gate items.
