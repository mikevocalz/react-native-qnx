# Hermes QNX Feasibility

## TL;DR

Hermes for QNX is **feasible but not plug-and-play**. CMake configures cleanly
for QNX x86_64 using the existing `cmake/toolchains/qnx-8.0.cmake`, a
Windows `hermesc.exe` from `hermes-compiler`, and a small Python interpreter.
Building the first non-trivial target (`libhermesConsoleHost.a`) reaches QNX
headers and libc++ compatibility errors that need source-level patches.

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
- `libhermesPlatform.a` compiles and links successfully.
- The lean-library build path (`HERMES_BUILD_LEAN_LIBHERMES=ON`) excludes the
  compiler and avoids needing `shermes`.

## Errors found in the first build

### 1. `machine/endian.h` not found

`llvh/Support/Host.h` includes `<machine/endian.h>` (BSD-style). QNX 8.0 ships
this under `usr/include/devs/include_<arch>/machine/endian.h`, not the
standard `usr/include` tree.

**Fix applied in toolchain:** `cmake/toolchains/qnx-8.0.cmake` now adds the
architecture-specific `devs/include_<arch>` path. It must be done carefully;
adding `devs/sys` globally makes libc++ pick up internal `sys/stddef.h` and
`sys/time.h` and break.

### 2. `__utime64_scale64_floor` not declared

Caused by `<sys/time.h>` resolving to the internal `devs/sys/time.h` when the
`devs` tree is put on the include path. The public `usr/include/sys/time.h`
should be used instead.

**Implication:** the toolchain should only expose the bare minimum of `devs`
(generally `devs/include_<arch>` for `machine/endian.h`) and should keep
`usr/include/sys` first. If `devs/sys` is needed for `sys/_types.h`, it must be
placed *after* the standard `usr/include` paths.

### 3. `::isatty` not declared in `hermes/Support/OSCompat.h`

QNX has `isatty` in `<unistd.h>`, but `OSCompat.h` is wrapping the global
function. For QNX the wrapper needs either an explicit `using ::isatty;` with
`<unistd.h>` or a `#include <unistd.h>` before the `inline`.

### 4. `__utime64_scale64_floor` and other internal time macros

These are part of the QNX kernel/driver `bintime` API and are not meant to be
visible in user-space headers. This confirms that the `devs/sys` path must not
shadow the public `usr/include/sys` tree.

## Recommended next steps

1. **Create a Hermes/QNX patch set** under `patches/hermes-qnx/` (or fork
   `third-party/hermes`) that:
   - Replaces or shims `<machine/endian.h>` with QNX's public endian helpers
     (`<gulliver.h>` or `<qh/endian.h>`).
   - Adds `#include <unistd.h>` in `hermes/Support/OSCompat.h` for `__QNXNTO__`.
   - Fixes any remaining `pthread_getname_np` / `pthread_setname_np` and
     `mallinfo`/`mallctl` differences.
2. **Refine `cmake/toolchains/qnx-8.0.cmake`** to avoid exposing `devs/sys` and
   to keep the public `usr/include` sysroot first.
3. **Build `libhermesConsoleHost.a`**, then `libhermes.a`, then `libhermes.so`
   or `libhermes.a` to validate the full VM for QNX.
4. **Wire the QNX build into `CMakeLists.txt`** so `rnqnx_qnx` links against
   the cross-built Hermes instead of the host `jsc`/`hermes` stub, or keep the
   current interpreter bridge until Hermes is ready.

## Decision

Hermes on QNX is **proven to be configurable** with the existing toolchain and
prebuilt host tools, but it requires a focused porting effort to patch llvh and
Hermes for QNX headers. The payoff is large (real bytecode execution, smaller
memory, JSI integration). It should be a follow-on milestone, not a same-day
fix.

Generated with [Devin](https://devin.ai)
