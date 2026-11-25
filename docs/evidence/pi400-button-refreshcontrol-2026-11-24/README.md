# Pi 400 re-run: Button override and pull-to-refresh, 2026-11-24

Both checks ran on the **Pi 400** (`qnxuser@192.168.2.2`, QNX 8.0.0 `RaspberryPi400 aarch64le`), the board for
[ADR-0023](../../adr/0023-pi400-stands-in-for-device-parity.md). Nothing here ran on a stock Pi 4 QSTI.

Why: [pi400-v1-gates-2026-11-24](../pi400-v1-gates-2026-11-24/README.md) found that `Button` drew black text on this target and that
`RefreshControl` fired no `onRefresh`. PR #233 added the `Button` override and PR #235 added the QNX `ScrollView` pull detection. Both
PRs say they were not re-run on the board. This run does that.

## What ran

- Source: `main` at `eaf33754` (includes #233 and #235). Fresh clone, `pnpm install --frozen-lockfile`.
- Bundle: `apps/rn-playground/component-device-probe.js`, `metro build --config metro.public.config.js --platform qnx --dev false`, then
  `hermesc` 260318099.0.5 `-O -emit-binary`. SHA-256 in [board/bundle-sha256.txt](board/bundle-sha256.txt).
- App: the playground binary already on the board from the earlier receipt (`279e300f...`, `/data/home/qnxuser/v1g2/playground`). Both
  fixes are JavaScript overrides, so only the bundle changed. No native rebuild was made (the build container from that run no longer exists).
- Launch: by hand over ssh with `v1g2-run.sh playground org.rnqnx.playground --bundle .../component-probe-pr233-235.hbc`. Input came from
  `rnqnx-inject5` through Screen. Screenshots came from `sudo -n screenshot`. Each run was stopped with `v1g2-stop.sh` (SIGTERM).
- `/dev/io-hid/io-hid` existed, so USB was not started. `drm-rpi4`, `screen`, `rnqnx-winmgr` and `demolauncher` kept the same pids
  before and after. The board was not rebooted and no boot file was touched.

## Results

| Check                                                                                              | Result                                                                                     | Receipt                                                                                                                                                                                                                                                            |
| -------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| `Button` title drawn with the override                                                             | Pass. The title is blue on the dark page; the 2026-11-24 run drew black.                   | [frames/01-initial.png](frames/01-initial.png)                                                                                                                                                                                                                     |
| `Button` `onPress`                                                                                 | Pass, 2 of 2 clicks logged (`Button onPress`). Counter went from `Button 0` to `Button 2`. | [board/run1-drag-pull.txt](board/run1-drag-pull.txt), [frames/02-after-button-clicks.png](frames/02-after-button-clicks.png)                                                                                                                                       |
| `RefreshControl` `onRefresh` on a downward drag of 140 px from the top (`drag 700 420 700 560 20`) | Pass. One `RefreshControl onRefresh` in each of two runs.                                  | [board/run1-drag-pull.txt](board/run1-drag-pull.txt), [board/run2-short-drag-wheel-long-drag.txt](board/run2-short-drag-wheel-long-drag.txt), [frames/03-after-pull.png](frames/03-after-pull.png), [frames/04-run2-after-pull.png](frames/04-run2-after-pull.png) |
| Short drag (40 px, under the 72 px threshold)                                                      | Pass. No `onRefresh`.                                                                      | run 2 log                                                                                                                                                                                                                                                          |
| Wheel up 5 notches at the top                                                                      | No `onRefresh`. See the limit below.                                                       | run 2 log                                                                                                                                                                                                                                                          |

Run 2 order: short drag, wheel, long drag, from a fresh process. The counts of `onRefresh` after each step were 0, 0, 1.

## Limits

- **Wheel does not trigger a pull.** The probe page text says "wheel up at the top", and on the Pi 400 that does nothing. The #235 detection
  reads touch and scroll events. I did not trace why a wheel notch at the top does not reach it. The supported gesture is a
  drag, and the Pi 400 has no touch panel, so the drag was a pointer drag from `rnqnx-inject5`. The probe text is stale and was not edited here.
- **Run 1 wheel result is not usable.** In run 1 the wheel came after the drag, and the probe sets `refreshing` true and never clears it, so
  a second pull is disabled by design. Run 2 tested the wheel first.
- **No touch device and no physical keyboard.** Every pointer event came from the injector through Screen. This does not close the
  touch and keyboard part of the `device-parity` gate.
- **Not run:** the other components in the probe, the VList, Section and VSection pages, the `refreshing` visual state of `RefreshControl`, a physical mouse wheel, and any run under the stock `fullscreen-winmgr`.
- HDMI output was captured by the board's own `screenshot`, not by an external capture.

## Board state at the end

No playground process was running. `drm-rpi4` (696343), `screen` (737305), `rnqnx-winmgr` (786459) and `demolauncher` (1085462) were
running, and the stock desktop is shown in [frames/05-final-desktop.png](frames/05-final-desktop.png). The desktop shows a `QNX Components`
tile that this run did not create. The new bundle stays at `/data/home/qnxuser/v1g2/bundles/component-probe-pr233-235.hbc`. Temporary logs
in `/tmp` and screenshot bitmaps were removed.

No gate status in `docs/releases/v1-readiness.json` changed.
