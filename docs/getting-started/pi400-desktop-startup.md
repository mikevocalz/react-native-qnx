# Pi 400 standalone desktop startup candidate

This experimental board-specific controller starts the existing QNX desktop
without waiting for Ethernet. It does not launch a React Native app or change the
recovery image, firmware configuration or stock launcher files. It is not an
installer and is not enabled by adding it to this repository.

## Contract

`scripts/pi400-desktop-startup.sh` runs only when `uname -m` reports
`RaspberryPi400` and the existing compatibility prefix contains
`enable-rnqnx-autostart`. That legacy marker now selects the desktop, not an app.
Driver administration requires root; the launcher subprocess must be UID 1000
with groups 1000 and 36. The prefix, node names and user identity are specific to
this experimental Pi 400 image, not a stock Pi 4/5 QSTI installation.

The controller uses the existing `drm-rpi4` stack and HDMI2 Screen configuration.
It starts a service only when its required nodes or process are absent. Device
waits are bounded; input failure is logged but does not suppress display startup.
Both DRM nodes must exist before access is granted to `qnxuser`. The launcher
uses the owned user overlay or falls back to the unchanged stock registry. It
rejects symlinked overlay, owner and desktop-configuration paths.

An exclusive volatile file prevents concurrent invocations. Normal exit, INT and
TERM remove only that invocation's lock; an existing lock is never deleted.
A killed invocation can leave a stale lock within the current boot. An
administrator must inspect the running services before removing that lock and
retrying. The script does not kill existing graphics/input services on failure.
Logs go under the trusted compatibility prefix's private `desktop-logs`
directory. Installation must keep that prefix root-owned and non-writable by
the app user. The controller rejects a symlinked log directory and uses a private
umask. Retained logs are not proof of a fresh boot.

## Validation and installation boundary

Run `node tests/js/pi400-desktop-startup.test.mjs` on a host with Python 3 and a
POSIX shell. Its isolated fake utilities cover absent network, missing runtime
and device nodes, input failure, service failure, duplicate invocation, launcher
UID, registry fallback and signal cleanup. The fixtures never contact a target.
They do not establish QNX shell compatibility, physical cursor/display behavior
or cold-boot reliability.

Before persistent installation, obtain explicit owner approval, record the exact
board/image and current startup-hook hashes, preserve a root-owned rollback copy
and confirm a reachable recovery/console path. Stage and hash the candidate in
the existing compatibility prefix before atomically replacing only the approved
hook. Do not run a second controller alongside an active invocation. Keep the
previous hook and its configuration available for rollback. Do not edit the
recovery IFS, firmware/rainbow settings or original peripheral startup stage.

Reboot/power cycling requires separate explicit approval. Record every actual
attempt using the existing [qualification capture](release-evidence.md) contract;
20 app relaunches are not 20 supervised cold boots. A source fixture or a working
current-boot desktop cannot close the v1 cold-boot gate. Pi 400 observations remain
separate from stock Pi 4 QSTI evidence.
