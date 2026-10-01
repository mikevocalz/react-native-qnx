# Raspberry Pi 4/5 — same-day QNX install lane

This is the shortest supported path from this repository to a physical Raspberry Pi
running QNX Screen.

## Known-good target baseline for October 2026

Use the current QNX Everywhere Quick Start Target Image (QSTI):

| Board          | QNX Software Center package    | QSTI version              | Build |
| -------------- | ------------------------------ | ------------------------- | ----- |
| Raspberry Pi 4 | com.qnx.qnx800.quickstart.rpi4 | 0.5.0.00050T202609021847L | 50    |
| Raspberry Pi 5 | com.qnx.qnx800.quickstart.rpi5 | 0.5.0.00050T202609021847L | 50    |

That QSTI line supports QNX SDP 8.0.5. QNX recommends an 8 GB or larger Pi 4 or
Pi 5 for the current image.

Sources:

- QNX Everywhere release notes: https://qnx.com/developers/docs/qnxeverywhere/rel_notes.html
- QSTI Raspberry Pi guide: https://qnx.com/developers/docs/qnxeverywhere/com.qnx.doc.target_images/topic/qsti/intro.html
- QSTI install guide: https://qnx.com/developers/docs/qnxeverywhere/com.qnx.doc.target_images/topic/qsti/install.html

## 1. Flash and boot the QSTI

Install QNX SDP 8.0.5 and the matching Pi 4 or Pi 5 QSTI package with QNX
Software Center, then flash its IMG file with Raspberry Pi Imager.

For the first graphical boot, power on and connect the HDMI display before
starting the Pi. QNX has documented Raspberry Pi cases where booting headless
can leave Screen at an unexpected resolution.

The default QSTI hostname is qnxpi, advertised over mDNS as qnxpi.local.

## 2. First login

The current QSTI uses qnxuser as the normal account. The initial terminal
credentials are:

    username: qnxuser
    password: qnxuser

Change the password immediately on a device that will stay on your network.

QNX documents root/root as the initial root credentials too, but root SSH login
is disabled by default. This repository does not require root SSH and defaults
to qnxuser.

Verify the board from your host:

    ssh -o MACs=hmac-sha2-256 qnxuser@qnxpi.local

On Windows, QNX explicitly requires hmac-sha2-256 for the stock image. The
repository target helper applies that MAC setting for SSH and SCP on every host,
so the install scripts behave consistently.

Official interaction/SSH guide:
https://qnx.com/developers/docs/qnxeverywhere/com.qnx.doc.target_images/topic/qsti/interacting-with-the-system.html

## 3. Activate the QNX build environment

Source the licensed SDP environment in the shell used to build:

    source /path/to/qnxsdp-env.sh

Or point the installer at it:

    export QNX_SDP_ENV=/path/to/qnxsdp-env.sh

The installer also checks the common QNX 8 installation locations used by Linux
and Git Bash.

## 4. Run the preflight

From the repository root:

    ./scripts/pi-preflight.sh

Defaults:

    QNX_TARGET_IP=qnxpi.local
    QNX_TARGET_USER=qnxuser
    QNX_TARGET_DIR=/data/home/qnxuser/rnqnx

Override only what is different on your target:

    export QNX_TARGET_IP=192.168.1.50
    export QNX_TARGET_USER=qnxuser
    export QNX_SSH_KEY=$HOME/.ssh/id_ed25519

The preflight fails early unless all of these are true:

- the host can see qcc/q++ and their AArch64le target
- the target is reachable over SSH
- uname -m reports aarch64le
- /dev/screen is present
- the target deployment directory is writable

It also records whether screenshot, gles2-gears, egl-configs, and slog2info are
available.

## 5. Build, deploy, run, and capture evidence

Run:

    ./scripts/install-pi.sh

or, if pnpm is already available:

    pnpm pi:install

The default is an AArch64 Release build. The installer:

1. runs the Pi preflight
2. configures the QNX AArch64 preset
3. builds rnqnx_playground
4. uploads through a temporary filename and atomically moves it into place
5. runs it against QNX Screen
6. captures stdout
7. captures a Screen BMP while the app is visible when screenshot is available
8. captures a recent slog2 snapshot
9. stores the complete evidence set under artifacts/pi/<UTC timestamp>/

For a Debug pass:

    RNQNX_PI_CONFIG=Debug ./scripts/install-pi.sh

For a quick re-deploy of an already-built binary:

    RNQNX_SKIP_BUILD=1 ./scripts/install-pi.sh

## What success means today

A green install proves the physical-board lane:

host QNX AArch64 toolchain -> binary -> SSH/SCP -> QNX loader -> Screen context ->
double-buffered window -> presentation loop -> clean shutdown.

It does **not** yet mean the complete React Native + Hermes + Qt Quick runtime is
running on the Pi. The repository's full React/Fabric/Qt renderer is currently
host-validated; QNX target integration for that stack is the next milestone.

That distinction is intentional. The same-day lane gives us a real hardware
baseline first, with evidence, so renderer/Hermes work can be debugged against a
known-good QNX Screen/device path instead of mixing toolchain, SSH, graphics,
Hermes, and Fabric failures together.

## If the preflight fails

### qnxpi.local does not resolve

Use the IP address shown on the QNX Welcome screen:

    export QNX_TARGET_IP=<pi-ip-address>

### SSH negotiates no compatible MAC

Do not disable SSH integrity checks. The repository already requests
hmac-sha2-256, matching the current QSTI documentation.

### /dev/screen is missing

Confirm the QNX Welcome screen is running and Screen finished startup. The
current QSTI boots to a graphical welcome screen when graphics are healthy.

### Wrong architecture

The Pi target must report aarch64le. Do not deploy an x86_64 QEMU binary to the
Pi.

### Screenshot is missing

The app can still validate its Screen lifecycle from stdout. Install/restore the
QNX Screen utility package before requiring visual evidence.

### Pi 5 cannot join Wi-Fi when several Pi 5 boards are present

The September 2026 QNX release notes document QNXE-2016: multiple Pi 5 targets
may share a Wi-Fi MAC. Assign unique MAC addresses as described by QNX, or use
Ethernet for the validation session.
