# Use the shared Forge Rust engines from the QNX native host

This opt-in native slice links the same `nf-sys` Rust archive and per-document
C++ adapter used by Nitro Forge. The host fixture exercises real Tally editing,
formulas, undo/redo, snapshot restore, and reopening a saved document in a new
process. It does not replace the Qt Quick/Fabric renderer.

**Evidence boundary:** Linux host compilation and behavior are verified. A QNX
cross build and device run are pending. This slice exposes an internal C++ owner;
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

Three separate gaps block a QNX React Native feature claim:

- **Rust and SDP:** this development environment has no licensed QNX compiler or
  target. Rust 1.99.1 lists `aarch64-unknown-qnx` and `x86_64-pc-qnx`, with
  `target_os="qnx"`. The locked `getrandom` 0.3.4 used by the full `nf-sys` graph
  selects its QNX backend using `target_os="nto"`; Folio's `lopdf`/`rand` chain
  reaches that dependency. The compiler, target standard library, and dependency
  configuration need a reviewed compatibility resolution before producing the
  full QNX archive. Installing `qcc` alone is insufficient. This slice does not
  patch dependencies, remove engines, or claim a successful QNX Cargo build.
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

## Prepare a QNX build after resolving the toolchain gap

Use a licensed SDP environment and an audited QNX `nf-sys` archive built from
the same Forge revision. Record the archive hash, Rust compiler/target,
dependency compatibility changes, SDK version, CPU, and native system libraries.
`RNQNX_FORGE_RUST_TARGET` is an explicit declaration checked against CMake's
target CPU; it is not proof of the archive's origin.

The following is a target procedure to run once that artifact exists, not a
record of a successful cross build:

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
  -DNF_RUST_LIBRARY=/absolute/path/to/qnx/libnf_sys.a
cmake --build build/forge-qnx \
  --target rnqnx_forge_runtime forge_shared_engine_roundtrip
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

| Exact skill                             | Source revision                            | Applied decision                                                                               | Evidence                                             |
| --------------------------------------- | ------------------------------------------ | ---------------------------------------------------------------------------------------------- | ---------------------------------------------------- |
| Margelo `api-design`                    | `1e9e17be6f41c838db472d6a4b943fc26355a3be` | Ready document factory, explicit errors/cleanup, native call sites before any JS binding claim | Native API above and lifecycle test                  |
| Margelo `build-nitro-modules`           | `1e9e17be6f41c838db472d6a4b943fc26355a3be` | Isolate native owner until generated/runtime Nitro ABI and dispatcher are aligned              | No fabricated QNX HybridClass or generated bindings  |
| Margelo `cpp`                           | `1e9e17be6f41c838db472d6a4b943fc26355a3be` | Owned inputs/results, worker disposal, deterministic drain/close, small source files           | ASan/UBSan host lifecycle and concurrency tests      |
| Callstack `react-native-best-practices` | `59e9d17446a248b21d86f078fcd0bd2d36db1ca4` | Keep engine/I/O work off UI and JS threads; make no unmeasured performance claim               | Owned worker; Qt/JS integration explicitly pending   |
| Callstack `writing-user-docs`           | `59e9d17446a248b21d86f078fcd0bd2d36db1ca4` | State prerequisites, failures, and evidence limits before target instructions                  | Separate host procedure and unexecuted QNX procedure |

The independent native review caught a caller-thread disposal path. The
implementation now retains live sessions on the worker until close or shutdown,
with document destruction queuing its close. Remaining merge evidence is the
licensed QNX build/device lane and subsequent generated Nitro/JS integration.
