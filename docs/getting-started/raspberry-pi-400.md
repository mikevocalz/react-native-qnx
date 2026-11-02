# Raspberry Pi 400 — QNX 8 bring-up

The Raspberry Pi 400 is an **experimental physical target** for this repository.

QNX has published a working QNX 8 image recipe for the Pi 400 by adapting the
Raspberry Pi 4 Custom Target Image (CTI). The Pi 400 uses the BCM2711 SoC, but it
needs a Pi-400-specific device tree and Wi-Fi firmware.

This is different from the repository's current stock-QSTI baseline:

- **Pi 4 / Pi 5:** current QNX Everywhere QSTI 0.5.0 build 50 + SDP 8.0.5
- **Pi 400:** QNX-published custom-image adaptation; physical
  `react-native-qnx` validation is still pending

The QNX Pi 400 article is based on the Pi 4 CTI release `CTI-8.0.2`. Do not
describe a Pi 400 run as equivalent to the repository's SDP 8.0.5/QSTI build-50
baseline unless that exact combination has been rebuilt and proven on hardware.

Official references:

- QNX Pi 400 recipe:
  https://qnx.software/en/blog/2026/qnx-on-pi-400
- QNX Raspberry Pi QSTI guide:
  https://www.qnx.com/developers/docs/qnxeverywhere/com.qnx.doc.target_images/topic/qsti/intro.html
- QNX Raspberry Pi 4 BSP:
  https://www.qnx.com/developers/docs/BSP8.0/com.qnx.doc.bsp_raspberrypi.bcm2711.rpi4_8.0/topic/common/download.html

## Hardware

For a first bring-up, have:

- Raspberry Pi 400
- quality microSD card
- USB-C power supply suitable for the Pi 400
- micro-HDMI cable and display
- Ethernet cable if possible for the first networking pass
- mouse
- an x86-64 development host for QNX SDP: Windows 11, Ubuntu 22.04/24.04, or RHEL 9

The Pi 400 has 4 GB RAM. QNX's current general QSTI documentation recommends
8 GB or more for Pi 4/5, so the Pi 400 is not being promoted as the repository's
preferred performance target. It is still useful for physical AArch64 + Screen
bring-up.

## Development host: use the spare Windows PC first

For this Pi 400 bring-up, a spare x86-64 Windows PC is useful because the current
QNX SDP 8.0.5 host baseline supports Windows 11 x64, Ubuntu Desktop 22.04/24.04,
and RHEL 9. QNX SDP 8.0.5 does not support macOS hosts or ARM-based development
hosts.

That means the recommended lab layout is:

```text
Mac
  -> editing, Git/GitHub, notes, serial/log viewing, and orchestration

Old x86-64 PC
  -> QNX SDP 8.0.5 build/debug host
  -> Ethernet/USB-UART connection to the Pi 400

Raspberry Pi 400
  -> QNX target
```

Before installing QNX on the PC, record the Windows version and architecture:

```bat
winver
systeminfo | findstr /B /C:"OS Name" /C:"OS Version" /C:"System Type"
```

Host decision:

- **Windows 11 x64:** preferred Windows path for the current SDP 8.0.5 baseline.
- **Windows 10 x64:** legacy/deprecated host. Do not treat it as the repository's
  current validated baseline. Prefer upgrading the PC to Windows 11 if supported,
  or use Ubuntu 24.04 x86-64 on the same hardware.
- **Windows 7/8/8.1 or 32-bit Windows:** unsupported for this QNX 8 lane. Install
  Ubuntu 24.04 x86-64 on the PC instead.
- **PC cannot run a supported 64-bit OS:** do not use it as the QNX build host.

If the old PC cannot run Windows 11, the preferred recovery path is to install
Ubuntu 24.04 x86-64 on a spare SSD or use dual boot. This preserves the machine
as a dedicated QNX build/debug station and avoids relying on an unsupported
macOS or ARM-host toolchain.

The repository's current helper scripts are POSIX-shell-oriented. On a native
Windows QNX installation, validate the QNX toolchain first and use the Windows
environment supplied by QNX; do not assume the bash helpers are automatically
equivalent. Ubuntu x86-64 is the lowest-friction path for running the repository
automation unchanged.

For first hardware bring-up, connect the old PC to the Pi 400 over Ethernet and,
when available, a 3.3 V USB-to-TTL UART adapter. The Mac can remain on the same
LAN for source control and log review while the x86-64 host performs QNX builds.

## 1. Build the stock Pi 4 CTI once

Follow QNX's Pi 400 article and clone the Raspberry Pi 4 QNX 8 CTI it names:

```bash
git clone --branch release/CTI-8.0.2 \
  https://gitlab.com/qnx/custom-target-image-builds/raspberry-pi-4-qnx-8.0.git
cd raspberry-pi-4-qnx-8.0
```

Before changing anything, complete one successful stock Pi 4 image build using
the CTI repository's README. QNX specifically calls this out because the first
build prepares the `qnx800` tree used later in the Pi 400 conversion.

## 2. Switch the device tree to Pi 400

In `boot/Makefile`, replace:

```text
bcm2711-rpi-4-b.dtb
```

with:

```text
bcm2711-rpi-400.dtb
```

QNX's recipe downloads the Pi 400 DTB from Raspberry Pi firmware:

```text
https://github.com/raspberrypi/firmware/raw/refs/heads/master/boot/bcm2711-rpi-400.dtb
```

## 3. Switch the Wi-Fi firmware

In `boot/wifi/Makefile`, make these replacements:

```text
cyfmac43455-sdio.bin
  -> brcmfmac43456-sdio.bin

cyfmac43455-sdio.clm_blob
  -> brcmfmac43456-sdio.clm_blob

brcmfmac43455-sdio.raspberrypi,4-model-b.txt
  -> brcmfmac43456-sdio.raspberrypi,400.txt
```

The QNX recipe uses these firmware sources:

```text
https://github.com/armbian/firmware/raw/refs/heads/master/brcm/brcmfmac43456-sdio.bin
https://github.com/armbian/firmware/raw/refs/heads/master/brcm/brcmfmac43456-sdio.clm_blob
https://github.com/armbian/firmware/raw/refs/heads/master/brcm/brcmfmac43456-sdio.raspberrypi,400.txt
```

## 4. Override the generated Raspberry Pi snippets

After the successful stock CTI build, copy these generated snippets:

```text
qnx800/host/common/mkqnximage/extras/rasppi/rpi/snippets/boot_files.99.type_rasppi
qnx800/host/common/mkqnximage/extras/rasppi/rpi/snippets/system_files.type_rasppi
```

into the CTI repository's top-level `snippets/` directory.

In `snippets/boot_files.99.type_rasppi`, replace every
`bcm2711-rpi-4-b.dtb` reference with `bcm2711-rpi-400.dtb`.

In `snippets/system_files.type_rasppi`, change the Wi-Fi file entries to:

```text
[perms=444] etc/wifi/firmware/brcmfmac43456-sdio.bin=brcmfmac43456-sdio.bin
[perms=444] etc/wifi/firmware/brcmfmac43456-sdio.clm_blob=brcmfmac43456-sdio.clm_blob
[perms=444] etc/wifi/firmware/brcmfmac43456-sdio.raspberrypi,400.txt=brcmfmac43456-sdio.raspberrypi,400.txt
```

and change the Wi-Fi startup arguments to:

```text
fw=/system/etc/wifi/firmware/brcmfmac43456-sdio.bin
nvram=/system/etc/wifi/firmware/brcmfmac43456-sdio.raspberrypi,400.txt
clm_blob=/system/etc/wifi/firmware/brcmfmac43456-sdio.clm_blob
```

**Keep the original Pi 4 DTB and Wi-Fi files in the build tree.** QNX notes that
the current image tooling still checks for those files even though the Pi 400
image does not use them.

## 5. Build, flash, and boot

Run the CTI build again. QNX's recipe still emits `rpi4.img` unless you also
rename the image in the CTI Makefiles.

Flash that image to the microSD card.

For the first graphical boot, use the Pi 400 micro-HDMI port **closest to the
microSD-card side** of the keyboard chassis. That is the port QNX calls out in
its Pi 400 instructions.

A successful bring-up should reach the QNX graphical launcher.

## 6. Configure networking

Ethernet is the least-variable first path.

For Wi-Fi, QNX's Pi 400 recipe has you edit:

```text
/boot/wpa_supplicant.conf
```

then reboot.

Do not commit Wi-Fi credentials to this repository or to captured evidence.

## 7. Run the react-native-qnx physical-target lane

Once the Pi 400 is reachable over SSH and QNX Screen is running, the repository
does not require a Pi-400-specific application binary. The target is still
AArch64le and uses the same QNX Screen deployment lane.

On the development host:

```bash
export QNX_SDP_ENV=/path/to/qnxsdp-env.sh
export RNQNX_PI_BOARD=pi400

# Override these only if your custom image differs from the stock defaults:
export QNX_TARGET_IP=qnxpi.local
export QNX_TARGET_USER=qnxuser

pnpm pi:preflight
pnpm pi:install
```

The preflight must prove:

- QNX AArch64le toolchain is available on the host
- target reports `aarch64le`
- `/dev/screen` exists
- deployment directory is writable
- SSH/SCP work

`pnpm pi:install` records `RNQNX_PI_BOARD` in the evidence bundle so a Pi 400
run cannot accidentally be confused with a Pi 4/5 run.

After the Screen lane is green:

```bash
pnpm qnx:runtime:validate
```

That is the next gate for the Hermes + Nitro + Expo Modules QNX runtime.

## What counts as Pi 400 support

There are three separate claims; keep them separate:

1. **QNX boots on Pi 400** — covered by QNX's published custom-image recipe.
2. **react-native-qnx native Screen lane runs on Pi 400** — pending a captured
   `pnpm pi:install` hardware run.
3. **full React Native + Hermes + Qt Quick runtime runs on Pi 400** — later
   milestone after the native Screen and runtime probes pass.

Until #2 is captured, Pi 400 support remains experimental in this repository.

## Troubleshooting

### Black screen or no launcher

Confirm the correct Pi 400 DTB is in the image and use the HDMI port closest to
the microSD-card side. If possible, boot with the display connected from power-on.

### Wi-Fi does not start

Re-check all three `brcmfmac43456` filenames in both the Wi-Fi Makefile and
the copied system snippet. Use Ethernet to separate networking problems from
the QNX/Screen bring-up.

### `pnpm pi:preflight` says the target is not AArch64le

Stop there. Do not deploy. Capture `uname -a` and `uname -m` from the target
and verify the image actually booted the expected QNX AArch64 configuration.

### `/dev/screen` is missing

The React Native QNX physical lane requires QNX Screen. Fix the custom image's
graphics/Screen startup before debugging React Native, Hermes, Nitro, or Expo.
