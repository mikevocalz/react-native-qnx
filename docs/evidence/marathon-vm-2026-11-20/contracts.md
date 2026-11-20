# Marathon-Shell client contracts

Source: ~/Marathon-Shell in the mvm VM, commit 1f845160f0c249cb29da127fb66abaad380b5438.
Paths are relative to that repo. The advertised globals below come from
`wayland-info` against the running compositor (see wayland-info.txt).

Globals advertised to an unsandboxed client on socket marathon-wayland-0
(name, version): wl_compositor 5, wl_subcompositor 1, wl_data_device_manager 1,
wl_shm 2, qt_hardware_integration 1, wl_seat 4, wp_security_context_manager_v1 1,
marathon_surface_manager_v1 1, wp_fifo_manager_v1 1, wp_commit_timing_manager_v1 1,
xdg_activation_v1 1, wp_fractional_scale_manager_v1 1, xdg_wm_base 1, wp_viewporter 1,
zwp_text_input_manager_v2 1, zwp_text_input_manager_v3 1, wp_presentation 1,
zwp_idle_inhibit_manager_v1 1, wl_output 2, xdg_wm_dialog_v1 1.
Not advertised: zwlr_layer_shell_v1, zwlr_screencopy_manager_v1 (grim fails with
"compositor doesn't support the screen capture protocol"), zwp_input_method_*,
zwp_virtual_keyboard_manager_v1, wl_drm, zwp_linux_dmabuf_v1 (no /dev/dri in this VM;
log line `[LinuxDmabufV1] stat "/dev/dri/renderD128" failed`).
xdg_wm_base is version 1 only (docs/NATIVE_APP_LIFECYCLE.md:136-141).

## (a) A client asking the shell to open a URL

There is no Wayland protocol for it. The routes in the source:

1. `marathon://<appId>[/...]` over D-Bus, interface `org.marathonos.Shell.Navigation1`.
   - Bus name `org.marathonos.Shell` (crates/marathon-shell-native/src/ipc.rs:17), object
     `/org/marathonos/Shell/Navigation` (ipc.rs:33 path format, tasks/dbus.rs:21 export).
   - `Navigate(uri: s) -> b` at crates/marathon-shell-native/src/tasks/dbus.rs:142-161. It
     requires the `marathon://` prefix, takes the first path segment as the app id and
     launches that app. Everything after the first segment is ignored.
   - `LaunchApp(appId)` at dbus.rs:83, no permission check (comment at dbus.rs:79-80).
   - `LaunchAppWithRoute(appId, route, paramsJson)` at dbus.rs:163, caller must be known
     (`known_caller`, dbus.rs:57). The runner receives MARATHON_APP_ROUTE and
     MARATHON_APP_ROUTE_PARAMS (doc comment dbus.rs:161-162).
   - Deep links declared by app manifests (`deepLinks`): crates/marathon-shell-native/src/apps.rs:289,
     search results of type "deeplink" at crates/marathon-shell-native/src/search.rs:139,
     QML router signal at shell/qml/MarathonShell.qml:1104-1109 (`onDeepLinkRequested`
     calls `Tasks.launchAppWithRoute`).
2. http/https: the shell itself has no handler. A grep for `xdg-open|OpenURI|x-scheme-handler`
   over shell/ finds nothing in the compositor. The system relies on the desktop-file MIME
   default:
   - apps/pacific/shipwright-pacific.desktop:7 `MimeType=x-scheme-handler/http;x-scheme-handler/https;...`;
     packaging/pipeline-patches/0028-feat-browser-open-web-links-in-Pacific.patch:19-21 sets
     the mimeapps defaults; apps/pacific/README.md:108 says `xdg-open` reaches it and that it
     exposes `org.shipwright.pacific.ui openUrl(as)` on the session bus.
   - Pilot (the assistant daemon) exports `OpenUrl(url) -> b` (crates/pilotd/src/dbus.rs:51,
     implementation near :598, http/https only via `web_url`, crates/pilotd/src/onscreen.rs:236).
     `open_url` (onscreen.rs:246-262) calls `org.shipwright.pacific.ui /ui openUrl`, and falls back
     to `xdg-open <url>`.
   - A client in this VM with no Pacific and no mimeapps entry gets nothing from xdg-open
     (no handler is installed by this build).
3. `marathon-store://<verb>/<arg>` is a registered scheme for app-store operations only:
   scripts/store/marathon-store-handler.desktop:8, crates/marathon-store-handler/src/main.rs:2,
   and for sandboxed callers the handler forwards to `org.marathonos.Shell.AppStore1.RunStoreUrl`
   (main.rs:55-65, crates/marathon-shell-native/src/tasks/dbus.rs:314, the shell checks the
   caller holds the "system" permission, per the comment at main.rs:47-54).
4. Focus a window you already own: `xdg_activation_v1` (shell/src/wayland/xdgactivationv1.h:9-14).
   The shell issues tokens for processes it launches; a client's own token is honoured only
   with keyboard focus and a recent input serial.

## (b) How Back reaches a native Wayland app, and "unhandled"

Documented in docs/NATIVE_APP_LIFECYCLE.md:13-83 and implemented in
shell/src/wayland/waylandcompositor.cpp:2498 (`WaylandCompositor::sendNativeBack`),
shell/src/wayland/nativeback.h.

Order of attempts (docs/NATIVE_APP_LIFECYCLE.md:33-58, code at waylandcompositor.cpp:2498-2570):

1. If the toplevel has a `marathon_surface_v1`, the shell sends only the event
   `marathon_surface_v1.back_committed` (shell/protocols/marathon-surface-v1.xml:206-212; code
   waylandcompositor.cpp:2505-2510, marathonsurfacev1.cpp:257-260). The window gets no key.
   Gesture events `back_started`/`back_progress`/`back_cancelled` are in the XML at :183-204.
2. Else the newest mapped `xdg_popup` of that client gets `popup_done`
   (waylandcompositor.cpp:2527-2530), else the newest child `xdg_toplevel` gets `close`
   (waylandcompositor.cpp:2531-2535).
3. Else a key goes to the window: XF86Back (`Qt::Key_Back`) first (nativeback.h:35-39), Escape
   for Keel-hosted apps. If nothing visible changes, one fallback: Escape for GTK4/libadwaita,
   Alt+Left for Qt, GTK3 and unknown (nativeback.h:44-50). The toolkit is detected from
   `/proc/<pid>/maps` (nativeback.h:21-31, waylandcompositor.cpp:2543-2548).

Unhandled detection (no client-to-shell reply exists):

- The shell watches for `MARATHON_NATIVE_BACK_MS` (default 350, nativeback.h:53,
  waylandcompositor.cpp:2604-2611). Handled means: a commit whose damage is at least 1/10 of the
  window (nativeback.h:57-65), a title change (waylandcompositor.cpp:2642-2643), or any surface of
  the same client mapping, unmapping or being destroyed (:2644-2649).
- If none happens, `nativeBackUnhandled(surfaceId)` is emitted (waylandcompositor.h:177), wired
  at shell/main.cpp:183-184 to the lifecycle, which turns it into `systemBackUnhandled`
  (crates/marathon-shell-native/src/tasks.rs:296, :403) and the shell backs out of the app
  (docs/NATIVE_APP_LIFECYCLE.md:73-77).
- So the only way a client "reports unhandled" is by changing nothing visible within the window.
  There is no request in marathon_surface_v1 for it: its requests are destroy, set_window_flags,
  set_gesture_exclusion, set_system_bars only (marathon-surface-v1.xml:118-158). The watch is
  also armed in the marathon_surface path (waylandcompositor.cpp:2507).
- Known limit: a window that repaints most of itself continuously always reads as handled
  (docs/NATIVE_APP_LIFECYCLE.md:79-81).
- The `marathon_surface_manager_v1` global is only bound by clients that are not sandboxed, or
  by sandboxed ones whose security-context engine is `org.marathonos.AppRunner`
  (marathonsurfacev1.cpp:61-69, marathonsurfacev1.h:26). Create the marathon_surface_v1 before the
  toplevel's initial commit (marathon-surface-v1.xml:49-61).

## (c) How a native app exits, and how the shell closes it

- Shell closing a window: `WaylandCompositor::closeWindow(surfaceId)`
  (waylandcompositor.cpp:893; Q_INVOKABLE at waylandcompositor.h:105). It sends the XDG
  `xdg_toplevel.close` (`sendClose()`, waylandcompositor.cpp:928-934). With no xdg toplevel it
  falls back to `client->close()` (:935-941). Keel-hosted apps are closed by SIGTERM to their host,
  with a 5000 ms kill timer (:900-921). Callers: the task card close button
  (shell/qml/components/TaskCard.qml:110) and `Tasks::closeWindowRequested` wired at
  shell/main.cpp:165.
- D-Bus: `org.marathonos.Shell.Navigation1.CloseApp(appId)` (tasks/dbus.rs:121-132), refused for
  app callers, allowed for session services.
- Client exit: when the client destroys its surface or disconnects, `surfaceDestroyed` is emitted
  (waylandcompositor.cpp:1087, :1787) and `appClosed(pid)` (:1838, :1992), connected at
  shell/main.cpp:181-185 to the Tasks lifecycle. No other exit request exists in
  marathon_surface_v1.
- A child xdg_toplevel with a parent is also closed by Back (see b).
- Lifecycle state (Foreground, BackgroundActive, BackgroundIdle, Frozen, 10 s idle freeze) for native
  apps: docs/NATIVE_APP_LIFECYCLE.md:100-127. In this VM there is no delegated cgroup, so
  freezing is only logged (`[cgroup] no delegated marathon.slice`).
- `xdg_toplevel.suspended` is not sent: xdg_wm_base is version 1 (docs/NATIVE_APP_LIFECYCLE.md:132-145).

## (d) System bars, status bar, insets, safe area

- Only one protocol touches this: `marathon_surface_v1` (shell/protocols/marathon-surface-v1.xml).
  - `set_system_bars(mode)` with `shown`/`hidden` (xml:102-106, :151-158). The XML states v1
    records the mode but does not act on it; the status area stays shown. Code only stores the
    flag and emits `systemBarsHiddenChanged` (marathonsurfacev1.cpp:213-224). A grep for
    `systemBarsHidden` in shell/qml and shell/src finds no consumer outside marathonsurfacev1.{h,cpp}.
  - `insets(top, bottom, left, right)` event (xml:160-171): chrome the shell covers. The shell
    always sends zeros: `sendInsets(QMargins(), 0)` at waylandcompositor.cpp:196 and :1501
    (implementation marathonsurfacev1.cpp:226-241).
  - No request sets status bar style (light or dark content), colour or translucency, and no
    request or event reports the status bar height.
- Layout instead of insets: windows are laid out between the status area and the pill. The app
  window item is `x: ..., y: Constants.safeAreaTop`, `height: appWindowContainer.height -
Constants.safeAreaTop - Constants.safeAreaBottom - shell.appKeyboardInset`
  (shell/qml/MarathonShell.qml:2006-2009). Constants: `statusBarHeight = round(28 * scaleFactor)`
  (shell/qml/core/Constants.qml:97), `safeAreaTop = statusBarHeight` (:108),
  `safeAreaBottom = navBarHeight` (:109), left and right 0 (:110-111). These are QML-side only; a
  Wayland client is never told the numbers. Its toplevel is configured to the already-reduced
  viewport (`appConfigureSize`, waylandcompositor.cpp:1185-1191; binding at MarathonShell.qml:3159-3163).
- layer-shell (zwlr_layer_shell_v1): no source in shell/ implements it (grep `layer_shell|LayerShell|layer-shell`
  hits only the sandbox deny list, shell/src/wayland/securitycontextv1.cpp:37) and it is not in
  the global list above. Docs list it among privileged classes hidden from sandboxed clients
  (docs/WAYLAND_PROTOCOLS.md:61-66). Not usable by a client.
- wl_output (v2) is advertised; it gives the output size, not insets. wp_fractional_scale_manager_v1
  and per-client scale: shell/src/wayland/nativeclientscale.h.
- D-Bus: no interface in `org.marathonos.Shell.*` (Settings1, Display1, Ambience1, Wallpaper1, ...,
  list from crates/marathon-shell-native/src) exposes status bar style or height for a native
  client. Not verified beyond a name-level grep of those interface names.

## (e) Input: globals, text input, keyboard inset signalling

Served to clients (docs/WAYLAND_PROTOCOLS.md:8-14; code waylandcompositor.cpp:163-185):

- `wl_seat` v4 (touch, pointer, keyboard capabilities by Qt's seat), `wl_data_device_manager`.
- `zwp_text_input_manager_v2` (default on, env MARATHON_WL_ENABLE_TEXT_INPUT_V2=0 disables,
  waylandcompositor.cpp:164, :174-179).
- `zwp_text_input_manager_v3` always (waylandcompositor.cpp:181-185, comment: carries the on-screen
  keyboard's commits to GTK and other v3 clients). Requires Qt WaylandCompositorPrivate, present in
  this build (`HAVE_WAYLAND_PRIVATE`).
- `zwp_idle_inhibit_manager_v1` (waylandcompositor.cpp:262-266), `wp_viewporter`, `xdg_activation_v1`,
  `wp_fractional_scale_manager_v1`.
- Not served: `zwp_input_method_manager_v2`, `zwp_input_method_v1`, `zwp_input_panel_v1`,
  `zwp_virtual_keyboard_manager_v1`, `zwlr_virtual_pointer_manager_v1`. They are listed as
  privileged and hidden from sandboxed clients (shell/src/wayland/securitycontextv1.cpp:21-47) but
  the compositor creates no such global (absent from wayland-info above). wtype and wlrctl cannot
  inject input into this compositor.
- On-screen keyboard: in-process Marathon keyboard (shell/qml/components/VirtualKeyboard.qml,
  Qt input context `QT_IM_MODULES=marathon`, shell/main.cpp:515). For a Wayland client it appears
  and hides from the client's own text-input enable/disable: `handleTextInputSurfaceEnabled/Disabled`
  emit `nativeTextInputPanelRequested(true/false)` (waylandcompositor.cpp:1370-1390). A client must
  create a text_input object, send `enable` and take keyboard focus. GTK clients that create the
  object after focus are given a focus refresh (waylandcompositor.cpp:1348-1366).
- Resize and inset signalling for a keyboard-avoiding client:
  - `marathon_surface_v1.ime_inset(bottom)` exists (marathon-surface-v1.xml:173-181), but the
    shell sends 0 only (waylandcompositor.cpp:196, :1501). The XML says "Marathon resizes windows
    clear of its keyboard, so this is 0".
  - The keyboard height enters the layout as `shell.appKeyboardInset = virtualKeyboard.reservedHeight`
    (MarathonShell.qml:3028-3029), which shrinks the QML app window (:2009) and is added back into
    the compositor viewport `Qt.size(appWindow.width, appWindow.height + shell.appKeyboardInset)`
    (:3159-3163, property `appViewportSize`, waylandcompositor.cpp:1157-1164, used for toplevel
    configure sizes at :1185-1191). Reading the arithmetic, the configure size seems not to shrink
    when the keyboard opens, while the item the surface is drawn in does. This is a source reading,
    not a runtime observation: it needs a client plus a text-input focus to confirm whether
    xdg_toplevel.configure carries a smaller height when the keyboard is up.
  - A special case exists for input-method-service apps (Deckard Android apps): the keyboard cover
    is reported over D-Bus `org.marathonos.Shell.TextInput1` (`SetPanel`, MarathonShell.qml:3031-3037,
    crates/marathon-shell-native/src/text_input.rs:312) and their windows keep their size
    (MarathonShell.qml:3026-3029). Not a general client API.
- No document under docs/ mentions KeyboardAvoidingView or ime_inset (grep
  `KeyboardAvoiding|ime_inset|imeInset` over docs/ returned nothing). A grep for
  `statusBarStyle|status_bar_style|statusBarColor|StatusBar` over shell/src/wayland and
  crates/marathon-shell-native/src found only a clock-position setting (settings.rs:192).

## Build and run facts

- Compositor binary: ~/ms-build/shell/marathon-shell-bin (CMake target `marathon-shell`, OUTPUT_NAME
  marathon-shell-bin, shell/CMakeLists.txt:334-336). `--skip-lock` forces home (MarathonShell.qml:575-584).
- Screenshot mechanism: SIGUSR1 to the shell saves a PNG to $MARATHON_SCREENSHOT_PATH
  (shell/main.cpp:1238-1266). wlr-screencopy is not offered, so grim cannot be used.
- Socket default `marathon-wayland-0`, override MARATHON_WL_SOCKET_NAME (docs/WAYLAND_PROTOCOLS.md:34-37,
  waylandcompositor.cpp:100).
- The shell re-derives DBUS_SESSION_BUS_ADDRESS as unix:path=$XDG_RUNTIME_DIR/bus (shell/main.cpp:627),
  so a session bus must listen at /tmp/xdg/bus.
