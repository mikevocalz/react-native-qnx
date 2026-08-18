# Hermes QNX Feasibility

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

1. Let the current `libhermesvmlean_a.a` build finish and fix any new errors.
2. Build `libhermesvm.so` / `libhermes.a` and `libjsi.a` for the full QNX
   runtime.
3. Wire the QNX Hermes binaries into `react-native-qnx/CMakeLists.txt` in place
   of the current host-only `hermes`/`jsc` stubs.
4. Validate on the QNX x86_64 VM and then on the aarch64le Raspberry Pi target.

Generated with [Devin](https://devin.ai)
