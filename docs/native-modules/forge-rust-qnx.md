# Use the shared Forge Rust engines from the QNX native host

This opt-in native slice links the same `nf-sys` Rust archive and per-document
C++ adapter used by Nitro Forge. The host fixture exercises real Tally editing,
formulas, undo/redo, snapshot restore, and reopening a saved document in a new
process. It does not replace the Qt Quick/Fabric renderer.

**Evidence boundary:** Linux host compilation and behavior are verified. The
November 7 QNX cross-build and physical-device results are recorded in
[the target receipt](../evidence/forge-qnx-2026-11-07.md). This slice exposes an internal C++ owner;
it does not register a JavaScript API, generate QNX Nitro bindings, or establish
Expo Modules API 2.0 support for these engines.

## Check the prerequisites

Use a separate checkout of `mikevocalz/nitro-forge` containing the reviewed common
package. The initial dependency revision is
`2f33f2fb204077c4a493788ee36890c76f03b31a`
([Forge draft PR #57](https://github.com/mikevocalz/nitro-forge/pull/57)).
The QNX host CI pins the exact commit it consumes in
`.github/workflows/forge-native-host.yml`.

You need CMake 3.31.9 or later, a C++20 compiler, Ninja, Rust, and a prebuilt
`nf-sys` static archive for the operating system and CPU you are building for.
The CMake integration never runs Cargo and never substitutes a host archive
during a QNX build.

The following boundaries must be distinguished for a QNX React Native feature claim:

- **Rust and SDP:** November 7 target validation uses licensed SDP 8.0.5 and
  Rust 1.99.1's `aarch64-unknown-qnx` target (`target_os="qnx"`). The locked
  getrandom 0.3.4 recognizes the older `nto` name. The independently reviewed
  opt-in patch below selects its existing QNX backend without removing engines
  or changing global target cfg. Source-built std plus this patch produced the
  complete archive and passed physical-device probes. This experimental build
  does not change the default Cargo graph or establish an officially supported
  prebuilt Rust target distribution.
- **Nitro ABI:** this repository pins
  `1.0.0-alpha.nightly.105.g69d365fc9b18`, while the Forge implementation baseline
  uses `1.0.0-alpha.nightly.120.g33ef045b96c0`. The reused session and queue have
  no Nitro dependency. Forge's `Hybrid*` classes are deliberately not linked
  into this QNX target until a matching generated/runtime ABI is proven.
- **JavaScript lifecycle:** the existing QNX Nitro runtime only enables
  synchronous bindings. A tested JS-thread completion dispatcher, Promise
  settlement, and disposal before Hermes teardown are required before exposing
  the native document owner to applications.

The Rust [QNX platform documentation](https://doc.rust-lang.org/rustc/platform-support/nto-qnx.html)
describes target and SDK prerequisites. Record the exact compiler and target
used when resolving the dependency gap; do not infer compatibility from a target
name alone.

## Run the real engine on a development host

From the `react-native-qnx` checkout, set an absolute path to your reviewed
Forge checkout and build its archive explicitly:

```bash
export RNQNX_FORGE_CHECKOUT=/absolute/path/to/nitro-forge
cargo build --locked --release -p nf-sys \
  --manifest-path "$RNQNX_FORGE_CHECKOUT/Cargo.toml"

cmake -S tests/native/forge-host -B build/forge-host -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DRNQNX_FORGE_SOURCE_DIR="$RNQNX_FORGE_CHECKOUT" \
  -DNF_RUST_LIBRARY="$RNQNX_FORGE_CHECKOUT/target/release/libnf_sys.a" \
  -DRNQNX_ENABLE_HOST_SANITIZERS=ON
cmake --build build/forge-host --parallel
ctest --test-dir build/forge-host --output-on-failure
```

This standalone fixture does not require Qt, Hermes, Expo, npm installation, or
QNX SDP. Five CTest entries cover the native owner, configuration failures, the
shared queue, and separate write/read processes. The write process edits Tally
cells, recomputes a formula to `42`, verifies undo/redo and rejection atomicity,
then saves the existing `NFD1` format. The read process creates a new document,
restores the file, recomputes `42`, edits and undoes again.

On November 5, 2026, GCC 13.3 built the C++ boundary with ASan and UBSan and all
five tests passed. The sandbox prevented LeakSanitizer from reading
`/proc/<pid>/task`; the local run therefore used:

```bash
ASAN_OPTIONS=detect_leaks=0 ctest --test-dir build/forge-host --output-on-failure
```

That result is not a leak-check pass. Keep leak detection enabled on a host
that supports it. The Rust archive in this run was a normal release build;
the sanitizer instrumentation covers the C++ adapter and shared C++ session,
queue, and probes. The CTest transcript is
`build/forge-host/Testing/Temporary/LastTest.log`.

## Use the native owner

`QnxForgeRuntime` owns a worker before accepting work. `open()` resolves to a
ready `QnxForgeDocument`. Each document reuses `ForgeSerialQueue` and
`ForgeRustSession` from the Forge checkout; no engine, undo journal, document
format, or Rust handle registry is copied into this repository.

These C++ examples use blocking `get()` in a native command-line/test context.
An eventual Nitro binding must settle its Promise through the QNX JS dispatcher
instead of waiting on a UI or JS thread.

```cpp
rnqnx::forge::QnxForgeRuntime runtime;
auto document = runtime.open("tally").get();
auto edit = document->apply("setCell", R"({"addr":"A1","value":"42"})");
auto undo = document->apply("__undo");
edit.get();
undo.get(); // FIFO: this cannot overtake the edit.
```

Input strings and byte vectors are owned copies when a call returns. Results
own their strings, events, and bytes after the Rust result buffers are freed.
Errors reject the returned future without stopping later document operations:

```cpp
try {
  document->apply("setCell", R"({"addr":"A0","value":"9"})").get();
} catch (const std::exception& error) {
  // Report the invalid address; the original document is still usable.
}
auto savedBytes = document->snapshot().get();
auto reopened = runtime.open("tally").get();
reopened->restore(savedBytes).get();
```

Close is a FIFO barrier. Repeated calls share its completion and later
operations reject. Dropping a document schedules its close on the worker;
the worker retains live sessions so disposal during shutdown still closes
them on that worker. Dropped futures do not cancel accepted work.

```cpp
document->close().get();
reopened->close().get();
runtime.shutdown(); // Reject new work, drain accepted work, close, then join.
```

The runtime destructor performs the same shutdown. Documents may outlive it,
but subsequent operations reject. Shutdown is a blocking owner teardown step;
it is not an operation to call from a UI hot path. This slice provides no
cancellation or per-document parallelism: documents share one worker, while
each document preserves submission order. Its internal command envelope mirrors
the existing native ABI and is not a proposed public JavaScript API.

## Build QNX from the audited target archive

Use a licensed SDP environment and an audited QNX `nf-sys` archive built from
the same Forge revision. Record the archive hash, Rust compiler/target,
dependency compatibility changes, SDK version, CPU, and native system libraries.
`RNQNX_FORGE_RUST_TARGET` is an explicit declaration checked against CMake's
target CPU; it is not proof of the archive's origin.

The target receipt identifies the exact artifact and experimental compatibility
patch used for the November 7 build. Reproduce the native build with:

```bash
# Source your licensed SDP environment before this command.
cmake -S . -B build/forge-qnx -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/qnx-8.0.cmake \
  -DQNX_TARGET_ARCH=aarch64 \
  -DBUILD_TESTING=OFF \
  -DRNQNX_G0_PROBE_ONLY=ON \
  -DRNQNX_FETCH_REACT_NATIVE_DEPS=OFF \
  -DRNQNX_BUILD_FORGE_QNX=ON \
  -DRNQNX_FORGE_BUILD_PROBE=ON \
  -DRNQNX_FORGE_SOURCE_DIR="$RNQNX_FORGE_CHECKOUT" \
  -DRNQNX_FORGE_RUST_TARGET=aarch64-unknown-qnx \
  -DNF_RUST_LIBRARY=/absolute/path/to/qnx/libnf_sys.a \
  -DNF_RUST_SYSTEM_LIBRARIES="regex;fsnotify"
cmake --build build/forge-qnx \
  --target rnqnx_forge_runtime rnqnx_forge_lifecycle_probe \
    forge_shared_engine_roundtrip forge_serial_queue_test
```

Supply `NF_RUST_SYSTEM_LIBRARIES` if the audited Rust build reports additional
native link dependencies. For x86_64, use the matching QNX toolchain architecture
and `x86_64-pc-qnx` Rust target. QNX builds require `q++`/`qcc` and sourced
`QNX_HOST`/`QNX_TARGET`; a Linux compiler with only a changed CMake system name
fails configuration.

Deploy `build/forge-qnx/forge-native/forge_shared_engine_roundtrip` with its
required target libraries through your existing QNX deployment lane. On the
target, run two separate invocations in a writable directory:

```bash
./forge_shared_engine_roundtrip write /tmp/forge-tally.nfd
./forge_shared_engine_roundtrip read /tmp/forge-tally.nfd
./forge_serial_queue_test
./rnqnx_forge_lifecycle_probe
```

Capture both exit codes and stdout, the archive/executable hashes, target OS and
CPU, SDK provenance, and deployed libraries. This proves the shared native engine
on that target only. A later React Native feature gate must also execute the
QNX owner through aligned Nitro bindings, exercise JS Promise errors and
shutdown, and render/input through the existing Qt/Fabric host.

## Interpret failures and roll back

| Failure                                           | Next action                                                                                        |
| ------------------------------------------------- | -------------------------------------------------------------------------------------------------- |
| `RNQNX_FORGE_SOURCE_DIR` is missing or incomplete | Use an absolute checkout path containing the reviewed common package and per-document v1 headers.  |
| `NF_RUST_LIBRARY must name...`                    | Build and supply the archive explicitly for this target.                                           |
| `RNQNX_FORGE_RUST_TARGET must be...`              | Match the QNX architecture and audited Rust target; do not relabel a host archive.                 |
| QNX compiler/environment guard                    | Source the licensed SDK and use the repository QNX toolchain.                                      |
| Rust `getrandom` backend error                    | Resolve the `qnx`/`nto` target-cfg dependency mismatch in a reviewed Forge change before retrying. |
| `QNX Forge document is closed`                    | Open a new document; a closed handle cannot be reused.                                             |
| `QNX Forge runtime is shut down`                  | Create a new host owner for the next runtime lifecycle.                                            |

`RNQNX_BUILD_FORGE_QNX` and `RNQNX_FORGE_BUILD_PROBE` default to `OFF`. Disable
them to remove this native slice from the QNX build. The change does not edit
the Qt, Fabric, Hermes, Screen, Expo, Nitro package, or npm dependency graphs.

## Skill and review evidence

The sources below were read as guidance; no local skill installation or device
execution is implied.

| Exact skill                             | Source revision                            | Applied decision                                                                               | Evidence                                            |
| --------------------------------------- | ------------------------------------------ | ---------------------------------------------------------------------------------------------- | --------------------------------------------------- |
| Margelo `api-design`                    | `1e9e17be6f41c838db472d6a4b943fc26355a3be` | Ready document factory, explicit errors/cleanup, native call sites before any JS binding claim | Native API above and lifecycle test                 |
| Margelo `build-nitro-modules`           | `1e9e17be6f41c838db472d6a4b943fc26355a3be` | Isolate native owner until generated/runtime Nitro ABI and dispatcher are aligned              | No fabricated QNX HybridClass or generated bindings |
| Margelo `cpp`                           | `1e9e17be6f41c838db472d6a4b943fc26355a3be` | Owned inputs/results, worker disposal, deterministic drain/close, small source files           | ASan/UBSan host lifecycle and concurrency tests     |
| Callstack `react-native-best-practices` | `59e9d17446a248b21d86f078fcd0bd2d36db1ca4` | Keep engine/I/O work off UI and JS threads; make no unmeasured performance claim               | Owned worker; Qt/JS integration explicitly pending  |
| Callstack `writing-user-docs`           | `59e9d17446a248b21d86f078fcd0bd2d36db1ca4` | State prerequisites, failures, and evidence limits before target instructions                  | Separate host and physical-QNX procedures/receipts  |

The independent native review caught a caller-thread disposal path. The
implementation now retains live sessions on the worker until close or shutdown,
with document destruction queuing its close. The November 7 follow-up now
records licensed QNX build/device evidence. Hosted CI and maintainer review
remain; generated Nitro/JS integration is a separate application feature.

## QNX 8 Rust compatibility patch

`patches/getrandom-0.3.4-qnx.patch` adds Rust's QNX 8 `target_os="qnx"`
to the existing QNX file backend, native errno helper, and libc dependency
selection (including the original Cargo manifest). It preserves `nto`, all
versions/features, and the existing `/dev/urandom` reads and error handling.
It neither supplies custom random bytes nor changes the compiler target cfg.
An independent source review compared all extracted crate files and confirmed
only these four files changed. Keep the pinned libc 0.2.190: the manifest's
older minimum alone does not establish new-QNX support.

Prepare a separate Forge source copy at the exact revision above and a separate
copy of the published getrandom 0.3.4 crate. Before extraction, verify the crate
archive SHA-256 against Forge's Cargo.lock:
`899def5c37c4fd7b2664648c28120ecec138e4d395b459e5ca34f9cce2dd77fd`.
Do not edit Cargo's registry cache. Apply the patch inside the copied crate:

```bash
patch --dry-run -p1 < /absolute/react-native-qnx/patches/getrandom-0.3.4-qnx.patch
patch -p1 < /absolute/react-native-qnx/patches/getrandom-0.3.4-qnx.patch
```

The Rust 1.99.1 target has no prebuilt standard library. The diagnostic build
uses that exact compiler's `rust-src`, with the explicitly unstable build-std
mechanism enabled through `RUSTC_BOOTSTRAP=1`. This is source-built experimental
target evidence, not a claim of a supported prebuilt Rust distribution:

```bash
# Source licensed QNX SDP first; install matching Rust 1.99.1 + rust-src.
export CARGO_TARGET_DIR=/absolute/scratch/forge-rust-target
export CARGO_BUILD_JOBS=2
RUSTC_BOOTSTRAP=1 cargo build --offline -Zbuild-std=std,panic_unwind \
  --target aarch64-unknown-qnx --release -p nf-sys \
  --manifest-path /absolute/forge-source-copy/Cargo.toml \
  --config 'patch.crates-io.getrandom.path="/absolute/getrandom-0.3.4-qnx"'
```

The path override requires a lockfile source adjustment. Verify that the only
Forge Cargo.lock change removes getrandom's registry `source` and `checksum`;
all package versions and dependency edges must remain unchanged. Retain this
adjusted lockfile and use `--locked --offline` for subsequent builds. Cargo's
build-std workspace can warn that the patch is unused there; verify the actual
application compilation names the patched getrandom path. Build all seven
engines through `-p nf-sys`; do not omit Folio to evade this dependency.

With `RNQNX_FORGE_BUILD_PROBE=ON`, `rnqnx_forge_lifecycle_probe` also reuses the
native owner fixture on QNX. Run it separately from the shared write/read and
queue probes. No sanitizer result is implied by a normal target release build.

The target validation also builds `tests/native/forge-rust-toolchain` using the
same explicit getrandom path override and build-std flags above. Its checked-in
lockfile requires that override. It checks one successful 32-byte OS entropy
read and a thread returning 42; it does not statistically assess entropy quality.
Use `--manifest-path tests/native/forge-rust-toolchain/Cargo.toml` and omit
`-p nf-sys` when building this probe from the QNX repository.

`rustc --print native-static-libs` reports
`-lc -lm -lsocket -lregex -lfsnotify -lgcc_s -lc -lm -lsocket -lregex` for this
archive. The SDK driver supplies libc/libgcc, and the shared package supplies
m/socket; the explicit additional CMake libraries above complete that set.
Run lifecycle tests under an external watchdog: per-future waits do not bound
shutdown, joins or barriers. The reused fixture's output says "host QNX Forge
seam"; physical-target metadata in the receipt establishes where it ran.
