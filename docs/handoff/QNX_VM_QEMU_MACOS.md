# QNX 8.0.5 aarch64 VM in QEMU on macOS

This records how the QEMU VM used for the 2026-11-20 captures was built and
booted. Output from it is VM evidence and never Pi 400 evidence. See the
[capture receipt](../evidence/qnx-vm-2026-11-20/README.md). The older
[Windows x86_64 VM note](QNX_VM_WINDOWS.md) used a different SDP and target.

The QNX SDP, its packages and the VM images are licensed. None of them goes in
git.

## Why aarch64le

The SDP in the container has only `target/qnx/aarch64le`; the x86_64 target
libraries were removed to save disk, so `targets/qemu` (x86_64) cannot build.
The Pi binaries are aarch64le as well. The QEMU `virt` board needs four extra
packages, installed with `qnxsoftwarecenter_clt -installPackage` (myQNX sign-in
required, never print the options file):

- `com.qnx.qnx800.target.qemuvirt` (the `startup-qemu-virt` binary)
- `com.qnx.qnx800.target.driver.virtio.devb`, `.devc` and `.startup`
- `com.qnx.qnx800.target.screen.board.null` (the dummy WFD driver)

`qsc -verifyInstallation` reported 3674 `MissingFile` errors before and after
(all `target/qnx/x86_64` paths). The catalogue has no virtio-gpu or virtio
Screen board for aarch64: the `screen.board.*` list is the TI, NXP, Xilinx,
Raspberry Pi, Intel and `vmwgfx` boards plus `null`, `common_drm` and
`common_mesa`.

## Build

In the container, as a normal user, with the public key from the Mac:

```sh
source /work/qnx800/qnxsdp-env.sh
mkqnximage --type=qemu --arch=aarch64le --graphics=no --ssh-ident=/tmp/qnxvm.pub \
  --hostname=qnxvm --data-size=3500 --data-inodes=200000 --sys-size=300 --noprompt --build
```

The command builds `boot.part`, `system.part`, `data.part` and `ifs.bin`, then
fails at the last step: `builddisk` asks for `target/qnx/x86_64/boot/sys/ipl-diskpc1-nomsg`
even for aarch64. [`scripts/vm-proof/mkdisk.py`](../../scripts/vm-proof/mkdisk.py)
joins the three partitions into `disk-qemu` with an MBR (types 177, 178, 179),
which is all the `virt` board needs because QEMU loads `ifs.bin` with `-kernel`.
Copy both files to a volume the VM can use (`docker cp`); the 3.5 GiB data
partition holds Qt and the app.

## Boot

```sh
qemu-system-aarch64 -machine virt,gic-version=3 -accel tcg -cpu cortex-a57 -smp 4 -m 3G \
  -kernel ifs.bin \
  -drive file=disk-qemu,format=raw,if=none,id=drv0 -device virtio-blk-device,drive=drv0 \
  -netdev user,id=net0,hostfwd=tcp:127.0.0.1:2222-:22 -device virtio-net-device,netdev=net0 \
  -object rng-random,filename=/dev/urandom,id=rng0 -device virtio-rng-device,rng=rng0 \
  -display none -serial file:serial.log
ssh -i id_ed25519 -p 2222 root@127.0.0.1 'uname -a; pidin'
```

Up in about 45 seconds. Findings from getting here:

- **HVF does not work.** HVF needs GICv3 (`HVF does not support GICv2
emulation`), and `startup-qemu-virt` with `-cpu host` then stops at
  `gic_v3.c:966 ASSERT((use_memory_mapped_callouts == 0) || (gicc_paddr_base != ~0))`.
  TCG boots with either GIC version. The run used GICv3.
- `scp` needs `-O` (legacy protocol); the image has `sftp-server` but the
  default SFTP mode failed in the earlier tries.

## Screen without a GPU

The image above has no Screen. The runs copied these from the SDP into
`/data/vm/screen` and started it by hand:

- `sbin/screen`, `libscreen`, `libWFD`, `libgbm`, `libEGL`, `libGLESv2`,
  `libswblit`, `libscrmem`, `libmemobj`, `libimg` and friends
- `lib/dll/screen-sw.so` and `screen-stdbuf.so`
- `libWFDdummy.so`, `libwfdcfg-dummy.so` and `graphics.conf` from
  `usr/lib/graphics/dummy`, with `video-mode = 1280 x 800 @ 60`

Four things were not obvious:

1. Screen `dlopen`s `screen-sw.so` from the directory of `graphics.conf`, so
   `screen-sw.so` and `screen-stdbuf.so` go there too. `LD_LIBRARY_PATH` did not
   help for that call.
2. `libswblit` loads `libyuv.so` by bare name. Copy it to `/system/lib`.
3. `libyuv.so` uses `log10` without listing `libm`. Start Screen with
   `LD_PRELOAD=libm.so.3`.
4. The Qt platform plugin needs the patch in
   [`qtbase-qnx-software-grab.patch`](../../scripts/qt-public/patches/qtbase-qnx-software-grab.patch):
   there is no EGL driver, and without `toImage()` the app's `--screenshot`
   writes nothing.

The Qt libraries go in `/data/vm/qt`, the app in `/data/vm/app`, DejaVu LGC
fonts in `/data/vm/fonts` (`QT_QPA_FONTDIR`), and
[`scripts/vm-proof/capture.sh`](../../scripts/vm-proof/capture.sh) runs the app
once per bundle.
