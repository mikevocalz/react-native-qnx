# PR 149 native SVG event interest — November 17, 2026

The optional Reanimated integration now preserves native SVG layout subscribers
when no React layout prop is present. With both features enabled, the module
installs a per-host, weak-proxy predicate that queries the upstream event registry
for `onSvgLayout` and the native tag. Invalidation clears it before proxy cleanup.
The predicate captures no JSI values; the synchronized lookup briefly locks the
proxy's shared ownership while it runs.

The SVG prerequisite keeps reporters and bounds tracking, admitting dispatch for
either a React listener or current native interest. The real-package fixture
mounts a plain SVG rectangle, then directly registers, removes and re-registers
an actual native Worklets event handler. UI and RN queue barriers establish the
registration boundary. Three fresh hosts verify changed painted geometry,
exact layout payloads, UI-worklet execution, suppressed native dispatch after
removal, and no replay on registration alone. The final host retains a handler
that has delivered events and a demonstrably admitted pending library frame
callback during teardown.

## Source and control evidence

The control uses the previously committed Reanimated module with the new SVG
prerequisites and identical new fixture. It renders the width-20 rectangle, then
fails to receive the late native event. It is a module-integration control; it
does not represent the original whole branch. The first direct run also reaches
a separate LeakSanitizer ptrace limitation. That failure is preserved. The same
control through the unchanged configured CTest produces the expected subscriber
failure without the LeakSanitizer fatal diagnostic.

Restoring the reviewed module passes the provisional two-test pair in 18.74
seconds. The [complete receipt](../evidence/pr149-svg-native-interest-2026-11-17/README.md)
retains both controls, the candidate result, source manifests, commands, raw logs,
JUnit, pixel evidence and instrumentation limits.

## Actual public prerequisite and final checks

The final merge uses [published Expo commit `12cae678`](https://github.com/mikevocalz/react-native-qnx/commit/12cae67885b77f9a371059fc2b0a1168ef07430b),
tree `b057996aeb770ce1b1349077a0106c85d3f4ff4a`, whose parents are
`d50e34d99a504f48b690225ad28e8cb3cd8ca37a` and published SVG
`6f40a8d5a26a987ebfe277b9e7d7a0f051d4d58c`. The local PR 149 parent is
`ff86386a218ceda4e842e79c601fd70c2ff43b7a`, whose tree matches the earlier
published PR 149 head `75782e716d3a17fb7a22c47f883e37585a94f640`.

All 26 source files match the tested provisional candidate byte-for-byte. All
three package/lock/Worklets-CMake hashes and all 25 recorded build settings match.
Native and application source merged automatically. The handoff and readiness
unions preserve both parents' additions; the generated project index was
refreshed. Both independent reviewers approved these bounded resolutions.

| Final actual-parent check                    | Result                      |
| -------------------------------------------- | --------------------------- |
| Ordinary Worklets integration target rebuild | Pass, 72.545 seconds        |
| `rnqnx_worklets_integration_host_test`       | Pass, 17.85 seconds         |
| `rnqnx_worklets_rejects_missing_bootstrap`   | Pass, 2.25 seconds          |
| Configured pair                              | **2/2 pass, 20.10 seconds** |

The rebuilt executable, Reanimated archive and Metro bundle are byte-identical
to the provisional candidate. Test properties and source hashes remain unchanged.
The final run is recorded separately from the provisional evidence in the
[final source comparison](../evidence/pr149-svg-native-interest-2026-11-17/final-source-comparison.json)
and [final test result](../evidence/pr149-svg-native-interest-2026-11-17/final-ctest-result.json).

The existing Debug graph retains `HERMES_SLOW_DEBUG=ON`, its 60-second test limits
and configured `ASAN_OPTIONS=detect_leaks=0:halt_on_error=1`. AddressSanitizer
instruments the test translation unit and link; this does not establish whole
graph, UBSan or leak coverage. No sanitizer diagnostic occurs in either configured
candidate pair. Only the two affected native tests were rerun after this restack.

## Remaining acceptance and rollback

DRI: Worklets integration agent `qnx_readiness`. Independent source, provenance
and final merge reviewers: coordinator `root` and Expo agent `finish_expo159`.
The SVG implementation and its separate 49/49 graph retain their own
[prerequisite receipt](pr161-svg-layout-interest-2026-11-17.md).

The previous published PR 149 head's successful hosted workflows predate this
change. The new published head requires its own checks. This receipt qualifies
the exercised Linux native event path and teardown only. General public hook
compositions, animated SVG props, dedicated JS-thread work, remaining recognizers,
layout/scroll behavior and current physical QNX execution remain unqualified.
No feature default or dependency pin changes. All eleven formal release gate
objects remain unchanged and pending; keep the PR draft.

Rollback can remove the three-file registry/fixture continuation while retaining
the SVG prerequisite's marker and empty-by-default interest seam. Removing that
prerequisite requires coordinating the Reanimated setter references as well.
Rebuild the affected target and refresh the Metro fixture after any source change.
