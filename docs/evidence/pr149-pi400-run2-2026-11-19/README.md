# PR 149 on the Pi 400, run 2 (2026-11-19)

Build 2 of PR #149 (`feat/worklets-reanimated-gesture-handler-qnx`), run by hand over SSH between 07:37 and 07:52 UTC.
Worklets, Reanimated, Gesture Handler, Qt SVG and RNSVG were ON.

## Host and build

- Host: `qnxpi`, `QNX qnxpi 8.0.0 2026/06/05-16:21:14EDT RaspberryPi400 aarch64le`. This is the custom Raspberry Pi 400 QNX image, not a stock Pi 4. Display 1024x768 (EDID unreadable, owner fixing separately). Address `192.168.2.2`, user `qnxuser`.
- Commit built: `240dc0814dd129e806e5228d1b70b9cc27dbd2d6`. Cross-built in `qnx805-build` with `ninja -j2`, exit 0, 723/723 steps (`build-receipt.md`).
- Deployed to `/data/home/qnxuser/rnqnx-pr149-2`. Qt runtime `/data/home/qnxuser/qt612-public-o2`. `run-app.sh` is byte-identical to the one in `rnqnx-pr149-1`.
- Executable `org.rnqnx.showcase`: sha256 `315cb1c34a600b4d48810476c4acec5b87fcea21e02243efc1f549ee6a303143`, 14056088 bytes. `app.bundle.js` sha256 `358527886692ba802744dda0c6339b4713f5fde11c8ccbb270152596600204a5`.
- `SHA256SUMS` lists all 21 deployed files. `pi-hash-verify.txt` is the hash listing taken on the Pi. Both have 21 entries, and `diff` of the two sorted lists printed nothing when I compared them for this README.
- Deploy commands (in `run.txt`): `cp=0 mk=0 scp1=0 scp2=0 scp3=0`. One harmless line, `ls: /data/home/qnxuser/qt612-public-o2/plugins/imageformats: No such file or directory`, came from a directory listing and did not stop the deploy.

## Commands

Injector: `sudo -n /data/home/qnxuser/bin/rnqnx-inject4 <args>`. Capture: `sudo -n screenshot -file=/tmp/pr149b-<n>.bmp`, then `scp` and conversion to PNG on the Mac. Launch: `cd /data/home/qnxuser/rnqnx-pr149-2 && sh ./run-app.sh`. Stop: `sh ./run-app.sh --stop-running`.

`run.txt` holds every injector command and its status: 62 injector calls, each followed by `inject_exit=0`; the log has no non-zero `inject_exit`. Key ones:

| Check            | Injector arguments                                                                                       | Exit |
| ---------------- | -------------------------------------------------------------------------------------------------------- | ---- |
| Modal Escape     | `key esc sleep 800 move 600 300`                                                                         | 0    |
| Drawer Escape    | `key esc sleep 900 move 600 300`                                                                         | 0    |
| Held down, 2.5 s | `click 800 350 sleep 400 move 805 352`, then `keydown down sleep 2500 keyup down sleep 500 move 600 300` | 0    |
| Held down, 4 s   | `keydown down sleep 4000 keyup down sleep 500 move 600 300`                                              | 0    |
| Down x3          | `key down sleep 150 key down sleep 150 key down sleep 500 move 600 300`                                  | 0    |
| Up x4            | `key up sleep 150 key up sleep 150 key up sleep 150 key up sleep 300 move 610 300`                       | 0    |

Stop: `stop_exit=0`. The app ignored the quit request, so `run-app.sh` sent SIGTERM (`RNQNX_SINGLE_INSTANCE_WARNING pid 26193952 ignored quit; sending SIGTERM`) and the process ended (`RNQNX_SINGLE_INSTANCE stopped org.rnqnx.showcase`). A `pidin | grep` check afterwards matched nothing (`grep=1`), and `rnqnx-winmgr` and `demolauncher` were running.

## Screenshots

Each capture followed a pointer nudge, because the compositor can show a frame one input event late.

| File                                                                                                                                                                      | Shows                                                                                                                                                               |
| ------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `stock-desktop-before.png`, `stock-desktop-after.png`                                                                                                                     | Stock QNX desktop before launch and after the app was stopped.                                                                                                      |
| `gallery-first-screen.png`                                                                                                                                                | Gallery after launch.                                                                                                                                               |
| `modal-open.png`, `modal-after-escape.png`                                                                                                                                | Modal open, then closed after `key esc`. No click was sent in between.                                                                                              |
| `drawer-open.png`, `drawer-after-escape.png`                                                                                                                              | Drawer open, then closed after `key esc`.                                                                                                                           |
| `held-down-0-before.png` to `held-down-4-after-four-key-up.png`                                                                                                           | Focus ring on the sidebar tabs: none; Advanced after the 2.5 s hold; Documents after the 4 s hold; Components after three `key down`; Advanced after four `key up`. |
| `expo-image-gif-frame-a/b/c.png`                                                                                                                                          | Animated GIF chip: frames yellow, green, blue, taken about 1 s apart.                                                                                               |
| `expo-image-webp-frame-a/b/c.png`                                                                                                                                         | Animated WebP chip: red with a white bar, then green with a white bar.                                                                                              |
| `expo-image-resize-anim-a/b/c.png`                                                                                                                                        | Expo Image resize card with `startAnimating`: red, yellow, then green.                                                                                              |
| `reanimated-spring-mid.png`, `reanimated-spring-end.png`, `reanimated-end-state.png`                                                                                      | withSpring: box mid-travel ("spring running"), box and bar at the end ("spring finished"), and the end state with the UI frame count.                               |
| `gesture-pan-release.png`                                                                                                                                                 | Gesture Handler card after a drag: "released at 266, 85, springing back".                                                                                           |
| `gesture-double-tap.png`                                                                                                                                                  | Card after a double click: status "double tap".                                                                                                                     |
| `svg-image-card.png`, `svg-filters.png`, `svg-text-layout.png`, `svg-fill-rules.png`, `svg-pattern-symbol-marker.png`, `svg-text-and-touch.png`, `svg-gradients-clip.png` | RNSVG cards rendering: image hrefs, filters, text layout, fill rules, patterns and markers, gradients and clips.                                                    |
| `svg-touch-before.png`, `svg-touch-circle-pressed.png`, `svg-touch-circle-group-pressed.png`                                                                              | "Touch targets and updates" card: counters 0 and 0, then Circle 1 and Group 1 after presses (the third capture shows rotation 318 deg).                             |
| `svg-video-card-qtvideo-absent.png`                                                                                                                                       | Video card reading "QtVideo is not in this build"; the Audio card says the same.                                                                                    |

## Log lines (`app.txt`)

```
RNQNX_JS_LOG [gui] IMAGE-PROOF load {"count":1,"view":20,"sourceWidth":1024,"sourceHeight":1024,"cache":"none"}
RNQNX_JS_LOG [gui] IMAGE-PROOF startAnimating dispatched
RNQNX_JS_LOG [gui] reanimated UI frames: 27204, shared x: 220, runtime kind: 1
[RemoteImageCard] loaded https://images.unsplash.com/photo-1506744038136-46273834b3fb?w=640&q=80 in 154149 ms
[RemoteImageCard] loaded https://images.unsplash.com/photo-1506744038136-46273834b3fb?w=640&q=80 in 618203 ms
```

The app log has no ImageLoader lines and no line for the modal or drawer close, so those two results rest on the screenshots.

## Held `down`

`keydown down sleep 2500 keyup down` sends one key-down and one key-up. The page did not scroll, and focus moved one control per run (Advanced, then Documents). Three separate `key down` presses moved it on to Components, and four `key up` presses to Advanced, which fits a five-tab ring that wraps. This run cannot show OS key repeat, because `rnqnx-inject4` never sets `SCREEN_FLAG_KEY_REPEAT`. The held-key test with a repeating injector is in `../pr149-pi400-keyhold-2026-11-19/`.

## Not verified

- Pinch, rotation and any multi-pointer gesture.
- A gesture that keeps running while the JS thread is blocked. Run 1 printed `0 of 40 UI worklet jobs ran inside the busy window`, and this run did not repeat the worklets proof.
- 1920x1080. The display was 1024x768.
- A real keyboard held down. Run 2 sent single key-down events.
- Why `RemoteImageCard` reported load times of 154149 ms and 618203 ms. App logs from the 2026-11-13 runs on this board (not kept in this folder) show 677 to 721 ms for the same URL. The key-hold run on 2026-11-19 logged 30905 ms. The cause of the spread is unknown.
- Whether `onRequestClose` fired on the Modal Escape (no log line).
- Video and Audio. The cards say QtVideo is not in this build (`RNQNX_ENABLE_QT_VIDEO=OFF`).
- ImageLoader behaviour. The app log has none of its lines.
