# Component gallery layout on the Pi 400, 2026-11-13

Build: feat/expo-image-painted-view (merged with main #157), release consumer
app org.rnqnx.showcase, deployed to /data/home/qnxuser/rnqnx-tile-1.

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
