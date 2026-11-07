# Hermes QNX Feasibility

## Current physical AArch64 evidence (2026-11-06)

Hermes `hermes-v260318099.0.5` / React Native 0.88.1 passed the lean-bytecode
JSI probe on a physical Pi 400 running QNX 8.0.5, including 256 KiB and 1 MiB
worker stacks and exit status zero. See [recipe and boundaries](../getting-started/pi400-hello-world.md)
and [captured output](../evidence/pi400-hermes-2026-11-06.txt). This does not
validate the historical x86_64 lane, a complete React Native host, or HDMI
application rendering. Qt-for-QNX licensing remains unresolved.

## Historical investigation

The remaining notes describe older experiments, not the active dependency
versions or the current AArch64 result.

> **SDK 58 migration note (2026-10-01):** The experiments below were performed
> against React Native 0.86.2 / Hermes `250829098.0.16`. The active repository
> line is now Expo SDK 58 / React Native 0.88.0-rc.3, whose installed Hermes
> compiler package is `260318099.0.4`, while the bundled Hermes source tag is `hermes-v260318099.0.2`. Therefore
> `patches/hermes-qnx-250829.patch`, the `third-party/hermes-250829098`
> paths, and the recorded x86_64 runtime behavior are **historical evidence**.
> They must not be applied to the new Hermes source without a clean rebase,
> source audit, cross-build, and target run. The physical Pi Screen lane does
> not enable `RNQNX_BUILD_HERMES_QNX` and is unaffected by this quarantine.

## TL;DR

Hermes for QNX is **compiling**. CMake configures and the lean VM (`libhermesvmlean_a.a`)
is currently building for QNX x86_64. The fixes so far are in the repo's toolchain and
a small `machine/endian.h` shim; the remaining changes were made in the downloaded
Hermes source tree.

## Build recipe used

```powershell
$env:QNX_HOST  = 'C:\Users\Owner\qnx800\host\win64\x86_64'
$env:QNX_TARGET = 'C:\Users\Owner\qnx800\target\qnx'

cmake -S third-party/hermes-250829098 -B build/hermes-qnx-x86_64 -G Ninja `
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/qnx-8.0.cmake `
  -DQNX_TARGET_ARCH=x86_64 `
  -DIMPORT_HOST_COMPILERS=cmake/ImportHostCompilers.cmake `
  -DHERMES_ENABLE_INTL=OFF `
  -DHERMES_UNICODE_LITE=ON `
  -DHERMESVM_INTERNAL_JAVASCRIPT_NATIVE=OFF `
  -DHERMES_ENABLE_TEST_SUITE=OFF `
  -DHERMES_ENABLE_TOOLS=OFF `
  -DHERMES_BUILD_LEAN_LIBHERMES=ON `
  -DHERMES_BUILD_SHARED_JSI=OFF `
  -DHERMESVM_ALLOW_JIT=0 `
  -DHERMES_ALLOW_BOOST_CONTEXT=0 `
  -DLLVM_HOST_TRIPLE=x86_64-unknown-nto-qnx8.0.0 `
  -DCMAKE_SIZEOF_VOID_P=8 `
  -DCMAKE_BUILD_TYPE=MinSizeRel `
  -DPython_EXECUTABLE=C:\Users\Owner\python-embed\python.exe
```

`cmake/ImportHostCompilers.cmake` imports the prebuilt Windows `hermesc.exe`
from `hermes-compiler` so that the `InternalBytecode.hbc` generation step can
run without building a host Hermes.

## What already works

- QCC 12.2 is detected and produces code for `gcc_ntox86_64`.
- `HAVE_COMPUTED_GOTO` succeeds, so the indirect-threaded interpreter can be
  used on QNX.
- `libhermesPlatform.a`, `libhermesConsoleHost.a`, and `libjsi.a` compile and link.
- `lib/libhermesvmlean_a.a` (the lean QNX x86_64 Hermes VM static library)
  builds and links successfully.
- `lib/libhermesvmlean.so` (the lean QNX x86_64 Hermes VM shared library)
  also links successfully.
- `bin/rnqnx_hermes_qnx_probe` builds for QNX x86_64 and links against the
  cross-compiled `libhermesvmlean.so` + `libjsi.a` through `rnqnx_hermes`.
- The probe runs on the QNX x86_64 QEMU VM. It currently `SIGSEGV` in
  `hermes::vm::DictPropertyMap::lookupEntryFor` during `Runtime::initJSBuiltins`.
  The core shows the crash is in the first JS runtime construction, not in the
  build or JSI link.

## Fixes required and applied

### 1. `machine/endian.h` missing

`llvh/Support/Host.h` expects `<machine/endian.h>`, which QNX does not ship in
`usr/include/`. Added `cmake/qnx-hermes-shims/machine/endian.h` that defines
`BYTE_ORDER`, `BIG_ENDIAN`, and `LITTLE_ENDIAN` using GCC `__BYTE_ORDER__`
builtins. The toolchain now adds this shim directory as a system include path.

### 2. QNX API visibility (`isatty` etc.)

QNX hides `isatty`, `getenv`, and other POSIX.1-1988 functions in C++ unless a
feature-test macro like `_QNX_SOURCE` is defined. Added `-D_QNX_SOURCE` to the
QNX toolchain flags.

### 3. `CMAKE_SIZEOF_VOID_P` empty for cross-build

CMake's `CMAKE_SIZEOF_VOID_P` was not populated for the QNX cross toolchain,
so `libhermesvm-config.h` got an empty `HERMESVM_SIZEOF_VOID_P`. Explicitly set
`-DCMAKE_SIZEOF_VOID_P=8`.

### 4. `llvh/lib/Support/Unix/Path.inc` QNX incompatibilities

- QNX `statvfs` uses `f_flag` instead of `f_flags`.
- QNX does not define `MNT_LOCAL`.
- Added `__QNXNTO__` guards in `Path.inc` and treated the local-filesystem
  check as `return true`.

### 5. Hermes `InternalJavaScript` needs `shermes` for the native path

`shermes` (Hermes-to-C compiler) is not in the `hermes-compiler` package. With
`HERMESVM_INTERNAL_JAVASCRIPT_NATIVE=OFF` the build still tried to generate
`internal_unit.c`. Patched `lib/InternalJavaScript/CMakeLists.txt` to guard the
`internal_unit.c` generation and `hermesInternalUnit` target with
`if(HERMESVM_INTERNAL_JAVASCRIPT_NATIVE)`.

### 6. `lib/CMakeLists.txt`

With `HERMESVM_INTERNAL_JAVASCRIPT_NATIVE=OFF`, the `hermesInternalUnit_obj`
target does not exist, but `lib/CMakeLists.txt` still tried to link it into
`hermesvmlean_a`. Removed the redundant `hermesInternalUnit_obj` dependency;
`hermesInternalJavaScript_obj` already resolves to the correct target.

### 7. `lib/Support/OSCompatPosix.cpp` and `lib/VM/Instrumentation/ProcessStats.cpp`

QNX lacks `MAP_NORESERVE`, `madvise`/`MADV_*`, `mincore`, `pthread_getattr_np`,
`SA_RESTART`, `CLOCK_THREAD_CPUTIME_ID` for some configurations, and
`/proc/self/statm`. Added `__QNXNTO__` stub branches so the VM links: no-op
memory advice, no thread page-fault count, stubbed stack bounds, and
process-memory stats returning 0. `ProcessStats.cpp` treats QNX like
Emscripten (return 0 for RSS/VA).

## Remaining risks

- `libhermesvmlean_a.a` links but still uses stubbed QNX OSCompat (memory
  advice, thread IDs, page-fault counts, stack bounds, process RSS). This is
  fine for bootstrapping but may need real QNX APIs before production.
- Runtime testing on the QNX x86_64 VM is needed after the library links.
- The shared `libhermesvmlean.so` and `libjsi.a`/`libhermesapi.a` targets
  should be built and validated next.
- The aarch64 cross-build needs to be verified on the Raspberry Pi/QSTI target.

## Using the cross-built Hermes in react-native-qnx

`cmake/dependencies/HermesQNX.cmake` creates an `rnqnx_hermes` INTERFACE target
that mirrors the host `rnqnx_hermes` but uses the prebuilt QNX artifacts. To
build the QNX Hermes/JSI probe during a QNX configure:

```powershell
cmake -B build/rnqnx-qnx-x86_64 -G Ninja `
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/qnx-8.0.cmake `
  -DQNX_TARGET_ARCH=x86_64 `
  -DRNQNX_BUILD_HERMES_QNX=ON

ninja -C build/rnqnx-qnx-x86_64 rnqnx_hermes_qnx_probe
```

This produces `build/rnqnx-qnx-x86_64/bin/rnqnx_hermes_qnx_probe`, which runs
the same JSI runtime checks as the host Hermes probe.

## Running the QNX Hermes probe

Start the QNX x86_64 QEMU VM with host networking and port 2222 forwarded to the
guest sshd, deploy the probe and shared library, then run:

```powershell
# Start the VM (QEMU needs the disk-qemu.vmdk descriptor to point to the flat
# disk-qemu file; the descriptor may need the extent name patched to "disk-qemu").
& 'C:\Program Files\qemu\qemu-system-x86_64.exe' -m 1G -smp 2 -cpu max `
  -serial file:C:\Users\Owner\qnx-vm-test\output\qemu.out `
  -display none `
  -kernel C:\Users\Owner\qnx-vm-test\output\ifs.bin `
  -drive file=C:\Users\Owner\qnx-vm-test\output\disk-qemu.vmdk,if=ide,id=drv0 `
  -netdev user,id=net0,hostfwd=tcp::2222-:22 `
  -device virtio-net-pci,netdev=net0,mac=52:54:00:6a:a4:2d

# Deploy and run (SSH key at C:\Users\Owner\.ssh\id_ed25519_qnx)
scp -o MACs=hmac-sha2-256 -P 2222 -i C:\Users\Owner\.ssh\id_ed25519_qnx `
  build/rnqnx-qnx-x86_64/bin/rnqnx_hermes_qnx_probe root@127.0.0.1:/data/home/root/
scp -o MACs=hmac-sha2-256 -P 2222 -i C:\Users\Owner\.ssh\id_ed25519_qnx `
  build/hermes-qnx-x86_64/lib/libhermesvmlean.so root@127.0.0.1:/data/home/root/

ssh -o MACs=hmac-sha2-256 -p 2222 -i C:\Users\Owner\.ssh\id_ed25519_qnx `
  root@127.0.0.1 'cd /data/home/root && LD_LIBRARY_PATH=. ./rnqnx_hermes_qnx_probe'
```

Current status: the probe reaches `Runtime::initJSBuiltins` and then
`SIGSEGV` inside `hermes::vm::DictPropertyMap::lookupEntryFor`. The core dump
backtrace (analyzed with `ntox86_64-gdb`) shows the crash is in JS runtime
construction, not in the build or JSI link.

### GDB analysis of the crash

```
#0 hermes::vm::DictPropertyMap::lookupEntryFor(...)
#1 hermes::vm::DictPropertyMap::find(...)
#2 hermes::vm::JSObject::findProperty(...)
...
#5 hermes::vm::Runtime::initJSBuiltins(...)
#6 hermes::vm::Runtime::Runtime(...)
```

At frame `0`:

- `rdi` (first argument, `DictPropertyMap *self`) = `0x9d267800000000`
- `rsi` (`SymbolID`) = `0x1e1`

`0x9d267800000000` is not a canonical x86_64 user-space pointer (top 16 bits are
`0x9d26` instead of `0x0000`). Masking with the `HermesValue` `kDataMask`
(`0x0000ffffffffffff`) would yield `0x267800000000`, which is a valid 48-bit
pointer. This means `DictPropertyMap::lookupEntryFor` is being called with an
un-masked `HermesValue` object tag in the `self` pointer, or the GC segment base
used by `CompressedPointer`/`GCPointer` decoding was placed at an invalid
`0x9d26...` virtual address by the `StorageProvider`.

Attempts so far:

- Added QNX `mprotect`-based `vm_commit`/`vm_uncommit` stubs in
  `lib/Support/OSCompatPosix.cpp` (no change to the crash).
- Tried `StorageProvider::mallocProvider()` for the `Runtime` heap on QNX
  (no change to the crash).

The `0x9d267800000000` pointer is a real `GCCell *` inside a 4 GB contiguous
heap: `0x9d260000000000` (a 4 GB-aligned virtual address) + `0x78000000`
(offset ~120 MB). Instrumentation shows that `Runtime::create` on this build
uses `StorageProvider::mmapProvider()`, and `oscompat::qnx_vm_allocate_aligned`
successfully returns low, canonical addresses starting near `0x1000000000`. The
`0x9d260000000000` base used by the crashing `DictPropertyMap` is therefore _not_
coming from `qnx_vm_allocate_aligned`, nor from the `ContiguousVA` reservation
path that is forced by the QNX bump allocator. The high base is allocated
through some other `mmap` code path and has not yet been traced to its source.

The current `lib/Support/OSCompatPosix.cpp` fix on top of the QNX workarounds:

1. `qnxNextVA` starts at `0x1000000000` (64 GB) instead of `0x100000000`
   (4 GB) to avoid collisions with the QNX image/loader area. The low range
   `0x100000000` works in standalone `mmap` tests but is unavailable in the
   Hermes process; the 64 GB base is still well below the x86_64 canonical
   limit and is confirmed free by `mmap` probes.
2. `vm_commit` on QNX uses `mmap(..., MAP_FIXED)` instead of `mprotect`. On
   QNX, `mprotect` only flips protection bits and does not necessarily back
   `MAP_NORESERVE` `PROT_NONE` reservations with real pages. Re-mmapping with
   `MAP_FIXED` over the reserved range commits the pages.

## Patch files

The source-level changes made in the downloaded `third-party/hermes-250829098`
tree are captured in:

- `patches/hermes-qnx-250829.patch`

Apply the patch before configuring Hermes for QNX:

```powershell
cd third-party/hermes-250829098
git apply ../../patches/hermes-qnx-250829.patch
```

## Recommended next steps

1. Trace the origin of the `0x9d260000000000` 4 GB mapping. It is not from
   `qnx_vm_allocate_aligned` and not from the patched `vm_reserve_aligned`;
   use `pidin` or `/proc/<pid>/as` on QNX to see which mapped region contains
   `0x9d260000000000` and which allocator created it.
2. Let the current `libhermesvmlean_a.a` build finish and fix any new errors.
3. Build `libhermesvm.so` / `libhermes.a` and `libjsi.a` for the full QNX
   runtime.
4. Wire the QNX Hermes binaries into `react-native-qnx/CMakeLists.txt` in place
   of the current host-only `hermes`/`jsc` stubs.
5. Validate on the QNX x86_64 VM and then on the aarch64le Raspberry Pi target.

Generated with [Devin](https://devin.ai)
