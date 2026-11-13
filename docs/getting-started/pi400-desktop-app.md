# Run a React Native app on the Pi 400 desktop

This guide takes an app from source to a window on the QNX desktop of a
Raspberry Pi 400: build, deploy, launch, and what the title bar buttons do
there. It uses `apps/component-gallery` as the example. Start with
[the Pi 400 bring-up](raspberry-pi-400.md) if the board does not boot to the
QNX desktop yet.

Everything here is experimental. The build and deploy steps were run for one
Pi 400 with the QNX SDP 8.0.5 image, public-source Qt 6.12.0 and the stock QNX
launcher. On 2026-11-12 the board ran, under `rnqnx-winmgr` started by hand:
the app as a 1280 x 800 window over the desktop, moving and resizing it by
the title bar and a corner, minimize and restore from the dock, maximize and
back, restore by running the launch script again, and a click beside the
window reaching the launcher. Those runs used pointer events injected through
Screen; `demolauncher` ignores injected clicks, so the tile itself was not
driven that way.

## Develop with Fast Refresh

The build below is a release build: bundled bytecode, no Metro. For an
edit-and-see loop on the board, build the development client once and run
`rnqnx project dev`; see [the development workflow](dev-workflow.md). On the
Pi 400 here it is installed as `/data/home/qnxuser/run-chrome-app-dev.sh`
beside the release app.

## What the desktop runs

At boot, `/system/etc/startup/full_startup.sh` starts, in order:

1. USB input: `usb_start.sh`, `usblauncher_otg` and `io-hid -dusb`, then
   `chown 36 /dev/io-hid/io-hid` so members of the `screen` group (36) get
   mouse and keyboard events. The Pi 400 image runs `pi400-recover.sh` as
   `post_start`, which skips the stock USB block, so these lines live in
   `full_startup.sh`.
2. The Pi 400 graphics drivers from `/data/pi400-graphics-compat`
   (`drm-rpi4` and Screen with `graphics-pi400-hdmi2.conf`).
3. GPU access for apps: `chgrp 36` and `chmod 0660` on `/dev/drm/card0` and
   `/dev/drm/renderD128`. Mesa opens both, and apps run as `qnxuser`, who is
   in group 36.
4. The desktop: a window manager, then
   `demolauncher -a /system/etc/desktop_files`. The stock image starts
   `fullscreen-winmgr -s demolauncher,st,gtk`; after the install step in
   [Desktop window manager](#desktop-window-manager) it starts `rnqnx-winmgr`
   instead, and falls back to `fullscreen-winmgr` if that one cannot run.

No React Native app starts at boot. Launch apps by hand (below).

Check the input and GPU nodes before debugging an app:

```sh
ls -l /dev/io-hid/io-hid /dev/drm/card0 /dev/drm/renderD128
id    # qnxuser must list group 36(screen)
```

## Build

Builds run on an x86-64 Linux host with QNX SDP 8.0.5 (the `qnx805-build`
container in this project). Qt is the public-source 6.12.0 build from
[`scripts/qt-public/`](../../scripts/qt-public/README.md), with the QNX
plugin patch from PR #136 applied (see "GPU rendering" below).

```bash
source /path/to/qnx800/qnxsdp-env.sh
pnpm install --frozen-lockfile

# 1. JavaScript: Metro bundle, then Hermes bytecode with the pinned compiler.
node packages/react-native-qnx-cli/src/bin.mjs bundle-qnx \
  --app component-gallery --out build/gallery/app.bundle.js
"$HERMESC" -O -emit-binary -out apps/component-gallery/build/qnx/app.hbc \
  build/gallery/app.bundle.js

# 2. App identity, icon set and window settings into native-app.json.
cmake -S native/app-identity -B build/identity -G Ninja -DRNQNX_BUILD_IDENTITY_TOOL=ON
cmake --build build/identity
node packages/react-native-qnx-cli/src/bin.mjs project prepare-native \
  --project apps/component-gallery --platform qnx --bundle build/qnx/app.hbc \
  --identity-tool build/identity/rnqnx_identity_tool

# 3. The native host with the title bar compiled in.
pnpm qnx project build-native --project apps/component-gallery --window-chrome
```

The 2026-11-12 build ran the same CMake configure directly with the
`qnx-aarch64-release` preset, a toolchain wrapper that appends the public Qt
build to `CMAKE_FIND_ROOT_PATH` (it lives outside the SDP sysroot), and
`-DRNQNX_ENABLE_WINDOW_CHROME=ON -DRNQNX_ENABLE_INPUT_LIFECYCLE=ON`.

`$HERMESC` is `hermesc` from the pinned `hermes-compiler@260318099.0.5`
package (`node_modules/.pnpm/hermes-compiler@260318099.0.5/.../linux64-bin/hermesc`).
The bytecode must come from the same Hermes version as `libhermesvmlean.so`
on the board.

Always run `prepare-native` with the identity tool. It writes the icon set
(16 to 512 px) that the window and the title bar use. An app without
`expo.icon` gets the template's default icon from `project create`; see
[App icon and tile name](app-identity.md). `--skip-identity-tool` builds an app
with no icon set, and the title bar then shows the title alone.

The output directory is
`apps/component-gallery/build/qnx/native/consumer/qnx/org.rnqnx.showcase/`:
the executable (named after the app id), `app.bundle.js` and `icons/`.

## Deploy

Copy the build into a new directory on the board, next to the one that runs
now, and switch the launcher only after the new build works:

```sh
# On the Pi, as qnxuser
mkdir -p /data/home/qnxuser/rnqnx-chrome-app-next/assets/assets
```

Copy into that directory:

- `org.rnqnx.showcase` and `app.bundle.js`
- `icons/`
- `libhermesvmlean.so` from the QNX Hermes build
- `assets/icon.png` as `assets/assets/icon.png` (the gallery's Image card)

## Launch

The launcher script sets a self-contained environment. This is
`/data/home/qnxuser/run-chrome-app.sh` on the test board:

```sh
#!/bin/sh
# Launch by hand only; not part of boot or autostart.
A=/data/home/qnxuser/rnqnx-chrome-app
Q=/data/home/qnxuser/qt612-public
export LD_LIBRARY_PATH="$A:$Q/lib:/data/pi400-graphics-compat/lib/dll:/data/pi400-graphics-compat/usr/lib:/data/pi400-graphics-compat/usr/lib/graphics/rpi4-drm:/usr/lib"
export QT_QPA_PLATFORM=qnx:no-fullscreen
export QT_QPA_PLATFORM_PLUGIN_PATH=/data/home/qnxuser/glfix/plugins/platforms
export QT_PLUGIN_PATH=/data/home/qnxuser/glfix/plugins
export QML_IMPORT_PATH=$Q/qml
export QML2_IMPORT_PATH=$Q/qml
export QT_QPA_FONTDIR=/usr/share/fonts/dejavu
export QT_FORCE_STDERR_LOGGING=1
# Started from the launcher tile there is no terminal; keep a log.
[ -t 1 ] || exec >>/tmp/rnqnx-showcase.log 2>&1
cd "$A"
exec ./org.rnqnx.showcase "$@"
```

Run it from a terminal on the desktop (the `st` tile) or over SSH. Over SSH
the app still opens on the board's display. Useful arguments:

| Argument                  | Effect                                         |
| ------------------------- | ---------------------------------------------- |
| (none)                    | Window at the app's `initialSize`, centred     |
| `--fullscreen`            | Full screen, no title bar                      |
| `--window-size 1600x900`  | Overrides `initialSize`                        |
| `--title-bar-style none`  | Overrides `titleBarStyle`                      |
| `--screenshot <file.png>` | Renders the window once, writes the PNG, exits |
| `--describe-app`          | Prints the app id, name and icon sizes, exits  |

`--screenshot` grabs the app's own window. The board's `screenshot` tool needs
Screen privileges that `qnxuser` does not have.

### GPU rendering

Qt Quick renders with OpenGL ES on the Pi 400's V3D GPU. That needs the QNX
platform plugin built with the PR #136 patch
(`scripts/qt-public/patches/qtbase-qnx-offscreen-surface.patch`): Qt Quick's
render thread asks for an offscreen surface, and the stock 6.12.0 plugin
returns none, which drew a black window under `fullscreen-winmgr`. The patched
`libqqnx.so` is on the board in `/data/home/qnxuser/glfix/plugins/platforms`,
which is why both plugin paths point there.

Software rendering is the fallback for a board without the patched plugin:
point `QT_QPA_PLATFORM_PLUGIN_PATH` and `QT_PLUGIN_PATH` at the stock runtime
and add `export QT_QUICK_BACKEND=software`. It is slower, and `--screenshot`
returns no image with it.

## Windowed or full screen

An app with `extra.rnqnx.window.initialSize` opens as a window of that size
(title bar included), centred on the display. The gallery asks for
1280 x 800. Without `initialSize`, or with `--fullscreen`, the app fills the
display. Run the app with `QT_QPA_PLATFORM=qnx:no-fullscreen`; with plain
`qnx` the Qt plugin makes the first window full screen.

What the window is depends on the window manager. At start the app sets its
window's `SCREEN_PROPERTY_MANAGER_STRING` to `rnqnx:hello` and waits up to
500 ms for the answer (`RNQNX_WINDOW_MANAGER=windowed` or `fullscreen` skips
the wait). The startup line reports the result as `mode=`:

- **`rnqnx-winmgr`** answers `rnqnx-winmgr windowed`
  (`RNQNX_WINDOW_CHROME ... window=1280x800 mode=native-moved`). The app gets
  a real native window of its own size. The QNX desktop, its wallpaper and
  tiles stay visible and clickable around it, and the title bar code moves and
  resizes the native window.
- **`fullscreen-winmgr`** sets every window to the display size each time the
  window posts a frame and hides every other window, the launcher included
  (`mode=contained`). The native window fills the display and the title bar
  code draws the app's window inside it: a frame with a 1 px outline on the
  dark desktop colour. The area around it is the app's own window, so it
  stays dark.

- Drag the title bar to move the window. The bar stays on the display.
- Drag an edge to resize; within 16 px of a corner both edges move. The
  window keeps at least 400 x 300. Screen has no resize cursors, so a resize
  arrow appears beside the pointer while it is on an edge.
- The React app is laid out again at the new size.
- After each move or resize the log shows
  `RNQNX_WINDOW_FRAME 1280x800+320+140`, the window's place on the display.

## Title bar buttons

Under `rnqnx-winmgr`:

| Button            | What happens                                                                                    |
| ----------------- | ----------------------------------------------------------------------------------------------- |
| Close (red)       | Closes the window; the app exits and its dock entry goes.                                       |
| Minimize (yellow) | The window hides; its dock entry dims. The app keeps running with its state.                    |
| Expand (green)    | Toggles between the screen and the window's last size and place. A double click on the bar too. |

Click the dock entry, or the app's tile, to bring a minimized window back.

Under `fullscreen-winmgr`:

| Button            | What happens                                                                                                            |
| ----------------- | ----------------------------------------------------------------------------------------------------------------------- |
| Close (red)       | Closes the window; the app exits.                                                                                       |
| Minimize (yellow) | The window goes away and the launcher shows. The app keeps running with its state (tab, press count, text).             |
| Expand (green)    | Toggles between the full display and the window. A double click on the bar does the same. Hidden for a full-screen app. |

The green glyph shows two triangles pointing out while the window is compact
and two pointing in while it fills the display.

To get a minimized app back, click its tile on the desktop (below) or run the
launcher script again. The new process finds the running one, tells it to
show its window, and exits:

```text
RNQNX_SINGLE_INSTANCE restored the running org.rnqnx.showcase   (second launch)
RNQNX_WINDOW_ACTION restore                                     (running app)
```

Under `fullscreen-winmgr`, Alt+Tab does not list a minimized app.
`fullscreen-winmgr` lists windows, and minimize has to release the app's
window, because that is the only event that makes this window manager show
the launcher again. Its Alt+Tab bar shows a preview of each window and no
title or icon; the manager ignores the `Title` message. Under `rnqnx-winmgr`
a minimized app keeps its window and stays in the Alt+Tab list (below).

## One instance per app

There is never more than one copy of an app on the board. The rules:

- The instance key is the app's base id: the app id without a build-variant
  suffix (`.dev`, `.debug`, `.preview`; the list is `buildVariantSuffixes()` in
  `native/window-chrome/WindowHost.cpp`). `org.rnqnx.showcase` and
  `org.rnqnx.showcase.dev` are one app. The first process listens on the local
  socket `/tmp/rnqnx-<base id>`: a fixed directory (created if missing, so a
  session's `TMPDIR` does not matter; `RNQNX_SOCKET_DIR` overrides it), no uid
  in the name, and world read/write, so any login (the desktop launcher, SSH,
  root) reaches it.
- Starting is one step under a lock any login can take (`<socket>.lock`, an
  `fcntl` record lock that disappears with its process). Launches that start
  together cannot both become the instance: the loser finds the owner and
  restores it. A socket file with nobody behind it (the app was killed) is
  removed; one that cannot be removed gets the per-user fallback path
  `<socket>.<uid>`, which later launches also check.
- A second launch restores the running window and exits. This is the default,
  and what the tile does.
- `--replace` (used by `rnqnx project dev` and `project run`) asks the running
  copy to quit over the socket (`quit`), waits up to 5 s for its process to
  exit, then sends SIGTERM to the pid it reported and waits 3 s more. It never
  sends SIGKILL; if the old copy will not go, the launch fails and starts
  nothing. A quit that arrives while the app fetches its bundle, shows the dev
  error screen or reloads still ends the app.
- The socket answers `restore`, `info` and `quit` with `ok pid=<n>
kind=<dev|release>`, so a launcher can tell which build is running; anything
  else gets `error unknown command`.
- `--restore-only` shows the running copy and exits 75 without starting when
  none answers. `--stop-running` quits it and exits.
- `run-app.sh` checks `pidin` for the app's executable (any variant suffix)
  before it starts the app. As a normal user it sees that user's processes; as
  root it sees all. A copy it cannot reach over the socket is stopped with
  SIGTERM before one new copy starts, so there is still exactly one.
- `rnqnx-winmgr` collapses windows of one app into a single dock entry. If a
  second window of an app appears anyway, the first window is raised and the
  manager logs `second window of '<app>'` to `/tmp/rnqnx-winmgr.log`; it kills
  nothing.

Builds made before this change listen on `rnqnx-<id>-<uid>` and answer
`restore` only. A new launch still restores them; `--replace` cannot quit
them over the socket, and `run-app.sh` stops them by pid. Every app is rebuilt
with this change, so this only matters while an old build is still running.

## Launcher tile

`demolauncher` draws the desktop tiles from `/system/etc/desktop_files`:
`desktop.conf` holds one `[Launcher]` block per tile, and each block names a
`.desktop` file with the tile's name, command and icon. Generate both from the
app's identity after `prepare-native` (the tile uses the 128 px icon from the
generated icon set):

```bash
node packages/react-native-qnx-cli/src/bin.mjs project desktop-entry \
  --project apps/component-gallery \
  --app-dir /data/home/qnxuser/launcher-tile \
  --run-as qnxuser
```

Without `--exec` the tile runs `<app-dir>/run-app.sh`, the guarded launcher
(below). Pass `--exec` only for a launcher script of your own, and make it do
what `run-app.sh` does.

It writes `build/qnx/desktop/`:

| File                         | Contents                                                                                                       |
| ---------------------------- | -------------------------------------------------------------------------------------------------------------- |
| `org.rnqnx.showcase.desktop` | `Name` (the tile label), `Exec`, `Icon=<app-dir>/icons/icon-128.png`, `Singleton=Y`                            |
| `launcher-tile.conf`         | The `[Launcher]` block: row 1 after Developer Desktop, Terminal and Readme, drawn with its name                |
| `run-app.sh`                 | The guarded launcher the tile runs: Qt environment, and the one-instance check                                 |
| `install-tile.sh`            | Copies the tile and block into `/system/etc/desktop_files` (appending once), and `run-app.sh` into `--app-dir` |

`--run-as qnxuser` matters on this image: `demolauncher` runs as root, so
without it the app would run as root. `--row` and `--order` move the tile.

On the board, copy the four files and `icons/icon-128.png` into
`--app-dir`, then as root:

```sh
cd /data/home/qnxuser/launcher-tile && sudo sh ./install-tile.sh
sudo slay demolauncher; sudo on -d demolauncher -a /system/etc/desktop_files
```

The script keeps the first `desktop.conf` as `desktop.conf.before-rnqnx-tile`.
To remove the tile, copy that file back over `desktop.conf`, delete the
`.desktop` file and restart `demolauncher`. If the screen stays black after
restarting the launcher, start `fullscreen-winmgr -s demolauncher,st,gtk`
again, then the launcher.

Clicking the tile starts the app; with the app minimized it brings the running
copy back. The test board has had this tile since 2026-11-12.

## Desktop window manager

`fullscreen-winmgr` cannot show the desktop next to an app. It makes exactly
one window per display visible, the most recent one, hides the rest (the
launcher too) and resets every window to the display size on each frame. Its
source is not published: the SDP's Screen tutorials include a `win-manager`
sample, but under a QNX confidential licence that forbids redistribution and
modification without a written licence.

`tools/qnx-winmgr/rnqnx-winmgr.c` is a small window manager of our own
(MIT, like the rest of this repository), written against the public Screen
API. It speaks the launcher protocol `demolauncher` uses with
`fullscreen-winmgr`, so the stock launcher runs unchanged:

- The launcher window fills the display at the bottom of the stack and stays
  visible: wallpaper, tiles and the bottom banner.
- App windows keep the size and position they ask for and stack above the
  launcher. Clicking a window behind another brings it to the front.
- A dock floats above the launcher's bottom banner while any app window
  exists. Each entry shows the app's name and icon, taken from the launcher's
  `.desktop` file (the launch tag, or a file named after the program such as
  `org.rnqnx.showcase.desktop`). Click an entry to restore a minimized window,
  raise a covered one, or minimize the one in front.
- `Tag=`, `switch=` and `closed=` messages work as before, so a tile with
  `Singleton=Y` brings back a running app instead of starting another.
- Alt+Tab, as the launcher's banner says. Hold Alt and press Tab: a panel in
  the middle of the display lists every app window, minimized ones included
  (dimmed), with the icon and name from the same `.desktop` file as the dock,
  most recently used first. The window after the front one is selected. Tab
  again (or the arrow keys) moves the selection, Shift+Tab moves back, and
  releasing Alt brings the selected window to the front, restoring it if it
  was minimized. A click on a tile does the same. Escape closes the panel
  without switching.
- Alt+Tab+Del closes the selected app: with the panel open, press Delete. The
  manager sends the window a close event, the same request as the title-bar
  close (Qt calls `QWindow::close()`), and the tile disappears when the
  window goes. An app that still has its window three seconds later gets
  `SIGTERM`.
- `WINMGR_KEYMOD` (`shift`, `ctrl`, `alt`, `ralt`) and `WINMGR_HOTKEY`
  (`tab`, `space`, `grave`, `f1` to `f12`, `q`, `w`, `t`, `m`) change the keys,
  with the values `fullscreen-winmgr` takes. Set them in the environment that
  starts `rnqnx-winmgr`.

To see Alt+Tab, the manager routes every keyboard to an input session of its
own (`screen_create_session_type(..., SCREEN_EVENT_KEYBOARD)` and each
keyboard's `SCREEN_PROPERTY_SESSION`, the way `fullscreen-winmgr` does) and
injects every key it does not use back into the display, where it reaches the
focused window. Typing in an app is unchanged; a keyboard plugged in later is
routed when Screen reports it.

Build it with the SDP (the container in this project):

```bash
source /work/qnx800/qnxsdp-env.sh
tools/qnx-winmgr/build.sh build/qnx-winmgr/rnqnx-winmgr
```

Try it without touching boot. As root on the Pi, with the binary in
`/data/rnqnx-winmgr/`:

```sh
slay fullscreen-winmgr demolauncher
on -d /data/rnqnx-winmgr/rnqnx-winmgr -s demolauncher,st,gtk
sleep 2; on -d demolauncher -a /system/etc/desktop_files
```

`-v` logs every Screen event to stderr. To go back to the stock desktop from
a hand-started `rnqnx-winmgr`:

```sh
slay rnqnx-winmgr demolauncher
on -d fullscreen-winmgr -s demolauncher,st,gtk
sleep 2; on -d demolauncher -a /system/etc/desktop_files
```

To start it at boot, copy `rnqnx-winmgr`, `tools/qnx-winmgr/rnqnx-desktop.sh`
and `tools/qnx-winmgr/install-boot.sh` into one directory on the Pi and run
`sh install-boot.sh` as root. It:

1. copies the binary to `/data/rnqnx-winmgr/` (root-owned, since it runs as
   root at boot);
2. writes `/system/etc/startup/rnqnx-desktop.sh`, the stock desktop start
   with `rnqnx-winmgr` as the window manager. If the binary is missing or
   exits within two seconds, it starts `fullscreen-winmgr` instead. If
   `rnqnx-winmgr` stops later, a watcher starts `fullscreen-winmgr` and a
   fresh `demolauncher`, so `slay rnqnx-winmgr` is also how to switch a
   running board back to the stock desktop;
3. keeps `full_startup.sh` as `full_startup.sh.before-rnqnx-winmgr` and
   changes its last line to run `rnqnx-desktop.sh`.

USB input, the graphics drivers, Screen, the launcher and its tiles start as
before. `sh install-boot.sh --undo` puts the old `full_startup.sh` back.

Each press logs `RNQNX_WINDOW_ACTION close|minimize|maximize`.

## Mouse wheel

ScrollView, FlatList and SectionList scroll 100 px per wheel notch, eased over
120 ms, on every platform. Qt's QNX plugin reports a notch as 10 angle units
instead of the usual 120, and Qt's `Flickable` scaled its wheel fling for 120,
so one notch used to move a page a few pixels. A scroll view at its edge
passes the wheel on to the scroll view around it. Shift+wheel scrolls
sideways.

## Troubleshooting

**Black window or black screen.** Check that both plugin paths point at the
patched plugin in `glfix`. If the patched plugin is missing, use the software
fallback above. Confirm `/dev/drm/renderD128` is group 36 and mode 0660.

**No mouse pointer, or clicks do nothing.** `ls -l /dev/io-hid/io-hid` must
show owner 36. If the node is missing, `io-hid` did not start; check
`/tmp/pi400-usb.log`. Re-plug the mouse after `io-hid` is running.

**Two windows of one app.** Not possible with a build that has the guard;
check `pidin -F "%a %N %U" | grep <app>` and the app's launcher. Start apps
through `run-app.sh` (or the tile), not the binary, and use `--replace`.

**The app will not close.** Press the red light or close the terminal it runs
in. From SSH, `pidin -F "%a %N" | grep org.rnqnx` gives the pid, then
`kill <pid>`; never `kill -9`. The app exits on SIGTERM. A second launch while
it runs does not start a second copy; it brings the first one back.

**The window fills the screen although `initialSize` is set.** `--fullscreen`
turns windowed mode off. Check that the build has `initialSize` in
`native-app.json`. Under `fullscreen-winmgr` the startup line
`RNQNX_WINDOW_CHROME ... window=1920x1080 mode=contained` is expected, because
the native window is always display-sized there.

**Black around the app.** The startup line says `mode=contained`: either the
window manager is `fullscreen-winmgr` (`pidin -F "%a %N" | grep winmgr`), or
the app runs with `QT_QPA_PLATFORM=qnx` instead of `qnx:no-fullscreen`.

**The tile does nothing.** `demolauncher` reads tiles only when it starts, and
it rejects a `.desktop` file with a second `=` on a line. Run
`project desktop-entry` again rather than editing the file by hand.

**Text looks like a serif face.** The host picks the first of Inter, Roboto,
Noto Sans and DejaVu Sans that the font directory has. With
`QT_QPA_FONTDIR=/usr/share/fonts/dejavu` that is DejaVu Sans.

**A view is missing.** Mount failures are logged as
`RNQNX_MOUNT_FAILURE type=<n> tag=<n> error=<n> <detail>`.
