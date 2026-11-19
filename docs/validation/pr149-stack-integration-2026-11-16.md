# PR 149 final SVG and Expo host integration

Validation ran on November 16 in America/New_York (November 17 UTC). The
Worklets branch at local `ebd8182e6cded86331505beab780c79ec5e8cbea` incorporates
published Expo head `d50e34d99a504f48b690225ad28e8cb3cd8ca37a`, whose verified
tree is `03455c6af3891b91aa4a7e804055a777af57c5aa`. That prerequisite includes
published SVG head `d3eacc75dcad761c992736223a68b53ff852203b`.

The merge had one conflict, between independent additions to
`docs/handoff/CURRENT_STATE.md`. Both parent section bodies were retained.
Native sources merged automatically. The pre-receipt merged tree is
`01d5d6dbc912c08351bd0ddeffb6b28f19e0a6f4`; later changes add this receipt and
canonical evidence metadata only. All historical failure and pass receipts
remain intact.

## Integration and independent review

The combined adapter retains the Worklets view observers and the SVG
`ViewProps::onLayout` guards in their respective paths. The host suite retains
the mount-hook ordering regression and adds both listener lifecycle
regressions. Expo's direct Fabric fixture explicitly subscribes node 701 to
`onLayout`, as required by the corrected production contract. No production
guard, assertion, timeout, dependency pin or default feature switch changes
in this merge.

The separate SVG/Fabric agent independently compared the combined adapter
and host test against both parents and found no blocking source issue.
That review also verified that the handoff retains both parent section
bodies without unresolved conflicts or duplicate headings. The earlier
independent reviews of gesture cancellation, the real-library fixture and
the runnable playground error boundary remain recorded in their receipts.

## Affected native validation

The existing Debug host graph was incrementally rebuilt with all previously
enabled integrations, using Debian 13 x86-64, Clang 19.1.7, Qt 6.11.1,
CMake 3.31.10, Ninja 1.13.2, Node 26.13.1 and pnpm 12.18.0. The build passed.
The selected 23 CTests then passed sequentially in **95.48 seconds**:

| Coverage                                               | Result                                                                                                         |
| ------------------------------------------------------ | -------------------------------------------------------------------------------------------------------------- |
| Combined QtReactHost suite                             | 62 cases passed; includes listener lifecycle and mount-hook ordering                                           |
| Expo module and Fabric                                 | Both CTests passed with the final worker/cache code and explicit layout subscription                           |
| Public Worklets/Reanimated/Gesture Handler integration | Real library frame callbacks, timing pixels, pan, remount and queued-work teardown passed                      |
| Missing Worklets bootstrap                             | Real bundled negative control rejected the missing bootstrap                                                   |
| Runnable playground                                    | Screenshot, direct start/stop failures, reported fatal rejection and soft/handled continuation all passed      |
| Gesture and mounting                                   | Gesture core, shadow mutation adapter, shadow-tree mounting and QtQuick mounting passed                        |
| Image and SVG                                          | Fabric delivery, image units/item/queue, retained scene geometry, ForeignObject, image and loader cases passed |

Every emitted QtTest summary reports zero failures and zero skips. No
AddressSanitizer/UBSan diagnostic was observed. Instrumentation remains
scoped: host and Worklets fixture translation units and links use ASan;
React Native, Hermes, Qt and some native archives are not all instrumented,
and leak detection is disabled. This is not a whole-graph sanitizer or QNX
qualification claim.

The exact selected names are in [tests.txt](../evidence/pr149-stack-integration-2026-11-16/tests.txt).
From an equivalently configured host graph, the commands are:

```sh
cmake --build --preset linux-host-renderer --parallel 3
ctest --preset linux-host-renderer --parallel 1 \
  --tests-from-file "$PWD/docs/evidence/pr149-stack-integration-2026-11-16/tests.txt" \
  --output-junit /tmp/pr149-stack-ctest.xml
```

The [build log](../evidence/pr149-stack-integration-2026-11-16/build.log),
[CTest summary](../evidence/pr149-stack-integration-2026-11-16/ctest.log),
[complete test output](../evidence/pr149-stack-integration-2026-11-16/last-test.log)
and [JUnit output](../evidence/pr149-stack-integration-2026-11-16/ctest.xml)
retain the actual commands, diagnostics and results. The
[source inventory](../evidence/pr149-stack-integration-2026-11-16/source-validation.json)
records both parent identities, all 46 changed source/pin hashes, selected
CTest commands/properties and toolchain switches. Those hashes were checked
again after native execution and still match.

## Light checks and retained limits

The following checks pass on the combined source:

- `pnpm typecheck` ([log](../evidence/pr149-stack-integration-2026-11-16/typecheck.log)).
- `pnpm validate:dependencies` ([log](../evidence/pr149-stack-integration-2026-11-16/dependencies.log)).
- `pnpm validate:architecture` ([log](../evidence/pr149-stack-integration-2026-11-16/architecture.log)).
- `node tests/js/host-renderer-contract.test.mjs` ([log](../evidence/pr149-stack-integration-2026-11-16/host-contract.log)); this also verifies the ON/OFF inventory contract.
- `node tests/js/worklets-frame-probe.test.mjs` ([log](../evidence/pr149-stack-integration-2026-11-16/worklets-frame.log)); this check's host-VM scope remains separate from the linked native fixture above.
- `pnpm validate:docs` after normal regeneration ([log](../evidence/pr149-stack-integration-2026-11-16/docs-validation.log)).
- `pnpm format:check` ([log](../evidence/pr149-stack-integration-2026-11-16/format-check.log)).

The prior [61/61 native run](../evidence/pr149-playground-errors-2026-11-16/README.md)
belongs to the earlier Worklets source. The
[149-case guarded SVG experiment](pr161-layout-dispatch-2026-11-16.md)
belongs to its separately identified copied graph. Neither is relabeled
as a full run of this final merge. The focused integration checks do not
repeat the full corpus. The full JavaScript runner was also not rerun;
its previously recorded Marathon Unix-socket `EPERM` limit remains.

The final published head needs its own required hosted workflows. Dedicated
JavaScript-thread work, busy-JavaScript rendering, remaining recognizers,
layout/scroll qualification, a fresh clean combined QNX build and current
device acceptance remain open. Feature switches remain OFF by default,
all eleven release gates remain pending, and this host receipt does not
promote the draft PR or certify a QNX device.
