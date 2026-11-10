# PR #108 / #110 integration — November 10, 2026

## Restack after PR #110's SDK follow-up

This merge preserves published #108
`66ac3b9e245b3a02604bb87fba58002f15cbd2c9` as its first parent and incorporates
published #110 `d5d81908a83d4f2fbc022c5915d6f6ae23193192` as its second parent.
Both parents already contain the identical SDK PR-filter removal. The merge
only reconciles documentation and generated evidence state, retaining both
PR-specific SDK follow-up records and the earlier integration history.

All files outside documentation/generated state, including the SDK workflow,
are byte-identical to #108 `66ac3b9`. Native sources, tests, dependencies and
every retained evidence artifact are unchanged. The source-bound 47/47 ON and
25/25 OFF results below retain their original scope and leak-detection
limitation. No native or broad runtime suite was rerun for this restack.

Validation passed: exact file comparison against first parent `66ac3b9`,
comparison of all seven release-gate records against both parents,
`node tests/js/host-renderer-contract.test.mjs`, `pnpm docs:sync`,
`pnpm validate:docs`, changed-file Prettier and `git diff --check`. The complete
#110 evidence entry matches its published parent; the earlier #108 entry is
preserved with this additive restack record.

The seven release-gate records remain unchanged. The new merge must receive
its own hosted CI results; #110's checks and the earlier #108 checks do not
substitute for them. All earlier entries below remain revision-specific
history. Restack DRI: `audit_base_stack`; independent reviewer and publisher:
the parent integrator.

## Stacked SDK validation follow-up

Published integration `3fa112e217d0f01e8f075ad2d95994297c158c87` is the
parent of this CI/documentation follow-up. Retargeting #108 onto the #110
branch exposed an SDK workflow gap: its PR base allowlist covered `main`
and three older feature branches, so the full SDK validation job did not run
for this stack. Removing only that `pull_request.branches` restriction gives
stacked PRs the same JS, type, dependency, security and remaining SDK checks.
Push branches, permissions, concurrency and every job/step remain unchanged.
The full host-renderer workflow already uses this all-PR scope.

Native implementation, fixtures, dependency graph and recorded artifact/source
hashes are unchanged. The existing 47/47 ON and 25/25 OFF results below still
describe those identical native sources, with their leak-detection limitation.
They are retained results, not new runs for this workflow-only change. The new
published candidate still needs its own hosted SDK result.

Validation passed: PyYAML 6.0.3 comparison of the complete workflow structures,
`node tests/js/host-renderer-contract.test.mjs`, `pnpm docs:sync`,
`pnpm validate:docs`, changed-file Prettier and `git diff --check`.
The comparison confirms that only the PR base restriction changed. No new test
duplicates the event-filter edit. No native or broad runtime suite was rerun.

CI/documentation DRI: `audit_small_prs`; independent reviewer and publisher:
the parent integrator. Repository QNX Platform Validation keeps the existing
native evidence scoped to unchanged sources. The pinned Callstack
`github-actions` skill at `59e9d17446a248b21d86f078fcd0bd2d36db1ca4` was read;
its mobile artifact examples do not apply to this trigger-only change, so no
mobile build pattern or skill installation is claimed. Reverting this
follow-up restores the former PR filter without changing runtime behavior.

## Candidate and ownership

This receipt covers the local combination of adaptive-layout PR #108 at
`47cd9d8220e17b4d6909b9b5480548a3be5cf4cf` and the published PR #110 stack at
`73f7da04739fd790fb67565b68ff2fac6c3992e3`. The latter includes the published
#104 follow-up and #106 public-API repairs. Validation ran against the combined
source before publication. The merge containing this receipt preserves both
published input histories; the accompanying artifact records bind the resolved
runtime/test sources by SHA-256. Hosted CI for that published merge remains
separate from the local results below.

The integration preserves both feature sets, the pinned dependency graph and
the existing Fabric/Hermes/Yoga/Qt Quick runtime. The user's smooth QNX report
belongs to the earlier PR #110 candidate and remains in
[its owner report](../evidence/gallery-owner-report-2026-11-10.md). It does not
qualify the newly combined tree. The earlier adaptive results remain in the
[November 9 receipt](../evidence/adaptive-window-layouts-2026-11-09.md).

Implementation: agents `root` and `audit_followups`. Independent source review:
`dev_integration_review`, plus root's integration review. The review covered
modal ownership, native metrics, teardown, generation cancellation and the
associated regression cases. Human merge review and named target acceptance
remain separate.

## Integration defects corrected

### Modal initial focus

The adaptive bridge and PR #110's window-wide modal owner previously both
implemented focus behavior. With `RNQNX_ENABLE_MODAL_FOCUS=ON`, the bridge now
keeps only state/viewport work and the single window owner handles focus. With
the option OFF, the adaptive bridge retains its per-surface focus behavior.

The merged ON path initially left focus on the empty modal root because Fabric
inserts the modal before its controls and View tab eligibility is applied after
insertion. The owner now performs one coalesced refresh after mounting and
replaces root fallback focus with the first eligible descendant. Explicit
descendant focus is preserved. The receiver and `QPointer` bound the queued
callback to its window owner, and it consults current entries after removal.

The native regression covers late tab eligibility, explicit non-tab focus and
removing/destroying a nested modal before the queued refresh. The real Fabric
View-focus test separately checks initial and nested-modal focus.

### Development metrics before evaluation

The new development branch bypassed the production path's native window
observer. Its bundle could read zero initial DeviceInfo dimensions before
surface startup, and later manual resizing used primary-screen fallback data.

`DevSession::Configuration.window` and `.client` now accept a guarded pair for
native geometry ownership. Each new runtime calls the existing
`QtReactHost::observeWindowMetrics(surfaceId, window, client)` before host
preparation and bundle evaluation. The playground supplies this pair and removes
its separate resize timer. Existing unbound sessions retain manual resizing;
bound sessions reject a second manual resize owner.

A successfully fetched bundle remains within the existing byte limit while
native client bounds are zero. Coalesced width/height notifications resume
startup only for the current generation. Stop, reload and window/client
destruction clear pending source and callbacks. Synchronous `prepareHost`
cancellation is checked before continuing into the runtime, including a callback
that throws after stopping. Once the surface starts, its native observer owns
geometry, screen and DPR updates.

The new native fixture uses actual Qt observation, DeviceInfo, Hermes and Fabric
surface constraints. Its bootstrap/AppRegistry stand-ins isolate lifecycle and
metrics; they do not establish React Refresh, rendered controls or a real Metro
editing session.

### Shared runtime-test readiness

The process-supervision JavaScript fixture now publishes its PID readiness file
by atomic rename after writing and closing a temporary file. This prevents the
supervisor from reading an empty file between creation and write. It is the
same focused fixture correction reviewed with PR #107; PR #107's native callback
guard is not part of this integration.

## Current local validation

Host: Linux x86-64, Clang 19, Qt 6.11.1, Debug (`-Og -g1`), Node 26.13.0 and
pnpm 12.8.1. The fresh full native build completed all 934 build steps, followed
by an 18-step incremental rebuild of the final source. Host
sanitizer options were enabled; instrumentation is partial and this is not a
claim that every dependency is sanitized. Native source dependencies were read
from existing caches into a separate build tree.

The enabled configuration includes desktop windowing, Modal focus, development
runtime, WebSockets, frame tracing, input lifecycle, text-input contract and the
accessibility bridge. The full-host-renderer requirement is ON.

Nine generated Metro fixtures were explicitly regenerated before the final
native run so the Image/Switch overrides from the new #106/#110 input are
included. Existing CMake dependency lists do not track every indirect platform
JavaScript override; an incremental native build alone would not establish that
these bundles were current. This validation regenerated the artifacts without
changing that broader build-system behavior.

| Check                                                              | Result                   | Scope                                                                                                                                |
| ------------------------------------------------------------------ | ------------------------ | ------------------------------------------------------------------------------------------------------------------------------------ |
| Native builds                                                      | Passed                   | Fresh 934-step build and final 18-step incremental build; nine generated Metro fixtures refreshed.                                   |
| Earlier focused native tests                                       | Passed, 9/9              | Public View focus, window metrics at scale 1/1.25/2, fullscreen and desktop gallery, React host, Modal focus and controlled Switch.  |
| Final enabled native inventory                                     | Passed, 47/47, exit 0    | 67.38 seconds; finalized DevSession changes and final #110 integration, with leak detection disabled locally as explained below.     |
| Disabled native configuration                                      | Passed, 25/25, exit 0    | Reconfigured all eight feature flags OFF, rebuilt in 55 steps, then executed all 25 registered tests in 32.48 seconds.               |
| Development metrics negative control                               | Expected failure, exit 1 | The same ON binary with `RNQNX_DEV_METRICS_UNOBSERVED_CONTROL=1` failed exactly at initial client dimensions.                        |
| `pnpm typecheck`                                                   | Passed                   | Both host and adaptive TypeScript configurations.                                                                                    |
| Host-renderer, public-entry and controlled-Switch JS contracts     | Passed                   | Source/orchestration tests on the combined tree.                                                                                     |
| Real reset-cache Metro bundles                                     | Passed                   | QNX 685,628 bytes; Marathon 685,669 bytes, including the adaptive path.                                                              |
| Runtime-baseline readiness fixture                                 | Passed                   | `node tests/js/runtime-baseline.test.mjs`; atomic PID-publication change, without the separate PR #107 native callback-guard change. |
| Docs sync, docs validation, changed-file formatting and diff check | Passed                   | Final handoff/state records synchronized; all seven release gates remain pending.                                                    |
| Hosted CI                                                          | Pending                  | Verify the actual published candidate, including enabled and disabled lanes.                                                         |
| QNX / Marathon target execution                                    | Not run                  | No fresh cross-build, board, physical input, compositor or performance qualification.                                                |

The JUnit copies normalize trailing whitespace in embedded font-license output;
test counts, outcomes and diagnostic text are otherwise unchanged.

The focused run is retained in the
[raw JUnit report](../evidence/pr108-integration-focused-2026-11-10.xml) and
[CTest log](../evidence/pr108-integration-focused-2026-11-10.txt). It predates
the final source merge and final DevSession guard. The final ON results are in
the [full JUnit report](../evidence/pr108-integration-native-on-2026-11-10.xml)
and [CTest log](../evidence/pr108-integration-native-on-2026-11-10.txt).

The first full run passed 44/47 and failed three executables with LeakSanitizer's
fatal ptrace-host diagnostic. That
[original JUnit report](../evidence/pr108-integration-full-initial-2026-11-10.xml)
and [original log](../evidence/pr108-integration-full-initial-2026-11-10.txt)
are retained. A complete rerun used process-local
`ASAN_OPTIONS=detect_leaks=0:halt_on_error=1` and
`UBSAN_OPTIONS=halt_on_error=1`, with no source, workflow or kernel change.
All 47 tests then passed. This is address/undefined-behavior checking where
instrumented, not leak-detection proof.

The [ON configure invocation](../evidence/pr108-integration-configure-on-2026-11-10.json)
records compiler/options and local dependency paths. The
[ON artifact record](../evidence/pr108-integration-native-on-artifacts-2026-11-10.json)
records hashes for five focused executables and all nine Metro fixtures before
the build tree was reconfigured OFF. The
[negative-control log](../evidence/pr108-integration-dev-negative-control-2026-11-10.txt)
contains the expected initial-dimensions failure.

The [OFF configure invocation](../evidence/pr108-integration-configure-off-2026-11-10.json),
[OFF JUnit report](../evidence/pr108-integration-native-off-2026-11-10.xml),
[OFF CTest log](../evidence/pr108-integration-native-off-2026-11-10.txt) and
[OFF artifact record](../evidence/pr108-integration-native-off-artifacts-2026-11-10.json)
retain the second configuration. All eight feature cache values were verified
OFF. Modal-focus, development-runtime and frame-trace tests were absent from
that inventory; the always-enabled controlled-Switch test remained registered.
The public View-focus test passed in both configurations, exercising both modal
focus-owner paths. Host sanitizer instrumentation remained enabled, with the
same explicit leak-detection limitation.

The final focused development command is:

```sh
export ASAN_OPTIONS=detect_leaks=0:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1
export QT_QUICK_BACKEND=software
ctest --test-dir "$RNQNX_BUILD_DIR" --output-on-failure \
  -R '^rnqnx_dev_session_host_test$'
RNQNX_DEV_METRICS_UNOBSERVED_CONTROL=1 \
  "$RNQNX_BUILD_DIR/rnqnx_dev_session_host_test" -platform offscreen
```

The first command must succeed. The second is an intentional failing control;
an unrelated startup, socket or loader failure does not prove the regression.

## Continuation and limits

Publish the integrated candidate and verify its actual Actions results. Preserve the
#104/#106/#110 receipts and all seven pending v1 gates.

Physical QNX keyboard/touch/IME behavior, Marathon Wayland delivery, accessibility
speech, cold boot and performance remain unqualified for this candidate. The
development runtime remains opt-in and host-only. A state-preserving Metro Fast
Refresh edit, recovery/reconnect workflow and debugger attachment still need
their own evidence. Host screenshots and synthetic input do not supply it.

Applied source/validation guidance: the repository native-API and platform
validation skills; pinned native skills `api-design`, `build-nitro-modules` and
`cpp` at `1e9e17be6f41c838db472d6a4b943fc26355a3be`; Callstack native guidance
at `59e9d17446a248b21d86f078fcd0bd2d36db1ca4`. The change adds internal C++
configuration fields and reuses the existing host method; it adds no new
JavaScript or Nitro native API.
