# Run a React Native app on the Pi 400 desktop

This guide takes an app from source to a window on the QNX desktop of a
Raspberry Pi 400: build, deploy, launch, and what the title bar buttons do
there. It uses `apps/component-gallery` as the example. Start with
[the Pi 400 bring-up](raspberry-pi-400.md) if the board does not boot to the
QNX desktop yet.

Everything here is experimental. The build and deploy steps were run for one
Pi 400 with the QNX SDP 8.0.5 image, public-source Qt 6.12.0 and the stock QNX
desktop. The windowed start, minimize/restore and expand behaviour described
below is built and covered by host tests; its first run on the board is
pending (2026-11-12).

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
4. The stock desktop: `fullscreen-winmgr -s demolauncher,st,gtk` and
   `demolauncher -a /system/etc/desktop_files`.

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
export QT_QPA_PLATFORM=qnx
export QT_QPA_PLATFORM_PLUGIN_PATH=/data/home/qnxuser/glfix/plugins/platforms
export QT_PLUGIN_PATH=/data/home/qnxuser/glfix/plugins
export QML_IMPORT_PATH=$Q/qml
export QML2_IMPORT_PATH=$Q/qml
export QT_QPA_FONTDIR=/usr/share/fonts/dejavu
export QT_FORCE_STDERR_LOGGING=1
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

An app with `extra.rnqnx.window.initialSize` opens as a normal window of that
size (title bar included), centred on the display. The gallery asks for
1280 x 800. Without `initialSize`, or with `--fullscreen`, the app fills the
display.

How it works on QNX: the host adds the qnx plugin's `no-fullscreen` option
before Qt loads (`QT_QPA_PLATFORM=qnx` becomes `qnx:no-fullscreen`), so Qt
creates the window at the requested size. `fullscreen-winmgr` resizes every
new window to the display when it first appears and ignores the app's size
after that. The host puts the window back to its size during the first two
seconds and logs it:

```text
RNQNX_WINDOW_GEOMETRY restored=1280x800+320+140 was=1920x1080+320+140
```

`fullscreen-winmgr` shows one window per display, the one used last, and
hides the rest, the launcher included. The area around a windowed app is
therefore the empty display, not the launcher.

## Title bar buttons under fullscreen-winmgr

| Button            | What happens                                                                                                                                 |
| ----------------- | -------------------------------------------------------------------------------------------------------------------------------------------- |
| Close (red)       | Closes the window; the app exits.                                                                                                            |
| Minimize (yellow) | The window goes away and the launcher shows. The app keeps running with its state (tab, press count, text).                                  |
| Expand (green)    | Toggles between the full display and the `initialSize` window. A double click on the bar does the same. Shown only when the app is windowed. |

To get a minimized app back, launch it again: run the launcher script, or tap
its launcher tile if you added one (below). The new process finds the running
one, tells it to show its window, and exits:

```text
RNQNX_SINGLE_INSTANCE restored the running org.rnqnx.showcase   (second launch)
RNQNX_WINDOW_ACTION restore                                     (running app)
```

Alt+Tab does not list a minimized app. `fullscreen-winmgr` only knows windows,
and minimize releases the app's window, because that is the only way this
window manager shows the launcher again.

Each press logs `RNQNX_WINDOW_ACTION close|minimize|maximize`.

### Optional launcher tile

`demolauncher` reads tiles from `/system/etc/desktop_files` when it starts.
A tile for the gallery, written as root:

```ini
# /system/etc/desktop_files/qnx-components.desktop
[Desktop Entry]
Name=QNX Components
Exec=/data/home/qnxuser/run-chrome-app.sh
Icon=/data/home/qnxuser/rnqnx-chrome-app/icons/icon-128.png
Singleton=Y
Categories=Utilities
```

plus a `[Launcher]` block naming `qnx-components.desktop` in
`desktop.conf`. The launcher picks it up the next time it starts. This
project's test board has no such tile; the owner adds it if wanted.

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

**The app will not close.** Press the red light or close the terminal it runs
in. From SSH, `pidin -F "%a %N" | grep org.rnqnx` gives the pid, then
`kill <pid>`; never `kill -9`. The app exits on SIGTERM. A second launch while
it runs does not start a second copy; it brings the first one back.

**The window fills the screen although `initialSize` is set.** Look for
`RNQNX_WINDOW_GEOMETRY` in the log. Its absence means the window was already
at its size. A `-platform` argument or `--fullscreen` turns windowed mode off.

**Text looks like a serif face.** The host picks the first of Inter, Roboto,
Noto Sans and DejaVu Sans that the font directory has. With
`QT_QPA_FONTDIR=/usr/share/fonts/dejavu` that is DejaVu Sans.

**A view is missing.** Mount failures are logged as
`RNQNX_MOUNT_FAILURE type=<n> tag=<n> error=<n> <detail>`.
