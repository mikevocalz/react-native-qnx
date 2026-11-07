# TextInput keyboard traversal: host and physical QNX receipt

The opt-in TextInput contract now gives each editable wrapper one Qt tab stop.
Host tests pass for forward and reverse traversal, eligibility changes and React
event delivery. Physical QNX native and React trials also passed. The
[earlier TextInput receipt](text-input-qnx-2026-11-07.md) remains attached to its
own candidate and artifacts.

## Candidate and behavior

Canonical source is `ff548dbeef45cceff4db986f96264db6701e7125` on
`codex/text-input-focus-integration`, cherry-picked from runtime source `9317516`.
The six-path change extends the existing bridge, shared QML, React fixture and
native/React probes. It adds no dependency or public native API.

In contract mode, the live editable FocusScope joins Qt's tab chain. Its child
editors remain outside that chain to avoid duplicate stops. Read-only or
invalidated sessions release active focus before removing their stop. Qt skips
hidden and disabled wrappers. Backtab maps to the same RN Tab key value as Tab;
traversal leaves text and native edit tokens unchanged. The default-OFF path
retains its original QML and editing behavior.

## Actual checks

| Gate                                         | Result and boundary                                                                                                                                                                                   |
| -------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Baseline characterization                    | Failed before the production change: `Tab did not reach second editor`. The test dispatched a Qt key event through an actual window with two independently owned FocusScopes.                         |
| Standalone native bridge, ASan/UBSan         | Passed 1/1 after the change. Covers Tab/Backtab, one wrapper stop, read-only/disabled/hidden skipping and invalidation.                                                                               |
| Fresh focused host React/native/control      | Passed 3/3: React TextInput probe, native bridge and strict interception control. React checks native focus targets, focus/blur callbacks, Tab key mapping, unchanged text and unchanged edit counts. |
| Full JavaScript suite                        | Passed.                                                                                                                                                                                               |
| Compiled OFF QML identity                    | Passed; the preserved component remains 2,390 bytes with its recorded hash.                                                                                                                           |
| Follow-up QNX cross-build                    | Passed with the new production bundle compiled to HBC.                                                                                                                                                |
| Physical QNX native bridge                   | Passed, exit 0: actual-window Tab/Backtab, read-only/disabled/hidden skipping, dynamic read-only and invalidation checks.                                                                             |
| Physical QNX React positive/control/recovery | Passed: positive 0, interception 1 at `RNQNX_TEXT_INPUT_FAIL JavaScript TextInput change count mismatch`, recovery 0. The unchanged full success marker follows the new focus assertions.             |
| Readback and cleanup                         | All three artifacts matched readback. Original application PID `9433114` remained alive before, during and after all gates; no probe/supervisor child remained.                                       |

Host sanitizer results do not establish whole-dependency instrumentation or
target sanitizer coverage. These trials use synthetic Qt events; no physical
keyboard/HID, language engine, software IME or assistive technology session is
claimed. No performance or presentation measurement was made. QNX VM and
Marathon/Wayland were not run for this follow-up.

## Reproduce and review

Use the existing configured, licensed host dependency graph:

```sh
cmake --build build/macos-full-host-tests --parallel 4 --target rnqnx_text_input_probe rnqnx_text_input_bridge_test
ctest --test-dir build/macos-full-host-tests -R '^rnqnx_text_input_' --output-on-failure
```

These are reproduction commands; this documentation edit did not rerun builds.
The runtime agent ran each physical trial under a 35-second supervisor in
`textinput-focus-20261107-verified`, using the existing private Qt environment.
Raw local logs are `/Users/mikevocalz/qnx-pi400-8.0.5/text-input-focus-physical.log`
and `/Users/mikevocalz/qnx-pi400-8.0.5/text-input-focus-qnx-build.log`. Nonfatal Qt
event-parameter deprecation and null-instance teardown warnings remain in the
logs. Previous PR70 artifacts were preserved.

The following local files and corresponding `-readback` files were hashed
directly; every pair matched:

| Artifact                      |   Bytes | SHA256                                                             |
| ----------------------------- | ------: | ------------------------------------------------------------------ |
| `text-input-focus-probe.qnx`  | 6113240 | `f1275ea9e478f64889ad9993d44564256f96c96f4f12a51051dfe728d992a36d` |
| `text-input-focus-probe.hbc`  |  715548 | `9271ff5b4c7054f94559f17a6a26b41a2c4938b1b06d425e8fbc457823c37b56` |
| `text-input-focus-bridge.qnx` |   75264 | `0edf57c84d203d987fc3063b5a6d98452cb806679f4d6fa63dcfa1586f4e5b44` |

DRI is `/root/runtime_harness`; independent review is the child source review
and `/root` integration review. Both reviewed the source change. Repository
`qnx-native-api` and `qnx-platform-validation` guidance keeps ownership and
host/target evidence separate; `no-ai-slop` informed this prose. No credentials,
private device addresses, user text or licensed vendor contents belong in this
shared receipt. Existing window dimensions, orientation, boot configuration,
privileges and evaluation-license terms remain unchanged.

Rollback reverts this focus follow-up or disables
`RNQNX_ENABLE_TEXT_INPUT_CONTRACT`, then redeploys the recorded known-good
application with its matching HBC. Keep the earlier TextInput evidence and
artifacts intact.
