# Held `down` with key repeat on the Pi 400 (2026-11-19)

Closes the held-key gap in `../pr149-pi400-run2-2026-11-19/`. Run 2 used `keydown down sleep 2500 keyup down`, which sends one key-down event, so it could not show key repeat. This run uses the new `keyhold` command of `tools/qnx-input/rnqnx-inject`.

## Why run 2 showed nothing

`send_key` set `SCREEN_FLAG_KEY_DOWN` once. QNX Screen defines a separate flag, `SCREEN_FLAG_KEY_REPEAT` ("Event contains keyboard mapping for key repeat", [Screen event flags, QNX 8.0](https://www.qnx.com/developers/docs/8.0/com.qnx.doc.screen/topic/group__screen__events_1Screen_Flags_Masks.html); the same text is in `screen/screen.h` of SDP 8.0.5, where `SCREEN_PROPERTY_REPEAT_DELAY` and `SCREEN_PROPERTY_REPEAT_RATE` describe the repeat timing). Qt's QNX platform plugin passes `flags & KEY_REPEAT` to `QWindowSystemInterface::handleExtendedKeyEvent` as the auto-repeat argument (`qtbase/src/plugins/platforms/qnx/qqnxscreeneventhandler.cpp`, `injectKeyboardEvent`, Qt 6.12.0). Nothing in the app was involved: with no repeat events there was nothing to repeat.

`keyhold KEY MS [HZ]` sends the key-down, waits 500 ms, sends `KEY_DOWN | KEY_REPEAT` events every `1000/HZ` ms (default 30 Hz, 1 to 120), and sends the key-up at `MS` (1 to 30000). Bad arguments print a message and exit 2: `keyhold down 0` returned `bad=2` on the Pi.

## Host and build

- Host: `qnxpi`, `QNX qnxpi 8.0.0 2026/06/05-16:21:14EDT RaspberryPi400 aarch64le`, custom Pi 400 image, display 1024x768.
- App: build 2 (commit `240dc0814dd129e806e5228d1b70b9cc27dbd2d6`) from `/data/home/qnxuser/rnqnx-pr149-2`, as in run 2. It was not re-hashed in this run.
- Injector: `/data/home/qnxuser/bin/rnqnx-inject5`, a new file (`rnqnx-inject4` untouched). Built with `tools/qnx-input/build.sh` in `qnx805-build` (`qcc -Vgcc_ntoaarch64le -O2 -Wall -Wextra -Werror`, exit 0). sha256 `3d52daac5471c49ee7046c7d2f4fa7e79740f3376c3213a7be2e7d2d270262fa` for the built file and for the deployed file (`SHA256SUMS`, `run.txt`).

## Commands

`explore.sh` and `hold.sh` ran on the Pi through `ssh ... 'sh -s' < script`. Every call below returned 0.

| Step                | Injector arguments                     | Exit |
| ------------------- | -------------------------------------- | ---- |
| Focus the window    | `click 800 350 sleep 400 move 805 352` | 0    |
| Tab, then Down      | `key tab ...`, then `key down ...`     | 0, 0 |
| Test A              | `keyhold down 800 10`                  | 0    |
| Test B (background) | `keyhold down 2500` (30 Hz)            | 0    |

Captures: `sudo -n screenshot -file=/tmp/pr149c-<n>.bmp`, all `0`. Launch and stop as in run 2 (`stop_exit=0`, then `grep=1`: no showcase process left; `rnqnx-winmgr` and `demolauncher` still running).

## What the screenshots show

The focus ring moves between the five sidebar tabs: Components, Advanced, Documents, Controls, More.

| File                                        | Focus ring on                                                         |
| ------------------------------------------- | --------------------------------------------------------------------- |
| `after-key-down.png`, `A-before.png`        | Advanced (start of test A)                                            |
| `A-after-800ms-10hz.png`                    | Components, after `keyhold down 800 10`                               |
| `B-during-2500ms.png`                       | Controls, captured about 1.5 s into `keyhold down 2500`               |
| `B-after-2500ms.png`                        | Documents, after the hold and a pointer nudge                         |
| `B-after-2s-later.png`                      | Documents, the same as the capture before it                          |
| `sidebar-crops-ex2-A0-A1-Bduring-B1-B2.png` | The sidebar of six captures side by side (cropped from the originals) |
| `stock-desktop-after.png`                   | Stock desktop after the app was stopped                               |

Test A: one press and about three repeats (at 500, 600 and 700 ms) make four presses. Starting on Advanced, four steps on a five-tab ring that wraps land on Components, which is what the capture shows. Test B: 30 Hz for 2.5 s is one press and about 61 repeats. Starting on Components, 62 steps land on Documents, as captured. These counts are my arithmetic from the injector's timing, not a count the app logged. The app log did not change (6 lines before and after), so nothing in the app records individual key events.

Result: with repeat events, a held `down` moved focus across the tabs repeatedly and wrapped, where run 2's single key-down moved it one step. The key was released correctly: the capture 2 s later shows the same focus ring on Documents as the one before it.

## Not verified

- A real USB keyboard held down. These are synthetic events through Screen. The delay, rate and flag follow the Screen documentation and Qt's handler, and the real keyboard path (`io-hid`) was not exercised.
- A scrolling list. The held key moved the sidebar focus ring, not a ScrollView or FlatList. Keyboard scrolling of a list was not tested.
- The step counts above were not logged by the app.
- The first `key tab` after the click showed no visible ring change (`after-key-tab.png`). Whether it moved focus without a ring is unknown.
- `RemoteImageCard` reported 30905 ms to load in this run, another value without an explanation.
- Pinch, rotation, multi-pointer, 1920x1080 and the JS-blocked gesture: not part of this run.
