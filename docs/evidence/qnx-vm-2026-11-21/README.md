# QNX VM re-capture of five newly exported components, 2026-11-21

**This is QNX VM evidence (QEMU, TCG software execution, software render). It is
not Pi 400 evidence and it does not make any Pi 400 or Pi 4 QSTI row
device-verified.** No gate in `docs/releases/v1-readiness.json` changes status.
Cold-boot and device-parity stay pending.

The [2026-11-20 receipt](../qnx-vm-2026-11-20/README.md) found that five
components threw `"X" is not implemented in this experimental entry` when
imported from `react-native`. The public entry now exports them
(`packages/react-native-qnx/Libraries/ReactNative/PublicAPI.js`). This folder
holds the VM re-capture of the fixed entry.

## What ran

| Item               | Value                                                                                                                                                                                                                                               |
| ------------------ | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Board              | QEMU 11.1.3 `-machine virt,gic-version=3`, TCG, `-cpu cortex-a57`, 4 CPUs, 3 GiB RAM ([run.sh](../qnx-vm-2026-11-20/vm/run.sh), unchanged)                                                                                                          |
| Guest              | `QNX qnxvm 8.0.0 2026/06/05-16:21:14EDT QEMU_virt aarch64le` ([vm/chkqnx6fs-and-uname.txt](vm/chkqnx6fs-and-uname.txt))                                                                                                                             |
| Native app         | `org.rnqnx.showcase` from the 2026-11-20 deploy, not rebuilt. Its sha256 on the guest copy is `4f465406...f69cf`, equal to the line in [../qnx-vm-2026-11-20/vm/deploy.sha256](../qnx-vm-2026-11-20/vm/deploy.sha256).                              |
| JavaScript bundles | Built on the Mac from main at `19fa1b25` with `bundle-qnx --app component-gallery`, then `scripts/vm-proof/build-bundles.sh` and the `hermes-compiler@260318099.0.5` macOS `hermesc`. Hashes in [vm/proof-bundles.sha256](vm/proof-bundles.sha256). |
| Capture            | `/data/vm/capture.sh /data/vm/app /data/vm/bundles-2 /data/vm/shots-2 <card-id>...` (identical to `scripts/vm-proof/capture.sh`), output in [vm/capture-run.txt](vm/capture-run.txt)                                                                |
| Display            | QNX Screen, `screen-sw.so`, dummy WFD driver, 1280 x 800, 1280 x 800 PNG per capture                                                                                                                                                                |

The bundle set differs from the first run only in the JavaScript. The JS entry
is where the five exports live, so the native binary did not need a rebuild.

## Image state

The VM disk had been left unclean when the T9 dropped and a QEMU was killed. A
copy-on-write backup (`cp -c`, APFS) was taken before the first boot, and the
original and the backup had the same sha256 (`bc2fbd24...0d85`). The guest
booted and answered ssh. With `/system` and `/data` remounted read-only,
`chkqnx6fs -v` finished both partitions with no errors or warnings
([vm/chkqnx6fs-and-uname.txt](vm/chkqnx6fs-and-uname.txt)). Both were remounted
read-write afterward. Nothing was rebuilt: the ssh key and the deploy tree were
intact.

## Results

Each capture is a one-shot render of one card. Nothing was pressed, scrolled or
typed. Every card prints `Resolved: import { X } from 'react-native'`, which is
the public entry and not the deep-import fallback.

| Component                | Capture                                                               | Result                                                                                                          |
| ------------------------ | --------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------- |
| `VirtualizedList`        | [virtualized-list.png](captures/virtualized-list.png)                 | Public entry resolves. Items 1 to 5 of 200 visible.                                                             |
| `VirtualizedSectionList` | [virtualized-section-list.png](captures/virtualized-section-list.png) | Public entry resolves. Alpha section (a1 to a3) and the Beta header and row `b1` (clipped by the card) visible. |
| `Button`                 | [button.png](captures/button.png)                                     | Public entry resolves. Enabled "Press me" and disabled "Disabled" render; presses: 0.                           |
| `SafeAreaView`           | [safe-area-view.png](captures/safe-area-view.png)                     | Public entry resolves. The child box renders; no inset is visible.                                              |
| `ImageBackground`        | [image-background.png](captures/image-background.png)                 | Public entry resolves. The bundled image paints behind the child text.                                          |

All five runs exited 0. Stderr per card is in [logs/](logs/). It contains the
known `no EGL display ... continuing without OpenGL` and `QNX_PHYSICAL_SCREEN_SIZE`
warnings from the software path.

`docs/component-support.json` moves the `vm` field of these five from `failed`
to `scoped`. The `host`, `pi400` and `pi4Qsti` fields are unchanged.

## Not shown

No pointer or keyboard input, no scrolling, no press, no GPU rendering, no
timing (TCG on an M3 Pro), and nothing about the Pi 400's Screen driver or its
V3D path. The other seven cards from the first run were not re-captured.
