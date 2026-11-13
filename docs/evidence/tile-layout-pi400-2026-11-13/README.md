# Component gallery layout on the Pi 400, 2026-11-13

Build: feat/expo-image-painted-view (merged with main #157), release consumer
app org.rnqnx.showcase, deployed to /data/home/qnxuser/rnqnx-tile-1.

Window sizes captured with the app's `--window-size WxH --screenshot` flag:

- window-586x600.png: tab bar at the bottom, cards fill the page width, rounded corners on both sides.
- window-880x600.png: 96 px tab rail on the left, one column of cards that ends with its right-side rounded corners inside the window.
- window-1280x800.png: rail plus a two-column grid.
- live-resized-1180-wide-desktop.png: the running window on the desktop after an edge drag made it 1180 px wide (from 1280 px). The grid re-flowed to the new width and the right-hand cards keep their rounded corners. Input came from tools/qnx-input rnqnx-inject (pointer drag); the compositor shows the new frame one input event late, which is why the capture follows a pointer move.

Single instance: `run-app.sh --replace` stops the running app over /tmp/rnqnx-org.rnqnx.showcase,
which answered `replacing pid 176304161 (release)`. A second `run-app.sh` restored the
running window and left one process.
