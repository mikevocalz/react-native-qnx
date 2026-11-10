# Modal keyboard focus and controlled Switch

Status: implemented in PR #110. Modal focus defaults OFF; controlled Switch
reconciliation is always enabled and requires the matching native rebuild.
The original author reported passing Linux native tests at `083c4bd`. The
PR #108 integration passed 47/47 enabled and 25/25 disabled local native tests.
The native Modal, controlled-Switch and public View-focus tests passed in the
enabled run; public View focus also passed in the disabled configuration.
Published-head CI is pending. See the
[validation handoff](validation/pr110-merge-readiness-2026-11-10.md) for the
revision, flags and remaining target checks.

## Modal focus (`RNQNX_ENABLE_MODAL_FOCUS`)

`cmake/RnqnxModalFocus.cmake` defines `RNQNX_ENABLE_MODAL_FOCUS` (default
`OFF`). Configuring it `ON` for a system other than Linux, Darwin or QNX fails.
When `OFF`, the adaptive `QtModalBridge` owns initial focus, Tab/Backtab,
Escape, background keyboard exclusion and nested focus restoration within its
surface. The existing native pointer barrier remains active.

When `ON`, `QtQuickMountingLayer` registers each rooted, mounted Modal with one
`ModalFocusOwner` per `QQuickWindow`. Several Fabric surfaces in one window
share that owner.

The adapter's modal bridge then handles state and viewport sizing only; it
does not install a second focus filter. The window owner performs one queued
refresh after mounting so controls that become tabbable after insertion can
receive initial focus. Focus on the empty modal root is a fallback; an explicit
focus choice inside the modal is preserved. Queued work is cancelled with the
window owner and consults current modal entries after removal.

- Top Modal: the visible, enabled Modal highest in the Qt Quick stacking order,
  whose logical Fabric ancestors are also visible and enabled.
- Keys: key, shortcut-override and input-method events for items outside the
  top Modal are consumed. Focus is moved back into the Modal.
- Tab and Shift+Tab: move through the top Modal's focus chain and wrap within
  it.
- Escape: one `onRequestClose` per physical press, sent to the top Modal only.
  Auto-repeat and the matching release are consumed. A release that follows a
  close does not reach the Modal underneath. Visibility stays controlled by
  React.
- Nested Modals: closing the top Modal restores the last focused item of the
  Modal below it.
- Restoration: the item focused before the first Modal opened is restored only
  if it still exists, is in the same window, and is visible and enabled.
  Otherwise focus goes to the window content item.
- Window deactivation: key input is consumed while a Modal is open and the
  window is inactive. Activation reapplies Modal focus.
- Teardown: Modal entries are removed before text-input callbacks are closed
  and items are deleted. Destroyed items and windows are tracked with
  `QPointer` and `destroyed` signals.

Not changed: pointer, touch and wheel barriers, visual portalling to the
surface root, logical Fabric parent order, Yoga geometry, fullscreen startup.

## Controlled Switch

Follows React Native 0.88.1 `Switch.js`: after a native change the override
calls `onChange`/`onValueChange`, records the native value, and in a layout
effect calls `SwitchNativeComponent.Commands.setValue(ref, value)` when the
native value differs from `props.value`. The adapter handles `setValue` with
exactly one boolean argument. It writes the QML value without emitting an
event. Malformed arguments and unmounted tags return a `MutationFailure`.

QML emits `toggled()` only for user toggles, so prop and command writes cannot
echo back to JavaScript. Disabled Switches do not toggle.

## Tests and evidence

- `tests/cpp/QtModalFocusTest.cpp`: requires `RNQNX_ENABLE_MODAL_FOCUS=ON`.
- `tests/cpp/QtViewFocusHostTest.cpp`: exercises public React/Fabric initial and
  nested focus on the combined adaptive host.
- `tests/cpp/QtControlledSwitchTest.cpp`
- `tests/js/qnx-controlled-switch.test.mjs`: runs the override with React and
  native doubles. It does not test Fabric command delivery.
- `apps/rn-playground/consumer-input-probe.js`: background/Modal/nested
  TextInputs, rejected and deferred Switches, and close-request counters for
  a production-window driver.

At implementation revision `083c4bd`, the [PR author reported](https://github.com/mikevocalz/react-native-qnx/pull/110)
23/23 Linux CTests with Modal focus, development runtime, frame trace and
WebSockets ON, and 17/17 with those options OFF. That run used clang 14,
Qt 6.11.1 and ASan/UBSan; two unrelated targets were excluded for an existing
clang 14/libstdc++ 11 compile incompatibility. The author also recorded an
ASan kernel-setting requirement. These are original-author results, with
their limitations retained in the validation handoff.

During the 2026-11-10 integration follow-up,
`node tests/js/qnx-controlled-switch.test.mjs`, the host-renderer inventory
test and `pnpm typecheck` passed. The Switch test uses React/native doubles;
it does not establish Fabric command delivery or Qt execution. The updated
candidate still needs its own full ON/OFF native CI result.

The later PR #108 integration passed an initial focused 9/9, followed by full
47/47 ON and 25/25 OFF runs. These include the queued initial-focus regression
and public View focus under both ownership paths. The initial 44/47 full run's
LeakSanitizer ptrace-host failures are retained alongside the process-local
leak-disabled successful rerun. The raw evidence and artifact/source hashes are in the
[PR #108 integration receipt](validation/pr108-integration-2026-11-10.md).

The [owner's smooth QNX report](evidence/gallery-owner-report-2026-11-10.md)
is retained without assuming the installed artifact hashes, feature flags or
tested actions. Physical keyboard, IME and touch behavior need a named
scenario receipt on the matching rebuilt QNX consumer. Host results do not
qualify those device behaviors.

## Rollback

Configure with `RNQNX_ENABLE_MODAL_FOCUS=OFF` (the default). The Switch
command path is not behind a flag; to roll it back, revert the Switch files
listed in the patch.
