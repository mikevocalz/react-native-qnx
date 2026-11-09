## Qt Code Review Report

**Scope**: diff: `git diff 3b414f185ec9b6c23eb7fbfdbc0afc9bfed18b94 87793906dae1653f7ceca57097b51192f431c7aa -- tests/cpp vnext`
**Files reviewed**: 26 C++ files, 2,751 added or modified lines
**Issues found**: 2 confirmed during review (0 final lint findings, 2 deep-analysis findings; both corrected and rechecked)
**Agents**: 2 shards: native tests and native implementation; final test-delta review repeated after integration. Cross-shard checks are best effort; the integrator traced the shared paths.
**Framework mode**: off

The qualified local source tree is also published as commit
`56093f7eaab414f206cd8f679abb36b0c2caea6c`; its Git tree exactly matches the local review
candidate. The main receipt records this publication mapping.

Official Qt `qt-cpp-review` version 3.0, pinned source
`062f7c9f0e16b7d2f66cd55e449a3ed85fba699e`. Phase 1 ran the skill's
`qt_review_lint.py --summary --diff=3b414f185ec9b6c23eb7fbfdbc0afc9bfed18b94`.
Phase 2 applied A1–A6 to numbered source diffs. The initial small snapshot used
inline review; once the scope exceeded 2,500 changed lines it was split into the
required two shards. Review was read-only; authorized implementation fixes were
made separately, followed by source rechecks and the native execution matrix
recorded in the [main receipt](adaptive-window-layouts-2026-11-09.md).

---

### Lint findings

| Rule                    | Count | Locations    | Mitigation                                |
| ----------------------- | ----- | ------------ | ----------------------------------------- |
| Final changed-line scan | 0     | 26 C++ files | No remaining deterministic lint findings. |

Earlier snapshots reported macro-safe `min`/`max` spelling, explicit optional
initialization, copy initialization and modal boolean-parameter naming. These
were corrected before the final scan. The final retained lint output is included
with the validation evidence; the original upstream font license bytes were not
modified to satisfy whitespace checks.

---

### Deep analysis findings

#### [D-001] Duplicate-DPR assertion ran after its listener was removed

- **File**: `tests/cpp/QtWindowMetricsTest.cpp:198`
- **Category**: A4 — API, Naming & C++ Correctness
- **Confidence**: 100/100
- **Finding**: The original test removed its second Dimensions listener before
  dispatching an unchanged-DPR event, then checked that the listener's counter
  did not increase. The assertion could pass even if a duplicate event was sent.
- **Trace**: `window-metrics-probe.js` increments `secondEvents` only in that
  subscription. The first explicit listener was already removed. The C++ test
  called `__removeSecondMetricsListener()` before the counter check.
- **Mitigation**: **Corrected and rechecked.** Keep the second listener live,
  send the DPR event, drain 20 microtask/native-event pump turns, assert the
  counter is unchanged, and remove the subscription afterward. The final review
  verified that `host.pumpEvents()` processes the observer's zero-delay timer.

#### [D-201] Sparse animation updates cleared native identifiers

- **File**: `vnext/src/fabric/QtQuickMountingLayer.cpp:386`
- **Category**: A4 — API, Naming & C++ Correctness
- **Confidence**: 98/100
- **Finding**: Newly forwarded `testID` and `nativeID` were assigned on every
  `updateStyle` call. Sparse NativeAnimated opacity/transform updates created an
  otherwise default ViewStyle, so they cleared existing identifiers even though
  React had not removed those props.
- **Trace**: `QtSchedulerDelegate::schedulerShouldSynchronouslyUpdateViewOnUIThread`
  → adapter `synchronouslyUpdateViewOnUIThread` → `viewStyleFromDynamic` →
  `updateStyle`. Complete Fabric updates came through `toViewStyle` and needed
  to retain the ability to clear identifiers intentionally.
- **Mitigation**: **Corrected and rechecked.** Both fields now use explicit
  optional presence. Complete Fabric updates engage them, including empty
  strings for removal; sparse updates leave them absent. The adapter-host
  regression sends real sparse opacity and transform deltas, verifies both
  their visual effects and identifier preservation, then checks that full
  Fabric prop removal clears both identifiers on the same native item.

---

### Investigation targets (human verification needed)

No additional source-level investigation targets met the skill's confidence
threshold. Physical QNX, real display changes, system text preferences, IME,
assistive technology and Marathon/Wayland remain separately unqualified runtime
lanes, as recorded in the main receipt. This source review does not promote them
to passes or constitute human/vendor approval.

---

### Summary

| Category                           | Lint  | Deep            | Investigate | Total |
| ---------------------------------- | ----- | --------------- | ----------- | ----- |
| A1 — Model Contracts               | 0     | 0               | 0           | 0     |
| A2 — Ownership & Lifecycle         | 0     | 0               | 0           | 0     |
| A3 — Thread Safety                 | 0     | 0               | 0           | 0     |
| A4 — API, Naming & C++ Correctness | 0     | 2               | 0           | 2     |
| A5 — Error Handling & Validation   | 0     | 0               | 0           | 0     |
| A6 — Performance & Code Quality    | 0     | 0               | 0           | 0     |
| **Total**                          | **0** | **2 corrected** | **0**       | **2** |

No confirmed finding remains open in the reviewed candidate. Findings below
confidence 60 were suppressed. Native behavior and sanitizer scope are reported
with the executed tests, separately from this source review.
