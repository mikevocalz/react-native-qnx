# Desktop windows and interactive gallery validation

The monitor application can opt into a native Qt Quick desktop frame around
its existing React Native surface. The frame provides a title, window controls,
File/Window/Help menus, keyboard shortcuts, focus feedback, and resize handles.
Fabric and Yoga still own the application content. Resizing the client area
updates the React surface constraints; title-bar height is excluded from both
layout and React touch coordinates.

This is an experimental, default-OFF build feature. It needs a rebuilt native
executable and the matching Qt Quick Controls Basic runtime plugins. Replacing
only the JavaScript or Hermes bytecode cannot add this frame or repair the
native input dispatcher.

Baseline React pointer delivery works with the input lifecycle experiment OFF.
Enabling the desktop frame does not require enabling that experiment or the
text/accessibility experiments. The ordinary window, all-feature configuration,
and desktop frame with baseline input have separate
[host test receipts and screenshots](../evidence/gallery-desktop-host-2026-11-09.md).

## Build and preview

With the repository's frozen dependencies and documented host Qt prerequisites:

```bash
cmake --preset linux-host-renderer -DRNQNX_ENABLE_DESKTOP_WINDOW=ON
cmake --build build/linux-host-renderer --target rnqnx_playground_host -j 2
pnpm qnx bundle-qnx --app component-conformance --out build/component-conformance.bundle.js
mkdir -p build/assets/assets
cp apps/component-conformance/assets/icon.png build/assets/assets/icon.png
build/linux-host-renderer/bin/rnqnx_playground_host \
  --desktop --bundle build/component-conformance.bundle.js \
  --app-id org.rnqnx.showcase --app-name "QNX Components" \
  --component-name RNQnxPlayground
```

This command is a host preview. For a configured QNX consumer, retain its
existing QNX SDP, Qt, Hermes and consumer descriptor settings, reconfigure that
build with `-DRNQNX_ENABLE_DESKTOP_WINDOW=ON`, and rebuild `rnqnx_consumer_app`.
Follow the [native consumer guide](native-consumer-app.md) and existing target
runtime deployment procedure. Keep the qualified executable separate until
the replacement has passed physical acceptance.

Desktop mode adds the Qt Quick Controls/Basic QML dependency closure. Stage
those modules, their Qt Quick Templates dependencies, and matching libraries
from the same target Qt build. A host QML directory is not a QNX runtime. The
feature fails at startup with `RNQNX_DESKTOP_ERROR` if its QML cannot load;
an executable built with the feature OFF rejects `--desktop` explicitly.

## Choose who draws the frame

| Launch mode                       | Frame owner                     | Application behavior                                                                    |
| --------------------------------- | ------------------------------- | --------------------------------------------------------------------------------------- |
| `--desktop`                       | Qt Quick client frame           | Title bar, close, maximize/restore, resize handles, menus and full screen.              |
| `--desktop --system-window-frame` | Existing desktop window manager | Keep the app menus and React client area; omit the custom title bar and resize handles. |
| No `--desktop`                    | Existing host behavior          | Baseline rendering and input, with no additional Controls dependency at runtime.        |

For QNX without manager-provided decorations, the client-frame launch is:

```bash
./org.rnqnx.showcase -platform qnx:no-fullscreen --desktop
```

For an **already configured and verified** QNX desktop manager that supports
the Qt Screen management protocol, the source-backed system-frame path is:

```bash
./org.rnqnx.showcase -platform qnx:desktop:no-fullscreen \
  --desktop --system-window-frame
```

Qt's `desktop` and `no-fullscreen` QPA options are independent. Its QNX backend
requests `Frame=Y` from the manager for an ordinary window in desktop mode.
Using the system-frame option avoids drawing another title bar inside that
frame. See the inspected
[Qt 6.12 QNX integration source](https://github.com/qt/qtbase/blob/v6.12.0/src/plugins/platforms/qnx/qqnxintegration.cpp)
and [QNX window source](https://github.com/qt/qtbase/blob/v6.12.0/src/plugins/platforms/qnx/qqnxwindow.cpp).

### Minimize and restore

Qt 6.12's QNX `minimize()` implementation reports that the operation is
unsupported. The application therefore disables its own minimize action on
the QNX plugin. It does not hide the app and leave the user without a restore
path. On supported desktop plugins, the action calls Qt's real window
minimization operation.

In system-frame mode, the window manager owns its title-bar buttons and restore
UI. That complete path still needs a physical test on the selected image.
QNX documents `screenwm` as an alternative to the fullscreen launcher, with
separate movable application windows and a desktop. Merely attaching a monitor
does not establish that this manager is running. See
[QNX desktop customization](https://www.qnx.com/developers/docs/qnxeverywhere/com.qnx.doc.target_images/topic/cti/snippets_customization.html).
This change does not alter the target's boot image or running window manager.

Read-only inventory on the target helps identify the existing environment:

```bash
pidin -p screen arguments
pidin -p screenwm arguments
pidin -p fullscreen-winmgr arguments
pidin -p demolauncher arguments
pidin -p org.rnqnx.showcase arguments
```

Use the observed app PID to inspect its actual `/dev/screen/<pid>/win-*` files,
if permitted by the image. QNX documents these runtime window properties in
the [Screen debugging guide](https://www.qnx.com/developers/docs/8.0/com.qnx.doc.screen/topic/manual/cscreen_screen-debugging-app.html).
An absent command or inaccessible diagnostic is a recorded gap, not a reason
to change permissions.

## Menus and keyboard behavior

The File menu closes the current application window. Window provides
maximize/restore and full screen, plus minimize where the native operation is
available. Help provides a shortcuts dialog and application information.
Application-specific Open/Save/Edit commands require real app handlers and
are not presented as inert menu items.

| Key            | Action                                  |
| -------------- | --------------------------------------- |
| F10            | Focus the menu bar.                     |
| Arrow keys     | Navigate a focused menu.                |
| Enter or Space | Activate a focused native control.      |
| Escape         | Close a desktop menu or Help dialog.    |
| F11            | Enter or leave full screen.             |
| Ctrl+W         | Close the application window.           |
| Ctrl+M         | Minimize on a supported desktop plugin. |

An open desktop menu or Help dialog disables the underlying React client
area, preventing the same click from activating a React control beneath it.
This does not claim complete keyboard/accessibility parity for every React
Native component, platform-native file dialogs, or IME support.

## Reversed mouse-wheel input

QNX Everywhere's September 2026 release notes list Raspberry Pi wheel
direction reversal as known issue **QNXE-1445**. It is a possible explanation
for the reported behavior, not proof of the exact bug in an unidentified
custom image. The Qt QNX backend already converts the Screen wheel sign, and
the ordinary Qt Flickable host path scrolls correctly with standard Qt deltas.

For an affected device, append the explicit app-local workaround:

```bash
./org.rnqnx.showcase -platform qnx:no-fullscreen --desktop --invert-wheel
```

The workaround reverses wheel deltas once, leaving mouse dragging and touch
gestures unchanged. Omit the option to restore normal behavior. Compare one
notch in each direction before and after, then verify the beginning and end
of both document lists. Keep this choice in the application's launch command,
not in a blanket platform sign change.

For a bounded diagnostic launch, Qt's `qt.qpa.screen.events` logging category
can expose the QPA wheel values. Record the exact Qt/plugin and image version
along with observed movement. See
[QNX release notes](https://www.qnx.com/developers/docs/qnxeverywhere/rel_notes.html)
and [Qt's QNX event conversion](https://github.com/qt/qtbase/blob/v6.12.0/src/plugins/platforms/qnx/qqnxscreeneventhandler.cpp).

## Required physical acceptance

PR #104's owner reported that the app renders but tabs/buttons fail, scrolling
is reversed, and the Switch looks wrong. That observation is a rendering pass
with interaction failures. It is not a qualification of this replacement.

| Case            | Exercise on the rebuilt native executable                                                                                      |
| --------------- | ------------------------------------------------------------------------------------------------------------------------------ |
| Pointer routing | Click text and padding of a button, repeat twice, then try a disabled control.                                                 |
| Navigation      | Visit Components, Documents, Sections, Controls, Adaptive and Limits, then return with retained state.                         |
| Modal           | Open from scrolled content; verify full client coverage, blocked background input, resize, close, then use the underlying app. |
| Switch          | Check transparent corners, distinct off track/thumb, on/off callbacks, disabled behavior and retained state.                   |
| Wheel           | Test both directions and both list boundaries with the chosen wheel policy; check dragging separately.                         |
| Window          | Close, maximize/restore, resize and toggle full screen; confirm React content stays below the menus.                           |
| Keyboard        | Focus and traverse menus, activate actions and dismiss the Help dialog.                                                        |
| Desktop manager | In system-frame mode, minimize and restore from the manager; confirm the same app state and no duplicate frame.                |

Attach executable and HBC hashes, source revision, feature flags, Qt/plugin
and QNX image identities, manager inventory, launch command, logs, screenshots
and actual case outcomes. Native host tests and screenshots remain host
evidence. Leave physical cases unqualified until they are actually performed.
