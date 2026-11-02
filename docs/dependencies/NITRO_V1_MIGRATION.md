# Nitro Modules v1 alpha native migration — experimental, stacked on PR #36

**Source verified:** upstream margelo/nitro tag `v1.0.0-alpha.nightly.105.g69d365fc9b18`. This is a prerelease, **not stable npm latest**. Actual matching npm artifacts, full native ABI, regenerated lockfile and QNX target execution remain **unverified** until the instructions below pass. Date 2026-11-02.

## V0.37 -> V1 concrete breaking changes

| Area | 0.37.x | 1.0 alpha | react-native-qnx change |
| --- | --- | --- | --- |
| Native source | react-native-nitro-modules/cpp/core,entrypoint,registry,... | nitro-core/native/cpp/{src,include/NitroModules} | explicit CMake source inventory and v1 includes |
| Registration | HybridObjectRegistry registerHybridObjectConstructor | HybridClassRegistry registerHybridClass | QnxNitroRuntime.cpp v1 registry |
| Module metadata | loadHybridMethods + Prototype | immutable HybridClass::create with Class/Base | QnxPlatformInfo Class descriptors and prototype methods |
| JS interface | NitroModules.createHybridObject(name) | NitroModules.getHybridClass(name) and new Constructor() | QNX wrapper and Hermes runtime probe |
| JS package | react-native-nitro-modules import | nitro-core publicly exports NitroModules | use nitro-core in custom QNX host |
| Threading | no QNX dispatcher | no QNX dispatcher yet | sync only; no Promises/callbacks until verified Qt/JS event-loop adapter |

Upstream v1 symbols inspected:
- https://github.com/margelo/nitro/blob/v1.0.0-alpha.nightly.105.g69d365fc9b18/packages/nitro-core/native/cpp/include/NitroModules/HybridClassRegistry.hpp
- https://github.com/margelo/nitro/blob/v1.0.0-alpha.nightly.105.g69d365fc9b18/packages/nitro-core/native/cpp/include/NitroModules/HybridClass.hpp
- https://github.com/margelo/nitro/blob/v1.0.0-alpha.nightly.105.g69d365fc9b18/packages/nitro-core/native/cpp/include/NitroModules/InstallNitro.hpp
- https://github.com/margelo/nitro/blob/v1.0.0-alpha.nightly.105.g69d365fc9b18/packages/nitro-core/js/src/NitroModulesProxy.ts

## Setup: stack on stable Expo/RN PR #36

```sh
# Git ref containing the proposed source migration
git checkout feat/nitro-v1-native-qnx-20261102
node --version
corepack pnpm install --lockfile-only
corepack pnpm install --frozen-lockfile
corepack pnpm validate:dependencies
corepack pnpm validate:qnx-nitro
corepack pnpm test:js
```

If the exact alpha is not on npm for all three packages, or any upstream release API moved again: STOP and update the entire pinned triple to a single available tag; never mix alpha builds or fake lockfile resolutions.

Native proof on licensed QNX SDP 8.0.5:
```sh
source "$QNX_SDP_ENV"
./scripts/validate-qnx-805-cross.sh
./scripts/configure-qnx.sh aarch64 Debug
./scripts/build-qnx.sh aarch64 Debug
pnpm qnx:runtime:validate
```

**Tests must show** Hermès starts; Nitro class registration; new QnxPlatformInfo().platform returns qnx; architecture correctly reflects device; add(20,22) equals 42; repeated registration is harmless; no async dispatcher installed; Expo QNX probe still runs; QNX Screen and Qt render path remain functional. Save separate 8.0.5 Pi stdout/slog2/exit-code evidence, not just older SDP 8.0.2 VM logs.

## Risks/known gaps

- The new NITRO_VERSION macro is fixed to the pinned alpha; update it with **every** exact version move. Compile against the exact installed nitro-core.
- v1 source inventory and CMake includes are source-audited, but C++20, CMake, linker, target pthread, constructor signatures and RN JSI ABI require actual compilation.
- New experimental Nitro JS modules may require a host install order before evaluation; `nitro-core` import throws if native proxy not installed. This is fail-fast, not safe implicit bootstrap.
- In v1 nitrogen platformSchema currently declares apple and android only. QNX and Marathon codegen needs a **separate** verified target extension. Do not claim general cross-platform Nitro 1 support on this synchronous platform proof.
- A Linux/Wayland Marathon host cannot reuse a QNX binary or rely on QNX-only ThreadUtils. Build distinct native adapters.
- Do not merge until PR #36 lockfile and source compatibility gates pass. A clean lockfile and QNX/Marathon host runtime conformance are required.

## Next follow-on

Add a real Qt JS-thread dispatcher with cancel/shutdown lifetime tests for Nitro Promise/callback support. Add QNX/Marathon Nitrogen generation after extension/target schema is accepted. Keep sync-only test as the native anchor.

## Revert

Revert the native class migration, restore the 0.37.1 source list and registration/JS probe, reinstall original locked packages (from main if stable upgrade is held). Never downgrade only one of Nitro/Nitrogen/nitro-core.

## Alpha source compile finding (2026-11-02)

The real Linux/Clang C++20 job compiled the upstream Nitro v1.0 alpha sources and found that `nitro-core/native/cpp/include/NitroModules/NitroLogger.hpp` uses `std::unique_ptr` but omits `#include <memory>`. This is an upstream header-order bug. The scoped `cpp/NitroCoreCompat.hpp` preinclude supplies that C++ standard header to **only** `rnqnx_nitro_qnx` on Clang/GCC. It does not change installed npm sources or suppress diagnostics. The workaround must be removed once a verified newer Nitro v1 fixes the header. A successful Linux compile would still not prove QNX SDP/linker behavior.
