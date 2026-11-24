# Host runner: clean Release build, macOS arm64 (2026-11-24)

Scope: every result here is **host** (macOS 27.0 arm64). Nothing ran on the QNX VM, the Pi 400
or any board. No release, tag or npm package was created. The `expo` gate in
`docs/releases/v1-readiness.json` stays `pending`.

| Where                 | Ran | Not run                                                             |
| --------------------- | --- | ------------------------------------------------------------------- |
| host (macOS arm64)    | yes | -                                                                   |
| QNX VM, Pi 400, board | no  | out of scope for this lane                                          |
| Device Fast Refresh   | no  | needs the board                                                     |
| Draft release         | no  | the owner decides (`RNQNX_RELEASE_OWNER_APPROVED` plus `--confirm`) |
| End-to-end recording  | no  | needs the board                                                     |

This closes item 6 of the open list in `docs/evidence/expo-runner-pack-2026-11-24/README.md`
("a prebuilt runner for macOS and Linux hosts") for macOS arm64 only, with the limits below.
Linux and macOS x86_64 are not built.

## Build

Source: `main` at `5682d2dc96d10d16fbd2582b496163d37a82f359` (includes #234 and #236), in a fresh
clone. Host: macOS 27.0, Apple clang 21.0.0, CMake 4.4.5, Homebrew Qt 6.12.0.

```sh
pnpm install --frozen-lockfile          # root install, pnpm 12.18.0, frozen lockfile
cmake --preset macos-host-release \
  -DBUILD_TESTING=ON -DRNQNX_BUILD_HERMES=ON -DRNQNX_ENABLE_HOST_SANITIZERS=OFF \
  -DRNQNX_REQUIRE_FULL_HOST_RENDERER=ON -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt
cmake --build build/macos-host-release \
  --target rnqnx_playground_host rnqnx_hermes_build_mode
```

`CMakeCache.txt` shows `CMAKE_BUILD_TYPE=Release`, `CMAKE_CXX_FLAGS_RELEASE=-O3 -DNDEBUG`,
`RNQNX_ENABLE_HOST_SANITIZERS=OFF`, `RNQNX_BUILD_HERMES=ON`. This is not the AddressSanitizer
test build. The preset sets `BUILD_TESTING=OFF`, but the host renderer graph only exists under
`BUILD_TESTING=ON`, so the build overrides it. The host Qt graph and Hermes are built from
source; the log has 859 steps and one Hermes `-Wsometimes-uninitialized` warning
(`TraceSerializer.cpp:26`), no errors. The first build ran 3:58.91 wall clock. The shell that
ran it was restarted partway, so `build-2-incremental.log` is the ninja re-check.

Logs: `configure.log`, `build-1.log`, `build-2-incremental.log`.

## Binary

`rnqnx_playground_host` is the runnable host executable in the CMake graph
(`vnext/probes/playground/main.cpp`). It is a development playground host, not a runner for an
arbitrary app: it loads the playground bundle or a bundle passed with `--bundle`.

| Item                                          | SHA-256                                                            | Bytes   |
| --------------------------------------------- | ------------------------------------------------------------------ | ------- |
| `bin/rnqnx_playground_host` as built          | `368b0973f3852671fea35348c54dc64715e9e43f4d69d0de4f69e0a3b21baa51` | 7478728 |
| `rnqnx-host-macos-arm64.tar.gz` (relocatable) | `3ce7c74e3d9ba84f899bf9680152b0e0ff9393138635d7b47136e854728f3ee8` | 4024930 |
| archived `rnqnx_playground_host`              | `3ed7a150510a757c288d76e217ea1a6685c00e0c17dbc2deaa4adf9ea5bcedcf` |         |
| archived `lib/libhermesvm.dylib`              | `99cf33752957cb5c852dd2f7851e42ca3dc126556a6433e31b1ab2ba1ff297f6` |         |
| archived `lib/libjsi.dylib`                   | `b2ec38c3e9cc6526c38ce0c224b841e6cd37755ad7803b14293f1669c9f28063` |         |
| archived `rnqnx-playground.bundle.js`         | `3148eec1f395456839c43f01da8760c536e1bb7e079ee72b4999e2f54f6962f8` |         |

`file` reports Mach-O 64-bit arm64. `rnqnx_hermes_build_mode` prints
`RNQNX_HERMES_BUILD_MODE Release` and exits 0, so the VM is not the slow-debug build.
`otool -L` shows no `libclang_rt.asan` dependency.

The binary as built finds `libhermesvm.dylib` and `libjsi.dylib` through absolute rpaths into the
build tree. The archive copy has those two rpaths removed, `@executable_path/lib` added, and
all three Mach-O files re-signed ad hoc (`codesign -s -`). Its hash therefore differs from the
as-built one. Inspection output: `binary-inspect.txt`.

## Smoke run (host)

`QT_QPA_PLATFORM=offscreen ./rnqnx_playground_host --bundle rnqnx-playground.bundle.js --screenshot <png>`
printed `Running "RNQnxPlayground"` and `RNQNX_PLAYGROUND_SCREENSHOT <png> 640x480`, and exited 0.
It ran twice: from the build tree (`smoke-playground-host-640x480.png`) and from a fresh
extraction of the archive in another directory (`smoke-relocated.txt`), which shows the
rpath rewrite works. The PNG shows the playground's text, colored squares and a wrapped
paragraph. Glog `W` lines at exit are destructor notices, not failures.

This shows the Release host starts, loads a Metro-built `qnx` bundle through Hermes and renders
once offscreen. It does not show a windowed run, input, Fast Refresh or any QNX behavior.

## Limits

1. **Not self-contained.** The binary links Homebrew Qt 6.12.0 dynamically at absolute paths
   (`/opt/homebrew/opt/qtbase`, `qtdeclarative`) plus an rpath to `/opt/homebrew/opt/qt/lib`. It
   runs only on a Mac with that Homebrew Qt installed. Bundling Qt (`macdeployqt` or equivalent)
   is not done, and with it the Qt license review that redistribution needs (ADR-0013).
2. **Ad hoc signature only.** No Developer ID signature, no notarization. Gatekeeper will
   quarantine a downloaded copy.
3. **Not published.** The archive is not committed and not on a release. Hosting it is the
   owner's decision under ADR-0018. There is no manifest entry, so `runner fetch --target
macos-arm64` has nothing to fetch and no hash is published for verification on download.
4. **Playground host only.** It is not a development runner for user apps and carries no runner
   descriptor (`rnqnx-runtime.json`), so `runner check` does not apply.
5. **macOS arm64 only.** Linux and macOS x86_64 were not built.
6. **Not reproducible-checked.** One build, no second clean build to compare hashes.
