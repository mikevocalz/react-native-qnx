# QNX x86_64 VM on Windows (QEMU workaround)

This document records the workaround used to boot a QNX x86_64 VM on the
Windows x86-64 host using the installed QNX SDP 8.0.2 and QEMU 11.1.0. It is a
temporary working note; the official supported QNX build host remains x86-64
Ubuntu 24.04 per `QNX_VALIDATION_QUEUE.md`.

## What was achieved

- `mkqnximage` successfully builds a QNX x86_64 VM image on Windows.
- QEMU 11.1.0 boots the VM, mounts the QNX6 system/data partitions, starts
  `sshd`, and exposes port 22 over user-mode NAT on host port 2222.
- Root SSH is configured with an ED25519 key injected into
  `/data/home/root/.ssh/authorized_keys`.
- A repo binary (`rnqnx_g0_cxx20_probe`) was built with the
  `qnx-x86_64-debug` CMake preset, copied to the VM with `scp`, and executed
  over SSH, producing the expected output:

  ```text
  RNQNX_G0_CXX20_OK ranges=12 designated=21 thread=34 jthread=unsupported
  ```

## Prerequisites

- Windows x86-64 host.
- Licensed QNX SDP 8.0.2 installed at `C:\Users\Owner\qnx800`.
- QEMU 11.1.0 (or later) installed at `C:\Program Files\qemu`, with
  `qemu-system-x86_64.exe` in PATH or invoked by absolute path.
- Git Bash or the QNX-provided bash at
  `C:\Users\Owner\qnx800\host\win64\x86_64\usr\bin\bash.exe`.

## Known Windows-specific bugs

1. **License check fails unless `HOME` is set.** The QNX bash must see
   `HOME=/cygdrive/c/Users/Owner`; otherwise the Cygwin-style path conversion
   for `~/.qnx/license` fails and `mkqnximage` reports "license check failed".

2. **`__LIB*` placeholders are not resolved by `mkifs`/`mkqnx6fsimg`.**
   `mkqnximage` emits lines such as `lib/__LIBC__` and `usr/lib/__LIBCRYPTO__`
   into the generated build files, but on Windows these are not expanded to the
   versioned library files. The resulting IFS boots without `libc.so.6` and
   other essential libraries. The workaround is to replace the placeholders
   with concrete filenames and add the runtime soname symlinks before running
   `mkifs` and `mkqnx6fsimg`. The mapping is recorded in
   `qnx-vm-test/fix-and-rebuild.sh`.

3. **Disk boot sector hangs under QEMU 11.1.0.** The image built by
   `mkqnximage` boots the QNX boot loader, which then hangs. The workaround is
   to clear the MBR bootable flag on partition 1 and boot the IFS directly with
   QEMU's `-kernel output/ifs.bin`, using the raw disk image only for
   system/data partitions.

4. **`.vmdk` descriptor truncates the backing filename.** `copyimage` generates
   an extent name of `disk-` instead of `disk-qemu` on some runs, so the VM
   image must be attached as the raw `disk-qemu` file with `-hda`, not via the
   `.vmdk` descriptor.

5. **`rng-random` is not supported by the Windows QEMU build.** The stock
   `runimage` script passes `-object rng-random,filename=/dev/urandom`, which
   QEMU rejects. It is omitted in the manual QEMU invocation below.

## Reproduction scripts

The working scripts are stored in `C:\Users\Owner\qnx-vm-test`:

- `fix-and-rebuild.sh` — sets `HOME`, sources the SDP environment, substitutes
  the `__LIB*` placeholders in `output/build/ifs.build` and
  `output/build/system.build`, then runs `mkifs` and `mkqnx6fsimg`.
- `rebuild-disk.sh` — sets `HOST_UNAME=WINNT` and `MKFS_PATH` correctly, then
  runs the QNX `builddisk` script to regenerate `output/disk-qemu`.
- `runimage-fixed.sh` — not fully functional because of the `rng-random` issue
  and disk-boot hang, so the VM is launched with the manual QEMU command
  below instead.

Typical rebuild flow after the first `mkqnximage --build`:

```bash
# From Git Bash or the QNX bash, in qnx-vm-test/
bash fix-and-rebuild.sh   # fix placeholders and rebuild IFS/system/data
bash rebuild-disk.sh      # regenerate disk-qemu
```

## Booting the VM

Use the raw disk image and boot the IFS directly. The disk MBR bootable flag
must already be cleared (one-time; byte at offset 446 set to `0x00`).

```powershell
Start-Process -FilePath 'C:\Program Files\qemu\qemu-system-x86_64.exe' `
  -ArgumentList @(
    '-smp', '2',
    '--cpu', 'max',
    '-m', '1G',
    '-hda', 'C:\Users\Owner\qnx-vm-test\output\disk-qemu',
    '-pidfile', 'C:\Users\Owner\qnx-vm-test\output\qemu.pid',
    '-nographic',
    '-kernel', 'C:\Users\Owner\qnx-vm-test\output\ifs.bin',
    '-serial', 'file:C:\Users\Owner\qnx-vm-test\output\qemu.out',
    '-netdev', 'user,id=net0,hostfwd=tcp::2222-:22',
    '-device', 'virtio-net-pci,netdev=net0'
  ) `
  -RedirectStandardOutput 'C:\Users\Owner\qnx-vm-test\output\qemu.stdout.log' `
  -RedirectStandardError 'C:\Users\Owner\qnx-vm-test\output\qemu.stderr.log' `
  -WindowStyle Hidden
```

Serial output goes to `output/qemu.out`. Wait for `Startup complete` and the
`QNX noname 8.0.0 ... x86_64` banner before connecting.

## Connecting to the VM

The SSH key was generated at `C:\Users\Owner\.ssh\id_ed25519_qnx` and injected
into the VM image.

```powershell
ssh -i C:\Users\Owner\.ssh\id_ed25519_qnx `
  -o StrictHostKeyChecking=no `
  -o UserKnownHostsFile=/dev/null `
  -o MACs=hmac-sha2-256 `
  -p 2222 root@127.0.0.1
```

`MACs=hmac-sha2-256` is required because the Windows OpenSSH client and the
QNX `sshd` otherwise disagree during rekeying.

## Deploying and running a repo binary

Build for the QNX x86_64 target:

```powershell
# In a QNX bash with PATH set to include qnx800/host/win64/x86_64/usr/bin
$env:HOME = '/cygdrive/c/Users/Owner'
# PATH additions and source qnxsdp-env.sh ...
cmake --preset qnx-x86_64-debug
cmake --build --preset qnx-x86_64-debug --target rnqnx_g0_cxx20_probe
```

### Using the repo scripts

`scripts/deploy-qnx.sh` and `scripts/run-qnx.sh` now accept optional SSH key,
port, extra options, and a remote directory. `scripts/vm-qnx.sh` is a convenience
wrapper that hard-codes the defaults for this local VM.

From Git Bash in the repo root, with the VM running:

```bash
./scripts/vm-qnx.sh deploy-run x86_64 rnqnx_g0_cxx20_probe Debug
```

This is equivalent to:

```bash
export QNX_TARGET_IP=127.0.0.1
export QNX_TARGET_USER=root
export QNX_SSH_PORT=2222
export QNX_TARGET_DIR=/data/home/root
export QNX_SSH_KEY="$HOME/.ssh/id_ed25519_qnx"
export QNX_SSH_OPTS="-o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o MACs=hmac-sha2-256"
./scripts/deploy-qnx.sh x86_64 rnqnx_g0_cxx20_probe Debug
./scripts/run-qnx.sh x86_64 rnqnx_g0_cxx20_probe
```

### Manual SCP/SSH

```powershell
scp -i C:\Users\Owner\.ssh\id_ed25519_qnx `
  -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null `
  -o MACs=hmac-sha2-256 -P 2222 `
  C:\Users\Owner\react-native-qnx\build\qnx-x86_64-debug\bin\rnqnx_g0_cxx20_probe `
  root@127.0.0.1:/data/home/root/rnqnx_g0_cxx20_probe

ssh -i C:\Users\Owner\.ssh\id_ed25519_qnx `
  -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null `
  -o MACs=hmac-sha2-256 -p 2222 root@127.0.0.1 `
  'chmod +x /data/home/root/rnqnx_g0_cxx20_probe && /data/home/root/rnqnx_g0_cxx20_probe'
```

## Enabling Screen with the software/dummy WFD blitter

`mkqnximage --graphics=yes` for QEMU x86_64 expects a virtio-drm graphics stack
(`sbin/drm-virtio`, `lib/dll/screen-virtiodrm.so`, Mesa/EGL/GLES libraries under
`usr/lib/graphics/virtio-drm/`, etc.) that is **not present** in this installed
QNX SDP 8.0.2. To run the Milestone 0 Screen executable without a real GPU,
switch the generated `output/build/system.build` graphics section to the
installed software/dummy WFD blitter.

After a normal `mkqnximage --build` (which will warn about missing virtio-drm
files), edit the graphics section in `output/build/system.build` so the system
partition contains:

```text
[type=link] /bin/drm-virtio=/system/bin/true
/lib/dll/screen-sw.so=lib/dll/screen-sw.so

/graphics/driver/libWFDdummy.so=usr/lib/graphics/dummy/libWFDdummy.so
/graphics/driver/libwfdcfg-dummy.so=usr/lib/graphics/dummy/libwfdcfg-dummy.so

/graphics/driver/graphics.conf={
begin khronos
  begin wfd device 1
    pipeline-ids = 1,2,3,4,5
    display-ids = 1
    wfd-dlls = libwfdcfg-dummy.so libWFDdummy.so
  end wfd device
end khronos

begin winmgr
  begin globals
    blit-config = sw
    alloc-config = stdbuf
  end globals

  begin display 1
    video-mode = 800 x 480 @ 60
    cursor = off
  end display

  begin class framebuffer
    display = 1
    pipeline = 1
    format = rgba8888
    usage = sw
  end class
end winmgr
}
```

Then rebuild the system/data partitions and disk with `rebuild-disk.sh` and boot
as usual. `screen` starts, `/dev/screen` exists, and `rnqnx_playground` runs
successfully.

The first attempt with the VMware SVGA driver (`screen-vmwgfxdrm.so` +
`-vga vmware`) failed at `open_drm_dev` because QEMU 11.1.0's VMware SVGA
emulation does not expose a DRM device that the QNX driver can open.

## Validated commands

```bash
# Build the Screen executable
cmake --preset qnx-x86_64-debug
cmake --build --preset qnx-x86_64-debug --target rnqnx_playground

# Deploy and run inside the VM
./scripts/vm-qnx.sh deploy-run x86_64 rnqnx_playground Debug
```

Expected output:

```text
rnqnx_playground: Screen surface initialized (1280x720)
rnqnx_playground: exiting cleanly after 600 frames
```

## Limitations

- The VM is **x86_64**, not AArch64, and uses a **software/dummy** Screen
  blitter. This validates the Screen code path and resource lifecycle but not a
  real GPU/display driver or the AArch64le build.
- This is not a substitute for the AArch64+Screen Raspberry Pi 4 target required
  by `QNX-RN-008`.
- `mkqnximage` itself is not fully fixed; the scripts are workarounds that edit
  generated build files. A proper fix belongs in the QNX tooling or in the
  supported Ubuntu 24.04 build host.
