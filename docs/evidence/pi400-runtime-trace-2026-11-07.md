# Bounded runtime trace implementation and Pi 400 receipt

This implements the first native recorder and boundary-marker slice of Plan 03.
It records no FPS, presentation latency or scheduling improvement. Input,
Fabric commit/mount, render-loop and compositor/presentation integration remain
open.

## Candidate and architecture

The implementation is `6a7b0f0f9161ad27dfb80c4433c0c7eba95516a5`, followed by the
locale-safe serializer at `1dde38b5df5f286947373e9ad907a369c83063d7` and instructions
at `db51cd4de2773720f6cc12ba7a48dab080d5bec4`. The root integrated these with main
for validation at `d05da14`; the application results below use that source graph.
The trace-only Nitro test follow-up is
`6c895ed63cd197a46a9cff1c64532e05ffa28dc4`, integrated by root as `1ffff9c`.
Its actual host/target result is not yet recorded here.

`RNQNX_ENABLE_FRAME_TRACE` defaults OFF and rejects unsupported operating
systems. ON definitions propagate publicly through `rnqnx_runtime_trace` and
`rnqnx_react_host`, so native consumers see matching class layouts. Existing
constructors and the OFF queue layout remain available. An explicit native
shared session enables recording; there is no global active session, public
JavaScript API, invented Nitro binding, extra dispatcher or scheduling change.

The changed paths are `cmake/RnqnxRuntimeTrace.cmake`, the root/React-Qt CMake
hooks, `RuntimeTrace.{h,cpp}`, the existing Qt host/queue headers and sources,
the playground runner and native correctness/integration tests. The recorder
owns fixed event storage and 64 thread slots. It retains the first records and
counts drops. Native owner and worker callbacks share this state under one
short recorder lock; callbacks and JavaScript execute outside that lock.
Snapshot allocation and JSON formatting occur after host teardown.

The runtime-trace implementation agent is the runtime/performance DRI. The
separate `renderer_build_audit` agent reviewed the recorder, hooks, OFF/ON ABI
contract, export ordering, locale fix and trace-only Nitro follow-up without a
blocking finding. Human release roles remain unassigned. The root agent owns
full-host integration, cross-build and device execution.

## Actual checks

| Gate                               | Result  | Command / environment / limit                                                                                                                                                                                                                                                                                                                                                                                                                                                               |
| ---------------------------------- | ------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Standalone macOS correctness       | Passed  | `cmake -S tests/native/runtime-trace -B build/trace-host -DRNQNX_ENABLE_FRAME_TRACE=ON -DCMAKE_BUILD_TYPE=Debug`; build `--parallel 2`; `ctest --test-dir build/trace-host --output-on-failure`: 2/2. AppleClang 21.0.0.21000334, ASan/UBSan.                                                                                                                                                                                                                                               |
| Standalone Linux correctness       | Passed  | Same source and options with `/opt/qnx-python/bin/cmake`, build `/home/qnxbuild/runtime-trace-linux`; `/opt/qnx-python/bin/ctest --test-dir /home/qnxbuild/runtime-trace-linux --output-on-failure`: 2/2. Ubuntu 24.04 amd64 container under Rosetta, GNU 13.3.0, ASan/UBSan. This is not Marathon or Wayland evidence.                                                                                                                                                                     |
| Standalone OFF / unsupported OS    | Passed  | Standalone default-OFF configure/build generated no trace target. Root OFF configure with `-DRNQNX_FETCH_REACT_NATIVE_DEPS=OFF -DBUILD_TESTING=OFF` also generated no trace target. Enabling the standalone flag with `-DCMAKE_SYSTEM_NAME=Generic` failed explicitly.                                                                                                                                                                                                                      |
| Full macOS host ON                 | Passed  | Root configured the existing `build/macos-full-host-tests` cache with trace ON; built the affected graph and ran CTest: 20/20 passed, including real host/session integration and recorder/JSON tests. MinSizeRel, Qt 6.12.0, input lifecycle ON, owned Nitro dispatcher ON. This result precedes the trace-only Nitro assertion follow-up.                                                                                                                                                 |
| Full macOS host OFF                | Passed  | Same full host graph with trace OFF: 18/18 passed. Input lifecycle and owned Nitro dispatcher stayed ON.                                                                                                                                                                                                                                                                                                                                                                                    |
| QNX cross-build                    | Passed  | Activated licensed SDP 8.0.5; standalone `/opt/qnx-python/bin/cmake -S /work/runtime-trace-implementation/tests/native/runtime-trace -B /home/qnxbuild/runtime-trace-qnx -DCMAKE_TOOLCHAIN_FILE=/work/runtime-trace-implementation/cmake/toolchains/qnx-8.0.cmake -DRNQNX_ENABLE_FRAME_TRACE=ON -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF`, then build `--parallel 2`. Root also cross-built trace ON and OFF playground candidates against target Qt 6.12.0. QCC/GCC 12.2.0, AArch64. |
| Physical native recorder           | Passed  | Pi 400 / QNX SDP 8.0.5, unprivileged `qnxuser`, externally supervised: `RNQNX_RUNTIME_TRACE_OK capacity drop order thread span freeze export`, exit 0. Readback matched the updated 60,016-byte candidate hash below.                                                                                                                                                                                                                                                                       |
| Physical application ON            | Passed  | Real Hello World, existing fullscreen/screenshot runner, trace ON and explicit `--trace-json` new path. Exact Nitro v1 marker, captured 1920×1080 PNG, clean exit 0, `RNQNX_TRACE_EXPORTED records=8 dropped=0`.                                                                                                                                                                                                                                                                            |
| Existing export path               | Passed  | Same ON app and an existing JSON destination returned 1 with `RNQNX_TRACE_EXPORT_FAILED`; the previous export remained intact. A screenshot alone did not make this failing export a passing trial.                                                                                                                                                                                                                                                                                         |
| Physical application OFF           | Passed  | OFF candidate rendered/captured 1920×1080 and exited 0 with the Nitro v1 marker. Using the correct private Qt environment, `--trace-json` returned 1 with `rnqnx-playground: Unknown option 'trace-json'.`                                                                                                                                                                                                                                                                                  |
| QNX VM / Marathon shell            | Not run | These physical QNX and portable host checks establish neither lane.                                                                                                                                                                                                                                                                                                                                                                                                                         |
| Performance / input / presentation | Not run | No calibrated clock mapping, tracing overhead comparison, interaction latency distribution, frame pacing or physical presentation measurement.                                                                                                                                                                                                                                                                                                                                              |

The standalone tests exercise capacity bounds, first-record retention,
concurrent sequence/timestamp order, four live worker identities, concurrent
drop accounting, 65-live-thread overflow, freeze, span pairing/unwinding,
privacy, session identity, export failure, localized streams and parsed JSON.
CTest provides an external 15-second bound. Worker joins have no independent
deadline. All recorder/test translation units are host sanitizer-instrumented;
Qt, RN, Hermes, system libraries and the entire application are not rebuilt as
a completely instrumented graph. No TSan or comprehensive leak claim is made.

The initial OFF argument-control attempt failed in the dynamic loader because
its environment omitted private Qt paths. It is excluded from the parser
result; only the corrected-environment run is counted. The earlier 54,456-byte
native artifact/readback is historical, superseded by the locale-safe candidate
below.

## Physical artifacts and trace meaning

| Artifact                      | Bytes   | SHA-256                                                            |
| ----------------------------- | ------- | ------------------------------------------------------------------ |
| Updated native recorder test  | 60016   | `c4b49318490cd8aad16df3d9e14cf72df09cab679f43b76030b584f04e5220bc` |
| Trace ON playground           | 6372112 | `be3c9da88b3689eccaf2b9524714805020fba493b5a483a99af8cdec424cc16c` |
| Trace OFF playground          | 6350344 | `d2d8ec962e6faee6ed55e80341cd29545167f8eab205da3e34a8da41a433710b` |
| Raw exported application JSON | 1571    | `93069fb7f53dda45e93c682e78473a6952aaa5c7f7655a550f89c4468df711d6` |
| Captured application PNG      | 24696   | `e2783976a89c42594a60a9b4657cff70844a5f8a1131e98b8ff1ad400ae14209` |

All three executable readbacks matched their local candidate hashes. The
unchanged deployed Hello World HBC is
`ec405c033b07c0cfe216eb95d3242b58d84bbf17fd2ee3d153153a6c528b1f1e`.
The [runtime baseline manifest](pi400-runtime-baseline-2026-11-07.json) records
the dependency, Qt/QPA plugin and preserved recovery-image closure. This slice
upgraded no package or native ABI dependency. The unchanged lockfile SHA-256 is
`e9602379b5849218cef9718da6db4430e36b51382fd787dcfbecb8269c0aaa52`.

The [formatted application snapshot](pi400-runtime-trace-2026-11-07.json)
contains the same eight records as the raw export; formatting changes its byte
representation. Sequence and session-relative recorded timestamps are ordered.
All eight records use owner-thread ordinal 1. Evaluation, surface start,
surface stop and queue close each have a begin/end pair. Surface 1 uses the same
nonzero generation 2 for its start/stop. No records were dropped.

This Hello World workload posted no owned native queue task, so its capture has
no queue admission/execution records. It does not prove that hook. The new
trace-only Nitro test must separately exercise and assert actual worker
admission/execution before that target gate closes.

Timestamps are taken after acquiring the recorder lock, relative to session
creation, which occurs after earlier playground setup. They are not process
startup timestamps, hardware sample times or presentation timestamps. No stage
latency or startup budget is derived from this capture. End marks a native call
return/unwind, not success or displayed content. Any dropped record invalidates
later timing analysis. Native recorder, real application capture and visible
HDMI behavior are separate evidence categories.

Raw logs and executable readbacks remain outside version control in the local
artifact workspace: `runtime-trace-app-physical.log`,
`runtime-trace-off-physical.log`, `runtime-trace-off-option-env.log`,
`runtime-trace-capture/` and `readback-runtime-trace-*.qnx`. Private target paths,
addresses and ordinary framework teardown diagnostics are not copied here.
The target's unset wall clock does not change the recorder's monotonic clock
contract. The existing working application and recovery boot were preserved.

## Skills, security and next slice

The repository QNX native API and validation skills were applied. Pinned
Margelo sources at `1e9e17be6f41c838db472d6a4b943fc26355a3be` informed these
specific decisions:

| Skill                 | Source SHA-256                                                     | Applied decision                                                                                                                   |
| --------------------- | ------------------------------------------------------------------ | ---------------------------------------------------------------------------------------------------------------------------------- |
| `api-design`          | `4c1dc9af65ba69e4716b3cfeee3592e7bc53ed705b13f25d0dfb0569a981ce03` | Internal native sessions with explicit export; no ambiguous FPS or public JS command API.                                          |
| `build-nitro-modules` | `57466320611351c5b49737f59dad254c66d0f9ed1d2bac565e532248aaba6185` | Reuse the actual owned Nitro route, without a new binding or extra queue.                                                          |
| `cpp`                 | `556801cd3c68728c7c6cab57d17b4eda68c902803f79d8bb048b3f2a825418e7` | Preallocate capture storage, keep callbacks outside locks, retain session ownership through host shutdown and serialize afterward. |

The owner-requested `no-ai-slop` skill was read for the instructions and report.
No new skill installation or dependency migration is implied. The
[Qt QFile reference](https://doc.qt.io/qt-6/qfile.html#open-2) documents the
permission-aware file creation API used by the export path; the inspected
export files were owner-readable/writable.

Trace records contain fixed stage names and numeric identities, never source
URLs, module names, typed text, coordinates, exception strings, object addresses
or application content. The current export refuses to replace a file and
reports write failure. No credentials, privileged kernel tracing or new target
abilities were added. Existing private Qt evaluation rights remain unchanged;
this receipt grants no production or redistribution license.

The next bounded step is physical execution of the traced Nitro assertions,
then separate input/Fabric/render-loop stage work and clock alignment before a
measured workload or optimization. Rollback disables `RNQNX_ENABLE_FRAME_TRACE`
and deploys the recorded OFF executable through the existing application
procedure. No boot, graphics or system service change is required.
