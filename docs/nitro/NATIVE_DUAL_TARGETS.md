# Distinct native Nitro v1 QNX and MarathonOS C++ adapters

**Experimental, stacked after Nitro v1 codegen PR #40.** This implements
a shared _portable C++ Nitro core_ build with two independent native
adapter libraries. It does not grant official Nitrogen code generation
support or prove a full native Wayland UI application.

## Native targets

`cmake/RnqnxNitroV1.cmake` owns the pinned Nitro v1 alpha C++ source
inventory, compiler definitions, compatibility include, and public JSI
includes in `rnqnx_nitro_core`.

`packages/qnx-nitro-runtime/CMakeLists.txt` creates the QNX-specific
`rnqnx_nitro_qnx` archive, including generated
`QnxPlatformInfo.binding.cpp`. It **does not** compile or include the
Marathon native class or any Linux Wayland runner.

`native/platform/linux-host/nitro/CMakeLists.txt` creates the
Linux-only `rnqnx_nitro_marathon` archive, including its own
`MarathonNitroRuntime.cpp`, `MarathonPlatformInfo.cpp` and generated
`MarathonPlatformInfo.binding.cpp`. It links the shared portable core
without adding QNX Screen, QNX SDK, Android, or a replacement Qt renderer.

The existing host CMake fixture now builds **both separate libraries**
against the real installed Nitro v1 alpha headers and C++ source:

```bash
pnpm install --frozen-lockfile
pnpm nitro:platform:check
pnpm test:js
cmake -S tests/native/nitro-v1-host -B build/nitro-v1-host \
  -G Ninja -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang
cmake --build build/nitro-v1-host \
  --target rnqnx_nitro_qnx rnqnx_nitro_marathon -j 4
```

The code signs/cross compiles _neither_ artifact on this host lane.
Required remaining gates:

- [ ] A successful Linux CI compile of both archives, with warnings and logs.
- [ ] QNX SDP 8.0.5 native cross-build and actual Pi runtime verification.
- [ ] MarathonOS Alpine/musl AArch64 cross-build and QEMU/CM5 run.
- [ ] A real Qt/JSI runtime calling `installMarathonNitroRuntime`, then
      new `MarathonPlatformInfo` returning marathon/architecture/42.
- [ ] Dynamic linker/closure checks for all C++ symbols under Hermes.
- [ ] Synchronous/async-dispatcher lifetime tests before events/Promises.
- [ ] Upstream Nitrogen's QNX/Marathon platform generator and autolinking.

There is no QNX native binary executing on Linux, only separately
compiled platform code. The Marathon adapter's installer currently
handles a _single synchronous proof class_; it does not claim support
for arbitrary third-party Nitro modules.

## API and engineering skills

Apply exact Margelo `api-design`, `build-nitro-modules`, `cpp` from
the current checked-in skills ledger, plus Callstack
`react-native-best-practices`, `upgrading-react-native` and
`react-native-testing`. Any change to platform ABI needs a
source-reviewed binding contract and an independent QNX regression check.

## Rollback

Remove the Marathon CMake/native adapter, restore QNX's previous single
library CMake source list and its direct Nitro core source/ThreadUtils
ownership, then regenerate/compile with the exact alpha lockfile.
No QNX Screen/Qt/Fabric renderer changes are made.
