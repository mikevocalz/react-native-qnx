# Bounded native runtime trace

`RNQNX_ENABLE_FRAME_TRACE` defaults OFF. When enabled, a native caller can create
an explicit `RuntimeTrace` session and pass it to the trace-enabled `QtReactHost`
constructor. The existing constructor records nothing. No public JavaScript API,
Nitro binding, global active recorder or scheduling change is added.

```cpp
auto trace = std::make_shared<rnqnx::runtime::RuntimeTrace>(16384);
{
  rnqnx::runtime::QtReactHost host(adapter, trace);
  // Run the owned application workload and stop native producers here.
}
// Host teardown and completion-queue closure have finished.
auto snapshot = trace->freeze();
rnqnx::runtime::RuntimeTrace::writeJson(snapshot, output);
```

A trace-enabled playground accepts `--trace-json /absolute/new-file.json`.
Combine it with the existing `--screenshot` path for a bounded one-shot workload.
The runner destroys its host before freezing and exporting. The export creates
a new file with owner read/write permissions and refuses to replace an existing
file. An export failure returns a nonzero application status. Use the existing
external owned-process watchdog for unattended target trials.

Storage is preallocated for 1 through 65,536 events. A session retains its first
N records and counts later drops. Thread identities are session-local ordinals
of `std::thread::id`, with 64 fixed registration slots. A reused OS thread ID
can reuse its ordinal. A registration overflow also increments the drop count.
Session IDs are process-local; correlate artifacts using their candidate and
process provenance, not this number alone. Numeric surface generations come
from the shared session counter, so separate hosts or restarted surfaces do not
reuse a generation within a session.

Schema version 1 records native boundary stages, begin/end/instant phase,
correlation, numeric surface/generation, sequence, thread and `recordedAtNs`.
The monotonic clock is `std::chrono::steady_clock`, relative to session creation.
Timestamps are taken after acquiring the recorder lock and include lock wait.
The lock protects shared recording state from owner/worker concurrency; it is
never held across application callbacks, JavaScript or Qt work. Recording
allocates no event storage and writes no files or console messages. Export and
snapshot copying happen after the workload. No trace overhead budget has been
measured, so this recorder is not a timing baseline for optimization yet.

Events contain no source URL, module name, exception message, user text,
coordinates, object address or arbitrary application payload. A frozen session
ignores subsequent records. Freeze does not stop a producer or cancel its work;
the caller must quiesce the workload first.

The current stages are host evaluation, surface start/stop, accepted queue
admission, execution of the queued closure and queue close. End means that the
native boundary returned or unwound; it does not assert success, completed
mounting or presentation. Queue execution excludes later RuntimeScheduler
checkpoint work. Zero surface/generation means unavailable or not applicable.
Unknown-surface stop requests therefore have generation zero.

This first slice has no input, Fabric commit/mount, render-loop, swap or display
presentation markers. It aligns no Qt, QNX kernel, hardware-input or compositor
clock. It cannot establish FPS, input-to-photon latency, missed frames or a
scheduling improvement. Any dropped record invalidates a timing analysis;
missing stages and unmatched intervals must remain missing.

Standalone correctness checks need only C++20 and threads:

```sh
cmake -S tests/native/runtime-trace -B build/trace-host \
  -DRNQNX_ENABLE_FRAME_TRACE=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build/trace-host --parallel 2
ctest --test-dir build/trace-host --output-on-failure
```

The tests check capacity, invalid capacity, first-record retention, concurrent
sequence/time ordering, distinct live worker identities, concurrent drops,
65-live-thread registration overflow, freeze, paired and unwound spans, session
IDs, export failure, privacy, locale-independent numbers and parsed JSON.
Host ASan/UBSan instruments the recorder and correctness executable; system
libraries remain outside that instrumentation. CTest supplies an external
15-second bound. Worker joins themselves are not a deadline mechanism.

The root full host tests also include a trace-only integration case for actual
Hermes evaluation, two hosts sharing a session and surface ID, native worker
admission/execution, stop and completion closure. Building this standalone test
does not execute that integration case.

For QNX cross-compilation, use the existing licensed SDP environment and pass
`-DCMAKE_TOOLCHAIN_FILE=/absolute/repository/cmake/toolchains/qnx-8.0.cmake`,
`-DRNQNX_ENABLE_FRAME_TRACE=ON` and `-DBUILD_TESTING=OFF` to the standalone project.
Deploy and run `bin/rnqnx_runtime_trace_test` with the authorized external target
supervisor. That native test is separate from a trace-enabled real application.
Disabling the flag removes this library, its hooks and playground option.
