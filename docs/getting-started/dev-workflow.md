# Development workflow: Metro, Fast Refresh and reload on a QNX board

A release build runs the Hermes bytecode staged next to the executable. A
development build (the "development client") can instead load the bundle from
Metro on your computer, apply Fast Refresh updates as you save, reload on
demand and show React Native's LogBox for errors. This page covers both halves:
building the development client once, then the daily loop.

Status: run on one Raspberry Pi 400 with QNX SDP 8.0.5, public-source Qt 6.12.0
and the component gallery, over the USB Ethernet link to a Mac (board
192.168.2.2, Mac 192.168.2.1), on 2026-11-13. Not covered: the Hermes debugger
(CDP), React DevTools, Marathon.

## The daily loop

From the app directory on the computer the board is plugged into:

```sh
rnqnx project dev \
  --device qnxuser@192.168.2.2 \
  --launcher /data/home/qnxuser/run-chrome-app-dev.sh
```

(`rnqnx` is `node <checkout>/packages/react-native-qnx-cli/src/bin.mjs`; add
`--identity <key>` and `--known-hosts <file>` if your SSH setup needs them.)

That starts Metro on port 8081, works out which of your addresses the board
reaches, and runs the launcher on the board with
`--dev-server http://<that address>:8081`. Then:

| You do                                                               | The app does                                                                                    |
| -------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------- |
| Save a component file                                                | Fast Refresh: the changed module is swapped in and component state is kept                      |
| Save a file that is not only components (for example the entry file) | Full reload                                                                                     |
| Press `r` in the Metro terminal, `Ctrl+R` or `F5` in the app         | Full reload: new React host, bundle fetched again                                               |
| Press `d` in the terminal, `Ctrl+D` or `F1` in the app               | Developer menu: Reload, Fast Refresh on/off, items the app added with `DevSettings.addMenuItem` |
| Introduce a syntax error and save                                    | LogBox shows the error over the app; fixing it and saving clears it                             |
| Throw during render                                                  | LogBox shows the error and component stack                                                      |
| Stop Metro                                                           | The app keeps running; when Metro is back it reloads by itself                                  |
| Start the app before Metro                                           | A "Could not connect to Metro" screen; it loads as soon as Metro starts                         |

To run only Metro (for example when you start the app on the board yourself):

```sh
rnqnx project start --project .
# then on the board
/data/home/qnxuser/run-chrome-app-dev.sh --dev-server http://192.168.2.1:8081
```

The launcher also reads `RNQNX_DEV_SERVER` from the environment. Without
either, the development client runs its staged bundle like a release build.

## How it works

- The bundle comes from
  `http://<server>/index.bundle?platform=qnx&dev=true&...`. It is plain
  JavaScript, so the development client links the Hermes VM that includes
  the compiler (`libhermesvm.so`); release builds keep the lean VM and
  bytecode.
- After the bundle runs, the host calls React Native's own
  `HMRClient.setup('qnx', 'index.bundle', host, port, true, 'http')`, the call
  AppDelegate and ReactHost make on iOS and Android. `HMRClient` opens
  `ws://<server>/hot` through the platform WebSocket module (Qt WebSockets) and
  evaluates each update with `globalEvalWithSourceUrl`.
- The native half React Native expects in a development bundle is provided by
  the host: `DevSettings` (reload, Fast Refresh toggle, menu items),
  `DevLoadingView` (the "Refreshing..." banner at the top) and `LogBox`
  (`show` starts a second React surface, `LogBox`, over the app, as iOS and
  Android do; `hide` stops it). Touches go to the LogBox surface while it is up.
- The host keeps Metro's `/message` socket open for `r` and `d` from the
  terminal and reconnects every 3 s if Metro goes away.
- A reload destroys the React host (runtime, scheduler, surfaces) and builds a
  new one in the same window; the window, title bar and Qt state stay.
- Errors before React can render (Metro unreachable, a syntax error in the
  first bundle, a throw while the bundle evaluates) show a native error screen
  with Reload and Dismiss.

## Build the development client

The development client is the same app built with two extra CMake options:

```cmake
-DRNQNX_ENABLE_WEBSOCKETS=ON
-DRNQNX_ENABLE_DEV_SUPPORT=ON
-DRNQNX_HERMES_QNX_DIR=<Hermes build tree that has lib/libhermesvm.so>
-DQt6WebSockets_DIR=<Qt WebSockets build>/lib/cmake/Qt6WebSockets
```

Both default to OFF. `RNQNX_ENABLE_DEV_SUPPORT` without
`RNQNX_ENABLE_WEBSOCKETS` stops at configure time.

Prerequisites, once per toolchain:

1. **Hermes with the compiler.** `scripts/configure-hermes-qnx.sh` configures
   the tree; build the `hermesvm` target in addition to `hermesvmlean`:
   `cmake --build build/hermes-qnx-aarch64 --target hermesvm jsi`. The source
   must be prepared by `scripts/prepare-hermes-qnx.sh` (it patches llvh's
   `Path.inc`, `ProcessStats.cpp` and the POSIX sampling profiler for QNX).
2. **Qt WebSockets** from the same public Qt 6.12.0 archive:
   `scripts/qt-public/build-websockets-qnx.sh`. It extracts only
   `qtwebsockets` and builds it against the existing QNX qtbase with
   `qt-configure-module`.

Deploy the build to its own directory, separate from the release app, with:

- the executable, `app.bundle.js` and `icons/` from the build;
- `libhermesvm.so` (instead of `libhermesvmlean.so`) and
  `libQt6WebSockets.so.6*`;
- `plugins/tls/libqopensslbackend.so` and `plugins/imageformats/` (see
  [image loader](../image-loader.md));
- a launcher like [`run-chrome-app-dev.sh`](#launcher) that puts the app
  directory first on `LD_LIBRARY_PATH` and `QT_PLUGIN_PATH`.

A development client uses the single-instance key `<appId>.dev`, so it can run
beside the installed release copy of the same app.

### Launcher

```sh
#!/bin/sh
A=/data/home/qnxuser/<dev app dir>
Q=/data/home/qnxuser/qt612-public-o2
export LD_LIBRARY_PATH="$A:$Q/lib:/data/pi400-graphics-compat/lib/dll:/data/pi400-graphics-compat/usr/lib:/data/pi400-graphics-compat/usr/lib/graphics/rpi4-drm:/usr/lib"
export QT_QPA_PLATFORM=qnx:no-fullscreen
export QT_QPA_PLATFORM_PLUGIN_PATH=$Q/plugins/platforms
export QT_PLUGIN_PATH=$A/plugins:$Q/plugins
export QML_IMPORT_PATH=$Q/qml QML2_IMPORT_PATH=$Q/qml
export QT_QPA_FONTDIR=/usr/share/fonts/dejavu
[ -t 1 ] || exec >>/tmp/rnqnx-showcase-dev.log 2>&1
cd "$A"
exec ./org.rnqnx.showcase "$@"
```

## Log lines

The development client prints these to its log (`/tmp/rnqnx-showcase-dev.log`
with the launcher above):

| Line                                        | Meaning                                                                        |
| ------------------------------------------- | ------------------------------------------------------------------------------ |
| `RNQNX_DEV_SERVER <url>`                    | Development mode is on                                                         |
| `RNQNX_DEV_BUNDLE_LOADED bytes=... ms=...`  | Bundle fetched from Metro                                                      |
| `RNQNX_DEV_BUNDLE_FAILED <title>: <detail>` | Fetch or build failed; the error screen is up                                  |
| `RNQNX_DEV_PACKAGER connected`              | `/message` socket open (terminal `r` and `d` work)                             |
| `RNQNX_DEV_RELOAD <reason>`                 | Reload started (`keyboard`, `Metro`, `menu`, `DevSettings`, `Metro connected`) |
| `RNQNX_DEV_LOGBOX shown` / `hidden`         | LogBox surface started or stopped                                              |
| `RNQNX_DEV_MENU open` / `closed`            | Developer menu                                                                 |

## Security

The development client fetches and runs whatever JavaScript the given server
sends, over plain HTTP by default. Use it on a private link (USB Ethernet, a
lab network) and never ship a build with `RNQNX_ENABLE_DEV_SUPPORT=ON`. Metro
listens on all interfaces; pass `--host <address>` to `project start` to limit
it. The `/message` socket accepts only `reload` and `devMenu` from Metro; the
app ignores anything else.

## Limits

- LogBox shows call stacks unsymbolicated ("Source Map" warning): symbolication
  goes through `fetch`, and React Native's networking module (RCTNetworking) is
  not implemented on QNX yet.

- No Hermes inspector (Chrome DevTools) or React DevTools connection. The
  React DevTools hook is installed by React Native's bundle and stays idle.
- Assets load from Metro over HTTP in development, from `assets/` beside the
  executable in release builds.
- The first bundle after Metro starts takes Metro's cold build time (about 10 s
  for the gallery on an M-series Mac); the banner counts the seconds.
