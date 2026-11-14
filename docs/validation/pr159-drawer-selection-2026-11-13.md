# Clear the previous drawer selection

Implementation: `cf43271`, continuing PR 159. The owner reported two selected
rows in the open drawer. The live capture showed Home and Components with the
same teal fill after navigating Components → Home. Home was active; Components retained
its old painted background.

RN 0.88.1's C++ HostPlatformColor considers a color meaningful only when its
alpha is positive. The adapter used that predicate to decide whether to update
the native background. Changing a row to transparent skipped the update and
left its old fill. Generic React Views now receive transparent for a cleared
or omitted background; native controls retain their own unset defaults.

The new real-ViewProps mutation regression checks opaque → transparent →
opaque → omitted. It fails on the old adapter and passes with the fix. All
eight mounting-adapter host cases pass in the local Linux/Qt 6.12 graph with
this isolated adapter delta. An initial attempt to transplant the whole newer
adapter into that older graph failed on its missing Modal bridge; the isolated
delta avoids mixing those interfaces. The broader transplanted Hermes/Fabric
suite still has its two separately recorded pixel/Modal fixture failures.
No full-host or sanitizer pass is claimed for this local run.

The same commit fixes the independent review's interrupted-swipe finding.
Actual animation progress drives drag positioning, while logical starting
state controls the release threshold. Gesture values live in the existing
per-instance Zustand store. Three interrupted-animation cases preserve state;
the first fails before the fix. The four router tests pass in shared-runner
order and typecheck passes. CodeRabbit reviewed the final four-file delta
against 2012cdf with zero findings after the initial finding was addressed.

## Pi 400 check

The QNX SDP 8.0.5 AArch64 consumer rebuild passed. It uses the disclosed
inherited O1/native recipe and upstream object reuse; public Qt 6.12.0 is O2.
The clean combined-build requirement remains open. No window dimensions, boot,
window manager or USB configuration changed.

The native binary is
`8538fcaecbe022d9a586fc8b7e09d12a4a08333565e86219eaed1476d2f424c5`.
The frozen final HBC is
`295bec66e55ba95d28b4139e7df8a163de4e2531e05f68d40a32a19b27ac4741`.
Native keyboard navigation visits Components, returns Home, then reopens the
drawer. Each destination has one teal row. The final pair repeats that check;
one showcase process and the USB input endpoint remain after testing.

`docs/evidence/drawer-selection-2026-11-13/` contains the original two-fill
capture, both corrected destinations, final-pair capture and regression logs.
Captures are the native 1280x800 window region converted from BMP without
resizing. Intermediate and final HBCs are distinguished in the manifest.
This is a custom-image Pi 400 result. Stock QSTI, Marathon, QNX VM and timing/
memory qualification remain separate. Exact-head CI must pass before merge.

DRI: current integration agent; ownership is the adapter, drawer gesture
state and their regressions. Human review is unassigned. Repository native API/
validation skills, the pinned Margelo api-design → build-nitro-modules → cpp
pipeline and no-ai-slop were applied as recorded in the preceding runtime
receipt. No Nitro API, dependency, permission, premium payload or licensing
change is introduced. Performance deltas are unmeasured.

Rollback: revert cf43271 and rebuild the native consumer/HBC together using
the guarded launcher. Retain the failing capture and regression evidence;
reverting restores the stale-fill behavior.
