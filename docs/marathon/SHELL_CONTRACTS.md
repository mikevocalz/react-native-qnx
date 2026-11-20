# Marathon-Shell contracts for a native client

What a Wayland client can rely on from Marathon-Shell for URL open, Back, exit, system bars and keyboard. Read at `patrickjquinn/Marathon-Shell` commit `1f845160f0c249cb29da127fb66abaad380b5438` and checked against the shell running in a VM on 2026-11-20. Every `file:line` below is a path inside that repository. The full reading, with more citations, is [contracts.md](../evidence/marathon-vm-2026-11-20/contracts.md).

| JavaScript API                                   | Shell contract                            | State in `@rn-qnx/react-native-marathon`                                |
| ------------------------------------------------ | ----------------------------------------- | ----------------------------------------------------------------------- |
| `BackHandler` back press                         | Yes: XF86Back key                         | Implemented and tested. The host turns the key into `hardwareBackPress` |
| `BackHandler.exitApp`                            | Yes: the client leaves, the shell notices | Implemented and tested. The host exposes `__rnqnxMarathonExitApp`       |
| `Linking.openURL`, `canOpenURL`, `getInitialURL` | Partial: D-Bus for `marathon://` only     | Tested stub. No bridge, see below                                       |
| `StatusBar`                                      | No                                        | Tested stub                                                             |
| `KeyboardAvoidingView`                           | No                                        | Tested stub                                                             |

## Back

`WaylandCompositor::sendNativeBack` (`shell/src/wayland/waylandcompositor.cpp:2498`, documented in `docs/NATIVE_APP_LIFECYCLE.md:13-83`):

1. A toplevel with a `marathon_surface_v1` gets the event `back_committed` and no key (`waylandcompositor.cpp:2505-2510`).
2. Otherwise the newest mapped `xdg_popup` gets `popup_done`, or the newest child `xdg_toplevel` gets `close` (`:2527-2535`).
3. Otherwise the window gets XF86Back (`Qt::Key_Back`, `shell/src/wayland/nativeback.h:35-39`). If nothing visible changes within 180 ms it gets one fallback key: Escape for GTK 4, Alt+Left for Qt, GTK 3 and unknown toolkits (`nativeback.h:44-50`).

A client cannot report "unhandled". The shell watches the window for `MARATHON_NATIVE_BACK_MS` (default 350). The back counts as handled if a commit damages at least a tenth of the window, the title changes, or a surface of the client maps or unmaps (`nativeback.h:57-65`, `waylandcompositor.cpp:2604-2649`). Otherwise the shell backs out of the app (`shell/main.cpp:183-184`). So a JavaScript handler that returns true must also change what is on screen, or the shell leaves the app anyway.

The host consumes `Qt::Key_Back` and leaves Escape and Alt+Left alone (`native/platform/linux-host/MarathonBackKey.hpp`).

## Exit

The shell closes a window with `xdg_toplevel.close`, and with `client->close()` when there is no xdg toplevel (`waylandcompositor.cpp:893-941`). When a client destroys its surface or disconnects, the shell emits `appClosed` (`:1087`, `:1787`, `:1838`, `:1992`; connected at `shell/main.cpp:181-185`). `marathon_surface_v1` has no exit request (`shell/protocols/marathon-surface-v1.xml:118-158`). `BackHandler.exitApp()` therefore ends the Qt application; in the VM run the process exited and the shell returned to home.

## Open URL: no bridge written

- `marathon://<appId>` has a D-Bus route: `org.marathonos.Shell.Navigation1.Navigate(uri) -> b`, which takes the first path segment as the app id and ignores the rest (`crates/marathon-shell-native/src/tasks/dbus.rs:142-161`). `LaunchApp` is at `dbus.rs:83` and `LaunchAppWithRoute` at `dbus.rs:163`.
- http and https have no handler in the shell. A search of `shell/` for `xdg-open`, `OpenURI` and `x-scheme-handler` finds nothing. The path is a `.desktop` MIME default: Pacific declares the schemes (`apps/pacific/shipwright-pacific.desktop:7`) and a pipeline patch sets the defaults (`packaging/pipeline-patches/0028-feat-browser-open-web-links-in-Pacific.patch:19-21`). The VM had no handler, so there is nothing to observe.
- There is no document or protocol for a launch URL delivered to a client, so `getInitialURL` and `url` events have no source. Deep links in app manifests (`crates/marathon-shell-native/src/apps.rs:289`) apply to Marathon apps, not to native clients.

A bridge for `marathon://` would call a D-Bus method on a bus the host does not otherwise use, for one scheme, with no way to learn the result of `canOpenURL`. That is a design decision for the owner and upstream, so `Linking` keeps its stub: `openURL` rejects, `canOpenURL` resolves false.

## System bars: no contract

- `marathon_surface_v1.set_system_bars(shown|hidden)` is recorded and does nothing: the XML says so (`marathon-surface-v1.xml:102-106`, `:151-158`), and `shell/src/wayland/marathonsurfacev1.cpp:213-224` only stores the flag. Nothing in `shell/qml` or `shell/src` reads it.
- The `insets` event is always zeros (`waylandcompositor.cpp:196`, `:1501`).
- There is no request for status bar style, colour or translucency, and no event with the bar height. The status bar is 28 x scale and the app window is laid out between the bars in QML (`shell/qml/MarathonShell.qml:2006-2009`, `shell/qml/core/Constants.qml:97-111`); a client is configured to the already-reduced size.
- `zwlr_layer_shell_v1` is not implemented (`wayland-info`, and the only hit in `shell/` is the sandbox deny list at `shell/src/wayland/securitycontextv1.cpp:37`).

`StatusBar` keeps recording what the app asked for and applies nothing.

## Keyboard: no contract for avoidance

- Clients get `zwp_text_input_manager_v2` and `v3` (`waylandcompositor.cpp:164-185`). The on-screen keyboard opens from the client's text-input enable (`waylandcompositor.cpp:1370-1390`).
- `marathon_surface_v1.ime_inset(bottom)` exists (`marathon-surface-v1.xml:173-181`) and the shell sends 0 (`waylandcompositor.cpp:196`, `:1501`). The keyboard height shrinks the QML item (`shell/qml/MarathonShell.qml:2009`, `:3028-3029`), and the compositor viewport adds it back (`:3159-3163`), so by the source the toplevel is not configured smaller when the keyboard opens. This is a reading of the source; it was not observed. The VM run did not check whether focusing the field made the client send a text-input enable, and no keyboard panel appears in its screenshots.
- No document in `docs/` mentions `KeyboardAvoidingView` or `ime_inset`.

`KeyboardAvoidingView` keeps rendering a plain `View`. When the shell sends a real `ime_inset`, the host can emit `keyboardDidShow` and `keyboardDidHide` and the view can use them.
