# Generic View accessibility: host and physical QNX receipt

Generic View metadata now reaches Qt's queryable semantic tree through public
`QQmlProperty` writes to `Accessible.*`. Host semantic-query and owned mounting
checks passed, including bounded physical QNX semantic-query trials. This is a bounded Plan 04
subset, not complete React Native accessibility support.

## Candidate and boundary

Canonical implementation is `cbdfd1833a844411f5637355bba7d0bc4f819f5e`,
cherry-picked from `e8c344d`. The nine-file change adds
`QtViewAccessibility`, a native semantic-query fixture and CMake integration;
it updates the generic View mounting path and RN prop conversion. No dependency,
renderer, public Nitro API or application state store was added.

`RNQNX_ENABLE_ACCESSIBILITY_BRIDGE` defaults OFF. Its PUBLIC compile definition
keeps the guarded two ViewStyle state booleans and mounted generic-View flag
consistent across consumers. ON requires the full Qt/React graph and Qt
accessibility support. The helper requires the item's Qt owner and a live QML
context; it hides the node before applying metadata and leaves it hidden on
unsupported roles or update failure.

The implemented role subset is none/default, button, text, header, image,
checkbox, radio and switch. Name, description, selected, checked, checkable and
mixed state update through Qt attached properties. Disabled state derives from
the actual item's enablement; this helper does not change input enablement to
match accessibility-only metadata. Clearing accessibility removes semantic
metadata/exposure. Native tests query parent/child order, updates, rejection,
re-exposure and deletion through `QAccessible`.

The bridge applies only to generic View. TextInput, value semantics, RN
actions and end-to-end reader output are excluded. Qt's default role interfaces
may advertise action names whose RN actions are unwired; advertised names do
not establish action delivery. The fixture is a native GUI semantic query,
not proof that React JavaScript accessibility props traverse Fabric correctly.

## Actual checks

| Gate                                     | Result and scope                                                                                                                                                                                                                                         |
| ---------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Baseline characterization                | Failed to expose the required semantic metadata through dynamic item properties before the bridge change.                                                                                                                                                |
| Standalone native semantic fixture       | Passed 1/1 with host ASan/UBSan. Queries actual Qt interfaces rather than checking property existence alone.                                                                                                                                             |
| Full graph mounting compile and fixture  | Passed 1/1. Exercises owned `QtQuickMountingLayer` creation, metadata application/clearing, exclusion of a non-View component, and deletion/interface removal.                                                                                           |
| All-options-ON host suite                | Passed 27/27; raw local log `/tmp/rnqnx-a11y-full-ctest.log`.                                                                                                                                                                                            |
| Fresh all-options-OFF host suite         | Passed 16/16 after a fresh all-options-OFF build at `cbdfd18`; `/tmp/rnqnx-a11y-off-ctest.log`.                                                                                                                                                          |
| Unsupported ON configuration             | Missing Qt/accessibility or full host graph is rejected; configuration rejection checks passed.                                                                                                                                                          |
| Physical QNX semantic query and mounting | Passed positive 0 and recovery 0 with `RNQNX_ACCESSIBILITY_VIEW_OK tree identity state update clear reject delete`. Includes actual factory/layer creation, style/clearing, non-View exclusion and deletion, with `RNQNX_ACCESSIBILITY_MOUNTING_TEST=1`. |
| Target rejection and cleanup             | Unsupported-role and missing-context negatives are internal assertions, not separate external trials. Original application PID `9433114` remained alive; no probe/supervisor child remained.                                                             |

Host instrumentation does not establish whole-dependency or target sanitizer
coverage. No timing, memory-growth budget, assistive output or presentation
measurement was made. QNX VM and Marathon/Wayland were not exercised.

Read-only inventory found QNX Qt accessibility compiled ON and AT-SPI/D-Bus
support OFF. Inspection of the Pi's Qt plugin root found the QNX platform plugin;
no reader was identified in that inspected scope. This does not establish that
the image has no reader or that an OS accessibility transport works.

## Reproduce and review

Use the existing licensed host dependency graph:

```sh
cmake -S . -B build/macos-full-host-tests -DRNQNX_ENABLE_ACCESSIBILITY_BRIDGE=ON
cmake --build build/macos-full-host-tests --parallel 4 --target rnqnx_accessibility_view_test
ctest --test-dir build/macos-full-host-tests -R '^rnqnx_accessibility_view_test$' --output-on-failure
```

These are reproduction commands; this documentation edit did not rerun builds.
The runtime agent cross-built source `cbdfd18` with the accessibility flag ON
using licensed QNX SDP8.0.5, GCC12.2 `q++` and private Qt6.12. Both physical
trials ran as `qnxuser` under a 35-second supervisor in
`accessibility-view-20261107-verified`. The original application PID `9433114`
remained alive before each trial and afterward. Earlier artifacts were preserved.

Local logs are
`/Users/mikevocalz/qnx-pi400-8.0.5/accessibility-view-physical.log` and
`/Users/mikevocalz/qnx-pi400-8.0.5/accessibility-view-qnx-build.log`.
The executable `accessibility-view-test.qnx` and downloaded
`accessibility-view-test-readback.qnx` were directly measured at 169,168 bytes
each; both SHA256 values are
`61f7db74214f1e86532c4bd2436eb769acb6aa6aef4e0bd72d825f076f818291`.
No React JavaScript fixture or HBC was part of this native semantic-query test.

DRI is `/root/runtime_harness`; independent child source review approved the
slice, with integration review by `/root`. Hosted CI coverage is being extended;
no CI pass is inferred from local results. Repository `qnx-native-api` and
`qnx-platform-validation` guidance keeps ownership and semantic-tree, React and
reader evidence separate; `no-ai-slop` informed this prose.

No credentials, private device addresses, user text or licensed vendor contents
belong in shared evidence. Existing window dimensions, orientation, boot
configuration, privileges and evaluation-license terms remain unchanged.
Physical HID, IME, accessibility reader output and RN action execution remain
unverified. Production distribution rights do not follow from evaluation runs.

Rollback disables `RNQNX_ENABLE_ACCESSIBILITY_BRIDGE`, rebuilds with consistent
PUBLIC definitions and redeploys the recorded known-good application. Source
rollback reverts this bounded bridge/fixture series. Keep prior TextInput and
focus artifacts intact.

The [read-only blur follow-up](text-input-readonly-blur-qnx-2026-11-07.md) records
deferred focus delivery, teardown regressions and exact PR72 revalidation.
