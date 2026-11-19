# Component gallery layout on the Pi 400, 2026-11-13

Evidence status, reviewed 2026-11-14: these are historical screenshot artifacts
and the original session's observations. This folder does not record the host
identity, exact deployed commit/bundle hash, full commands, or command exit
statuses. It is therefore incomplete device-run evidence and cannot qualify
the current PR head. A fresh bounded run must record those fields before any
current-candidate device gate is marked passed; no run was reconstructed or
performed during this review.

Build: feat/expo-image-painted-view (merged with main #157), release consumer
app org.rnqnx.showcase, originally reported as deployed to
/data/home/qnxuser/rnqnx-tile-1. The exact revision is unrecorded.

Window sizes captured with the app's `--window-size WxH --screenshot` flag:

- window-586x600.png: tab bar at the bottom, cards fill the page width, rounded corners on both sides.
- window-880x600.png: 96 px tab rail on the left, one column of cards that ends with its right-side rounded corners inside the window.
- window-1280x800.png: rail plus a two-column grid.
- live-resized-1180-wide-desktop.png: the running window on the desktop after an edge drag made it 1180 px wide (from 1280 px). The grid re-flowed to the new width and the right-hand cards keep their rounded corners. Input came from rnqnx-inject (PR #158) with a local `drag` subcommand; the compositor shows the new frame one input event late, which is why the capture follows a pointer move.

Single instance: `run-app.sh --replace` stops the running app over /tmp/rnqnx-org.rnqnx.showcase,
which answered `replacing pid 176304161 (release)`. A second `run-app.sh` restored the
running window and left one process.

Animated formats: animated-gif-webp-expo-image.png shows the gallery's expo-image card on the Pi.
Top row: Animated GIF at two moments (blue frame with a white bar, then a red frame). Bottom row: Animated WebP at two moments (the white bar has moved). Both decode through the qtimageformats plugins
(libqwebp.so and libqgif.so) in the app's plugins/imageformats directory; the sources are
apps/component-gallery/assets/anim.gif and anim.webp.

Live resize with the merged injector (tools/qnx-input, `drag`), sizes read from the app log
(`RNQNX_WINDOW_FRAME`): 1280x800 to 1180x800, to 589x800 (live-resized-589-wide-desktop.png: bottom
tab bar, cards fill the width, right corners rounded), back to 1184x800
(live-resized-back-wide-desktop.png: rail and two columns). The green button toggled maximise and
restore twice (two `RNQNX_WINDOW_ACTION maximize` lines, both captures taken).

Keyboard and pressed-state screenshots (the original notes name injector
arguments `key f01b` and `down`, but no complete command/exit receipt or exact
build identity was retained):

- modal-escape-before-after.png: the gallery's Modal appears open on the left and closed on the right. The notes attribute the change to `key f01b`; the image alone does not prove the input dispatch or `onRequestClose` callback.
- drawer-escape-before-after.png: the drawer appears open on the left and closed on the right. The same input attribution lacks a command receipt.
- tab-pressed-advanced.png: the Advanced tab shows a pressed wash and shrunken content. The original notes attribute it to a held `down`; its command status is unknown.

## What exists and what is missing for the three input claims

Searched on 2026-11-19 at head `12cae67`: `git grep` for `f01b`, every `rnqnx-inject`
mention under `docs/`, and the history of this folder (commits `b895bc5` to `f49e61e`).
The only record of these runs is the prose added in `d67b59b` (2026-11-13 15:53 -0500)
and the three screenshots. No log, transcript, receipt or exit status for any of them
is in the repository. They are not reconstructed here.

Independently checkable: `tools/qnx-input/rnqnx-inject.c` line 118 maps `esc` to `0xf01b`, so
`key f01b` is the Escape key, and `docs/handoff/HANDOFF_2026-11-13.md` lists `down` as an
injector command.

Missing for each of modal Escape, drawer Escape and the held `down` on the Advanced tab:

| Field             | Needed                                                                             |
| ----------------- | ---------------------------------------------------------------------------------- |
| Host              | Machine that ran the injector and capture (name, OS)                               |
| Target            | Pi 400 image identifier and the address used                                       |
| Build             | Deployed git SHA and `sha256` of the deployed executable and `app.bundle.js`       |
| Injector commands | Full command lines, in order, including the settle delay before each capture       |
| Capture commands  | Full command lines for the before and after frames                                 |
| Exit statuses     | `$?` of every injector and capture command                                         |
| Observed result   | Frame names, plus the app-log line for the Modal `onRequestClose` and drawer close |

A fresh run that records those fields next to the screenshots closes this finding. Until
then the three screenshots stay historical, as the status note at the top says.
