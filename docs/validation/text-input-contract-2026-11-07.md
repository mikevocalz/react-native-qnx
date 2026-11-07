# Proposed bounded TextInput implementation slice

Status: source audit and implementation contract. No production source,
dependency, bundle or native build changed. Base is main
`be71bc99e4664825f6c409e0704e57e66512375e` in the isolated
`codex/text-input-contract` worktree. The canonical audit also inspected
`d3648112dcdc1bfbe78e1d59c10fe89ba3379fa9`; the relevant TextInput sources match.

## Existing path and concrete gaps

Metro's QNX resolver maps RN's `Libraries/Components/TextInput/TextInput.js` to
the platform override. It renders RN 0.88.1's portable single-line Fabric
component; `QtReactHost.cpp` registers its descriptor. The mutation adapter
applies text traits and connects native events; the QML wrapper contains Qt
Quick `TextInput` and `TextEdit` children.

`tests/cpp/QtReactHostHostTest.cpp::textInputMounts` already sends a synthetic
`QKeyEvent` through a window into this real React component. It asserts ASCII
change/changeText, focus, selection, Enter and blur. Placeholder and multiline
have separate basic host tests. This is useful existing proof; another ASCII
property/event test would not close the missing contract.

The inspected gaps are:

- The platform override uses `React.useState` at lines 26/36. Zustand has no
  manifest/lock entry in this repository. The owner requires per-instance
  Zustand component state and native handles in refs.
- `ShadowViewMutationAdapter::applyTextInput` ignores native edit acknowledgments
  and selection. `TextInputStyle` has neither field. The mounting layer writes
  a differing JS text value regardless of `mostRecentEventCount`.
- The adapter has separate incrementing counters for change, focus, key, submit
  and selection, and observes `textChanged`, including programmatic writes.
  RN's selected native implementation uses one current native edit count,
  suppresses JS-originated feedback and rejects mismatched update tokens.
- The selected native component advertises `setTextAndSelection`; the adapter
  handles focus/blur but not that command.
- The QML selection bridge observes cursor-position changes, not every selection
  endpoint change. The existing ASCII test expects position 5 after `typedX`;
  new characterization must compare the JS selection with the actual child
  cursor/range rather than preserve a possibly stale snapshot.
- Deleting an item destroys its QObject connections immediately, but leaves its
  tag in `textInputConnected_`. Reused tags can therefore skip reconnection.
- A secure single-line field uses Qt Password echo mode. Multiline `TextEdit`
  has no corresponding mask/hint binding. Qt 6.12's Password-mode input-method
  queries can return real surrounding, selected, before-cursor and after-cursor
  text. Masking does not establish provider privacy.

The playground and host test currently contain identical 2,390-byte TextInput
QML blocks, SHA-256
`beaa92207ce63f6ea346df93669cadb63fe3ba48e3b26304798c4a807edf2ff2`.
The new probe must reuse the same definition rather than add a third copy.

## Proposed native editing contract

Keep standard TextInput props/events/commands. This adds no application-facing
Nitro API. Implement one Qt-owner editing state per live mounted field, shared
by the adapter's event path and the mounting layer's text/selection application.
It owns a native edit count, JS-application guard and lifetime generation;
QObject/QPointer/native handles stay native, outside Zustand state.

1. Initial and subsequent JS text writes do not increment the native edit count
   or emit native change feedback. Only committed native edits advance it, once
   per emitted native change. Focus, key, submit and selection observations use
   that current count without independently incrementing it.
2. Apply the text/selection part of a JS update only when its count matches the
   live native count. A mismatched token leaves both unchanged. Unrelated
   geometry/style props can still apply. Follow the inspected RN equality
   rule; do not replace it with a generic Promise or invented acknowledgment.
3. Implement the existing command shape
   `setTextAndSelection(eventCount, valueOrNull, start, end)`. Null leaves text
   unchanged; the paired `-1, -1` selection sentinel leaves selection unchanged.
   Inspect and preserve RN's behavior for invalid/out-of-range values before
   coding it; validate malformed transport argument types without an unchecked
   `folly::dynamic` conversion. No text is included in failure diagnostics.
4. Selection positions are UTF-16 code-unit offsets. UTF-8 is only text
   transport. Use real Qt input/selection APIs; do not count UTF-8 bytes or
   graphemes as RN selection offsets.
5. For ordinary fields, characterize Qt's actual preedit, commit, replacement
   and cancellation events. Uncommitted preedit is not a committed text change.
   Do not add a fake keyboard provider. Blur/unmount policy must be tested
   against actual Qt behavior before changing composition semantics.
6. Invalidate the native generation and remove the tag connection entry on
   deletion. Destroy connections before accepting a replacement field. Pending
   events addressed to a deleted QObject cannot mutate its replacement.
7. Secure multiline remains explicitly unsupported and fails before displaying
   a plaintext editor. Secure input-method commits/queries remain disabled and
   fail closed until an owned adapter can redact sensitive query values. The
   proposed bridge must reject that route, not merely set Password echo mode.
   Ordinary physical-key secure editing is a separately tested behavior; no
   software-provider, clipboard or accessibility privacy claim follows.

A focused `QtTextInputBridge` is the proposed native owner for the edit token,
JS-write guard, safe item reference and connections. It must not retain an RN
runtime or invoke callbacks under locks. Prefer Qt-owner confinement rather
than a new worker or mutex. Its exact interface is to be reviewed before code.

## Scoped path ownership

| Owner                | Proposed paths / change                                                                                                                                                                                                                              |
| -------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| RN platform DRI      | `packages/react-native-qnx/Libraries/Components/TextInput/TextInput.js`: replace only this component's `useState` with a retained vanilla Zustand store and primitive selectors; preserve props/event forwarding. Keep any native instance in a ref. |
| Dependency DRI       | Platform package and test-consumer manifests plus `pnpm-lock.yaml`: add only the audited exact Zustand release, then verify frozen resolution. No RN/React/Metro/Nitro upgrade.                                                                      |
| Qt/RN editing DRI    | New focused `QtTextInputBridge.{h,cpp}`; `ShadowViewMutationAdapter.{h,cpp}` and `QtQuickMountingLayer.{h,cpp}`: one native edit token, JS-write suppression, standard command, selection and teardown.                                              |
| Qt component DRI     | Shared TextInput QML source reused by playground, host tests and new probe; preserve Yoga-owned geometry and established window dimensions.                                                                                                          |
| Conformance DRI      | New isolated TextInput fixture/probe and CMake registration; selected host cases. State is per-instance Zustand; no unrelated React state migration.                                                                                                 |
| Independent reviewer | Different Qt/controlled-edit reviewer checks token ordering, UTF-16 ranges, secure query rejection and generation cleanup. Human assignment remains open.                                                                                            |

New experimental native behavior uses a default-OFF, narrowly documented build
switch until its target gate passes. Do not imply that the switch installs an
OS IME. Existing basic editing and the original QNX path remain available OFF.

## Deterministic real-component fixture

The fixture imports the actual platform TextInput and mounts it through the
existing AppRegistry/Fabric/Hermes/Qt route. Native tests send actual Qt key and
input-method events to the live focus item/window; they do not call JS event
handlers or set a QML `text` property as a substitute for an edit.

Use the synthetic string `A😀e\u0301`: five UTF-16 code units, eight UTF-8 bytes.
Typing `X` produces six UTF-16 units and count 1. Preedit `に` leaves committed
text/count unchanged. Committing it produces seven UTF-16 units and count 2.
A stale count-1 text/selection command changes neither value nor cursor. A
fresh count-2 command selecting `[1,3]` selects the emoji without a new native
change or JS-origin echo. A subsequent native commit replacing that selection
must yield the expected string, range and next count. Explicit cancellation
clears preedit while preserving committed text/count. Check actual child
selection/cursor against the JS observations after bounded RN/Qt delivery.

Separate cases exercise two independent fields; focus/blur tokens; controlled
JS replacement without feedback; secure query/commit rejection; and an event
queued to an old field before actual React surface teardown. Require the old
QPointer to clear before processing the queued Qt event. Restart and verify a
live field still edits. A forced tag-reuse native bridge test is separate from
React's own tag allocation; do not call it a physical IME session.

A causality control uses the existing `Scheduler::addEventListener` technique
from `QtInputLifecycleProbe.cpp` to consume native change events before they
reach JS. The same binary/bundle must then fail with an exact missing-JS-change
diagnostic, no success marker. Strict negative wrappers require the expected
exit/diagnostic and reject crashes, missing fixtures, timeouts and sanitizers.

Run host tests with supported sanitizers and OFF/ON configurations, compile the
exact QNX graph/HBC, then run positive/negative target probes with an external
owned-process watchdog and artifact readback. These are synthetic Qt-event
checks on a physical target. Real Pi keyboard/HID, language engine, software
keyboard geometry, assistive reader output and timing budgets remain separate.
No test or build has been run for this proposal.

## Zustand source check for the later dependency change

Read-only npm metadata on 2026-11-07 selected **candidate** `zustand@5.1.0`.
The registry tarball was read in memory and its SHA-512 SRI verified, without
installing, extracting to disk or changing a lockfile:

`sha512-ciJZlP48M1PTQbVQq81CGVFxHTLpZNrFjKqwjtCU34AWuXTZT/jrF3p9+fI84Cf1eSCMLapxqBy/SNTfwOBuqA==`

The archive was 27,169 compressed bytes; declared unpacked size 111,698. The
published package has no direct dependencies; React, types, Immer and the
external-store shim are optional peers. React's declared peer range is
`>=18.0.0`. This range admits the pinned React 19.3.0 but does not prove this
platform's Metro/Hermes behavior.

Published `vanilla.js` SHA-256 is
`15dcb1e8a40b51ef580fbb27e05bac872ff0e71f2f2ad4d316d7fb344319453e`;
`react.js` is
`19b688f2d3adf775f55af8135872d015ed78885352e348fd6956f17318e10d91`.
The inspected runtime implements `createStore`, `useStore(api, selector)` and
React's `useSyncExternalStore`, with native-aware CommonJS export conditions.
Use one vanilla store retained in a ref per component; select text and count
separately. Do not put item handles in that store or persist editing secrets.

The npm metadata supplied no `gitHead`. GitHub main's cached manifest showed a
different version, so it is not substituted for the verified published source.
No dependency is approved as installed or qualified until the exact lockfile,
resolver, bundle and native fixture pass. No broader state rewrite is proposed.

Sources: installed RN 0.88.1 Fabric TextInput props/events and
`RCTTextInputComponentView.mm`; licensed Qt 6.12 source provenance archive
`98ff4f44bac6ec3e1e62ee2a4316ae0e3d15badb015d753cc0268a20db52f165`;
[Qt input-method events](https://doc.qt.io/qt-6/qinputmethodevent.html),
[Qt TextInput](https://doc.qt.io/qt-6/qml-qtquick-textinput.html), and the
[immutable published Zustand tarball](https://registry.npmjs.org/zustand/-/zustand-5.1.0.tgz).
The existing native/API/validation skills and pinned source pipeline informed
this contract; `no-ai-slop` was applied to the prose. No actual credentials,
private user text, vendor source or licensed runtime were copied.
