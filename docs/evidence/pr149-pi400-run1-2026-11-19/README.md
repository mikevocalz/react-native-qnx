# PR 149 on the Pi 400, run 1 (2026-11-19)

Build 1 of PR #149 (`feat/worklets-reanimated-gesture-handler-qnx`), run on the board by hand over SSH.
Worklets, Reanimated and Gesture Handler were ON. Qt SVG and RNSVG were OFF.

## Host and build

- Host: `qnxpi`, `QNX qnxpi 8.0.0 2026/06/05-16:21:14EDT RaspberryPi400 aarch64le`. This is the custom Raspberry Pi 400 QNX image, not a stock Pi 4. Display 1024x768 (`RNQNX_WINDOW_CHROME ... window=1024x768`).
- Commit built: `26bd6aa238b7ee535e73e75d79017749b64f22e3`. Cross-built with QNX SDP 8.0.5, Qt 6.12.0 public, Hermes `hermes-qnx-full` (details in `build-receipt.md`).
- Deployed to `/data/home/qnxuser/rnqnx-pr149-1` on 2026-11-19 05:50 UTC, Qt runtime `/data/home/qnxuser/qt612-public-o2`.
- Hashes (`SHA256SUMS`, as printed in `deploy.txt`): executable `942c64cea3fc603be2e2343fcac557a5f132f2639c7e57aa8375ecf526726419`, `app.bundle.js` `358527886692ba802744dda0c6339b4713f5fde11c8ccbb270152596600204a5`, `libhermesvm.so` `a48934c6560d8da5adeb7fadb7469e4c02b9dd925846fdbb69bf36f78f96f62f`.
- The deploy log shows `mk=0` and `scp=0`. It also holds a `bash: line 1: sha256: command not found` line from a wrong hash command on the Pi, and the three hashes above came from a later command in the same log.

## What was kept

Only the deploy log, the app log (`app.txt`, the part from the 05:50 UTC start) and the screenshots survive from this run. The injector command lines and their exit statuses were not saved, so this README does not list them and run 1 gives no command-by-command receipt. Run 2 (`../pr149-pi400-run2-2026-11-19/`) repeats the interaction checks with full command logs.

## Screenshots

| File                                                     | Shows                                                                                                                                                                |
| -------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `app-window-first-screen.png`                            | Gallery window at 1024x768, produced by the app's own capture (`RNQNX_PLAYGROUND_SCREENSHOT /tmp/pr149-shot.png 1024x768`). It reads the Qt window, not the display. |
| `worklets-proof.png`                                     | Advanced tab, "react-native-worklets on the UI thread" card after "Run worklets proof" (text below).                                                                 |
| `reanimated-spring-mid.png`                              | Reanimated card, spring running: box mid-travel, status "spring running, UI frame callback count 0".                                                                 |
| `reanimated-spring-end.png`                              | Same card after the spring: "spring finished".                                                                                                                       |
| `reanimated-end-state.png`                               | Reanimated card with the box at its end position and "spring finished, UI frame callback count 6534".                                                                |
| `gesture-card-before.png`                                | Gesture Handler card, "Drag me, tapped 0", status "drag the card".                                                                                                   |
| `gesture-pan-release-1.png`, `gesture-pan-release-2.png` | After a drag: status "released at 286, 25, springing back"; the card is shifted in the first capture, back in place in the second.                                   |
| `gesture-tap.png`                                        | After a tap: "Drag me, tapped 1", status "pan began".                                                                                                                |
| `gesture-swipe-list.png`                                 | Swipeable list after a swipe: the rows read Drafts and Sent, with Inbox gone from the top.                                                                           |
| `svg-not-in-host-build.png`                              | SVG cards with the text "react-native-svg is not in this host build". Expected, since SVG was OFF in this build.                                                     |
| `stock-desktop.png`                                      | Stock QNX desktop ("QNX Everywhere") after the app was closed. The start-of-run and end-of-run desktop captures are byte-identical (same md5), so one copy is kept.  |

The compositor can show a frame one input event late, so captures followed a pointer nudge.

## Log lines (`app.txt`)

```
RNQNX_WORKLETS_SHUTDOWN_OK framesRequested=0 framesDelivered=0 uiJobs=0 jsPosts=0
RNQNX_PLAYGROUND_SCREENSHOT /tmp/pr149-shot.png 1024x768
RNQNX_JS_LOG [gui] IMAGE-PROOF load {"count":1,"view":20,"sourceWidth":1024,"sourceHeight":1024,"cache":"none"}
RNQNX_JS_LOG [gui] reanimated UI frames: 6534, shared x: 219.98725512191317, runtime kind: 1
RNQNX_JS_LOG [gui] JS thread role: gui
RNQNX_JS_LOG [gui] worklet thread role: ui-worklet
RNQNX_JS_LOG [gui] frames 30 in 459 ms, worst gap 23 ms, role ui-worklet, stats {"requested":11162,"delivered":11161,"signalled":12672,"uiJobs":0}
RNQNX_JS_LOG [gui] JS busy 300 ms: 0 of 40 UI worklet jobs ran inside the busy window
RNQNX_JS_LOG [gui] PROOF-COMPLETE
```

The worklets proof ran on a `ui-worklet` thread separate from the `gui` JS thread, and the UI frame loop kept delivering frames (30 frames in 459 ms, worst gap 23 ms). The line `0 of 40 UI worklet jobs ran inside the busy window` means that during the 300 ms JS block none of the 40 queued UI worklet jobs ran. This run therefore does not show UI-thread worklets executing while JS is blocked.

## Not verified

- Pinch, rotation and any multi-pointer gesture. The injector sends one pointer.
- A gesture that keeps running while the JS thread is blocked (see the 0 of 40 line above).
- 1920x1080. The display was 1024x768 (the monitor's EDID was unreadable and the owner is fixing that separately).
- A real keyboard. All input came from `rnqnx-inject*` through Screen, not a physical keyboard or mouse.
- The injector commands and exit statuses for this run (not retained).
- SVG rendering, which was OFF in this build.
- Video and Audio cards. They show "QtVideo ... is not in this build (RNQNX_ENABLE_QT_VIDEO=OFF)".
- Expo Image GIF and WebP frames, modal and drawer Escape, held key: run 2 and the key-hold folder cover them.
