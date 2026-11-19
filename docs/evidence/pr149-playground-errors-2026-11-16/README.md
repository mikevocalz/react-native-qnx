# Playground fatal-report boundary — Linux receipt

Receipt date: 2026-11-16 (user local); execution date: 2026-11-17 UTC.
Base: `dc76b72` (linked Worklets host fixture).
DRI: PR #149 native-runtime workstream. Independent reviewers: root and SVG
workstream agents, both with no remaining scoped blocker after the candidate
controls passed.

## Reproduced defect and fix

Metro catches a failed entry-module import and reports it through React Native's
`ErrorUtils` and native `ExceptionsManager`. That report can be fatal even when
`QtReactHost::evaluate()` returns true and `lastError()` is empty. The runnable
playground previously checked only those direct-error channels.

The real Metro control registers a runnable React component, then calls the
actual `TurboModuleRegistry.getEnforcing('RNQNX_TEST_MISSING_BOOTSTRAP')`. Before
the fix, the executable recorded `[js fatal]`, mounted the component, saved a
640×480 PNG, printed `RNQNX_PLAYGROUND_SCREENSHOT`, and exited 0. The
[original fatal log](fatal-before.txt) retains that false positive. The pre-fix
executable SHA-256 was
`b4adbc4834fbdae44b912abc50d8dbe8106333687f1bd2217377e74ee9aa1d9f`.

The playground now considers both direct errors and native reports marked
`isFatal` from the current application attempt. It checks after bundle evaluation,
after starting the surface, before capture, and after capture/cleanup has pumped
events. A reload creates a new host; the report index is also scoped explicitly
to the current attempt. Development startup errors retain the existing error
screen/reload path. Host reporting semantics are unchanged.

The positive control uses the actual `ErrorUtils.reportError` path for a soft
report, catches a separate exception, emits a benign warning, and mounts the
registered component. It must still save a successful screenshot. The test
requires the soft report, continued execution after the caught exception, and
the component's mount marker, so it cannot pass by skipping those paths. The
[original soft-control log](soft-before.txt) also records the pre-fix behavior
that must be preserved.

## Commands and observed results

```sh
cmake --build --preset linux-host-renderer \
  --target rnqnx_playground_host --parallel 3
ctest --test-dir build/linux-host-renderer \
  -R '^rnqnx_playground_' --output-on-failure
```

The rebuild passed. All five playground tests passed in 6.00 seconds:

| Test                                          | Observed boundary                                                                             |
| --------------------------------------------- | --------------------------------------------------------------------------------------------- |
| Normal bundled playground                     | Exit 0 and valid 640×480 PNG                                                                  |
| Direct start failure                          | Exit 1, start-error marker, no PNG/success marker                                             |
| Direct stop failure                           | Exit 1, stop-error marker, no PNG/success marker                                              |
| Real Metro reported fatal                     | Exit 1, `BUNDLE_FAILED` with missing-bootstrap marker and fatal report, no PNG/success marker |
| Reported soft error, caught error and warning | Soft report, continued execution and mount markers, exit 0 and valid PNG                      |

[Focused CTest output](playground-ctest.txt) records the candidate run. The full
renderer validator requires both new tests. Its orchestration contract test
passed, including rejection when either test is missing. The test bundles are
rebuilt through Metro on each requested native build, including transitive
platform and package changes.

The final actual configuration with Worklets, Reanimated and Gesture Handler
OFF passed and registered 17 tests, including both new error controls and no
Worklets integration tests. Its playground link command contains none of the
three extension archives or enabled definitions. This is OFF configuration and
inventory evidence; no separate OFF executable run is claimed.

Repository format and type checks passed. The exact generated playground compile
command also passed a Clang syntax check with development support enabled. That
checks the changed development branch's types and syntax; it does not exercise
interactive development reload.

## Complete host graph

After the focused controls, the complete configured graph built successfully
(215 remaining build steps). The full suite then ran sequentially with every
existing timeout and fixture retained:

```sh
cmake --build --preset linux-host-renderer --parallel 3
ctest --preset linux-host-renderer --parallel 1 --output-junit <evidence>/full-ctest.xml
```

Result: **61/61 CTests passed in 741.79 seconds**, with no sanitizer diagnostics.
The QtTest summaries report zero skipped cases. This includes the full React host,
input/accessibility probes and negative controls, Expo Image, native views,
graphics, SVG and video, all five playground controls, and both real-package
Worklets tests. In this full run the Worklets positive test took 15.43 seconds
and its missing-bootstrap control took 1.80 seconds.

- [Complete CTest summary](full-ctest.txt)
- [Raw per-test output](full-last-test.txt)
- [JUnit results](full-ctest.xml)
- [Changed source hashes](source-sha256.txt)

The SVG corpus covered 149 cases with zero pending or failed cases. Its shard
times were 5.99, 295.30, 273.25 and 7.63 seconds. Batch 13 took 272,846 ms to mount
and batch 14 took 260,552 ms, while the other batches took 31–5,346 ms. The
295.30-second shard has little margin below its existing 300-second timeout;
this local pass leaves the separately observed CI performance problem open for
investigation. No parallel heavy tests ran during these trials.

This receipt records the local PR #149 source tree, including the reviewed
fatal-report change, before integration with later PR #159/#161 commits. The
source hashes and containing commit identify that tree. Remote CI results and
historical QNX receipts remain tied to their own recorded revisions; neither is
superseded by this Linux host run.

## Source decisions

| Source inspected                                                                   | Decision                                                                                                             |
| ---------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------- |
| Installed Metro 0.87.1 `guardedLoadModule`                                         | Observe the fatal report after evaluation; Metro intentionally catches the entry-module exception.                   |
| Installed React Native 0.88.1 `ExceptionsManager.handleException` and `ErrorUtils` | Preserve the actual fatal/soft classification instead of classifying diagnostic text or every logged error as fatal. |
| `QtReactHost` and `QnxExceptionsManager`                                           | Reuse the existing per-host report metadata and leave the reporter/runtime contract unchanged.                       |
| Existing playground development reload loop                                        | Keep each attempt's report boundary inside the newly created host's lifetime.                                        |

The native API, platform validation and pinned upstream skill decisions are
recorded in the linked host receipt. The change adds checks at startup/capture
boundaries, not a new polling timer or frame-path scan. It adds no dependency,
premium source or new runtime API, and changes no licensing boundary. Interactive
Marathon/development sessions and QNX remain separate runtime acceptance work.

## Scope and rollback

The environment and pinned library versions are the same as the
[linked host receipt](../pr149-linked-host-2026-11-17/README.md): Linux, Qt 6.11.1,
Clang 19.1.7, actual Hermes/React Native, offscreen software rendering. These
playground controls establish error-boundary behavior. The production playground
target itself does not add sanitizer instrumentation; the linked test fixture
and standalone lifecycle/registry suites have the separately recorded sanitizer
scope.

No QNX cross-build, physical-device, busy-JS, presentation-performance, dedicated
React JS thread, extra recognizer, layout or scroll gate is completed by these
controls. Feature defaults, package pins, the renderer and the ADR-0020 bootstrap
exception remain unchanged. The PR remains draft for its outstanding acceptance
requirements. Revert this playground-error commit, including its controls and
required-inventory entries, to roll back the slice.
