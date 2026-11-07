# Plan 04 — Text input, keyboard, focus, insets and accessibility

## Publication baseline and Nitro v1 gate

This branch is based on main `1baad40a98362ce151ea73bd6f0a6e1d3020926e`. PRs #42, #44, #45, #46 and #48, plus program #47, are merged at this publication snapshot. Earlier source pins below identify inspected evidence rather than unmerged work. No runtime result is promoted to a new revision without revalidation. The current owner policy keeps premium Forge packages optional and outside core build, conformance and release requirements.

Application-facing native work must follow the owner's [Nitro v1 API-first contract](https://github.com/mikevocalz/react-native-qnx/blob/f06bc134d71fe410207525e0348b2fa2481c369b/docs/platform-modernization/NITRO_V1_API_POLICY.md): actual v1 class/registration/constructor APIs, aligned core/runtime/generator versions, typed operations, explicit errors and cleanup, and owned asynchronous work. API sketches below describe semantics; they are not older-version binding implementations or proof of supported code generation.

## Deliverable and current limits

This PR adds the delivery specification for usable native editing and accessible navigation. It contains no runtime implementation or device result. The goal is a shared React Native screen that can be edited with hardware and software keyboards, survives focus changes without losing text, exposes meaningful accessibility semantics and remains usable when a particular OS service is unavailable.

At main `e84b51430c239bc89dbd48ac35afab89f7c77c01`, [component support](https://github.com/mikevocalz/react-native-qnx/blob/e84b51430c239bc89dbd48ac35afab89f7c77c01/docs/component-support.md) records basic TextInput behavior, controlled-edit event counts and several native commands. It also explicitly records missing IME integration, a no-op KeyboardAvoidingView, zero safe-area insets and accessibility properties that are only QML dynamic properties. These are different support levels. A property being readable in a host test is not accessible output or software-keyboard support.

The [TextInput override](https://github.com/mikevocalz/react-native-qnx/blob/e84b51430c239bc89dbd48ac35afab89f7c77c01/packages/react-native-qnx/Libraries/Components/TextInput/TextInput.js) uses RN's portable text-input descriptor and tracks `mostRecentEventCount`. Its opening comment contains older gaps than its event forwarding; inspect actual code and tests before rewriting either. [PR #44](https://github.com/mikevocalz/react-native-qnx/pull/44), at `e9026fac8ce8deb5a8e6ba4c35d1dcc5592d478d`, proves a physical Hello World, not this editing/accessibility matrix.

## What Marathon teaches

Marathon's [InputContext](https://github.com/patrickjquinn/Marathon-Shell/blob/c2c1b2fe32aeaa51edf75a45045cbfbbc08c4ac5/shell/src/services/inputcontext.cpp), at `c2c1b2fe32aeaa51edf75a45045cbfbbc08c4ac5`, sends commits through `QInputMethodEvent`, queries surrounding text and selection, distinguishes text commits from navigation keys, and checks that expected text still precedes the cursor before replacing it. That last check prevents delayed correction from deleting text after the user moves the cursor or types elsewhere.

Transfer those contracts through the existing Qt/RN backend. Do not import a shell-owned singleton or Linux keyboard scan-code table into QNX. Marathon's Wayland text-input route and a QNX input-method/provider route need distinct adapters. The absence of a QNX software keyboard must be reported as a missing capability, not treated as successful keyboard display. QNX hardware-keyboard editing remains a useful independently testable slice.

## Architecture and proposed caller behavior

Keep four states separate: application visibility, native window activation, focused text item and accessibility focus. A modal may move focus while the app remains visible; a keyboard window may take activation without invalidating an editing session. Define stable surface/item generations so a queued IME event cannot target a destroyed or recycled field.

Use Qt's input-method events/queries for preedit, commit, selection, surrounding text, cursor geometry and input hints. Preserve the RN event-count contract across native edits and asynchronous controlled value updates. Specify how composition ends on blur, submit, cancellation and unmount, and verify that behavior against the installed RN/Qt interfaces. Audit text-offset units at every boundary, including UTF-16 indices, UTF-8 transport offsets and user-perceived grapheme edits. Test them; an ASCII-only successful edit is insufficient.

Keep the existing React Native component API as the public surface. These are proposed conformance usages, not proof the missing behavior works today. The implementation test fixture will supply imports and actual test drivers.

```tsx
import { useRef } from 'react';
import { TextInput } from 'react-native';
import { useStore } from 'zustand';
import { createStore } from 'zustand/vanilla';

type EditorState = { text: string; setText: (text: string) => void };
const createEditorStore = () =>
  createStore<EditorState>()((set) => ({
    text: '',
    setText: (text) => set({ text }),
  }));

// Each editor owns its store; selectors subscribe only to the values it needs.
function ControlledEditor() {
  const storeRef = useRef<ReturnType<typeof createEditorStore> | null>(null);
  if (storeRef.current === null) storeRef.current = createEditorStore();
  const text = useStore(storeRef.current, (state) => state.text);
  const setText = useStore(storeRef.current, (state) => state.setText);
  return (
    <TextInput value={text} onChangeText={setText} multiline accessibilityLabel="Document title" />
  );
}
```

The consumer fixture must supply its audited Zustand dependency. Application/component
state stays in Zustand; native resources and imperative handles stay in refs and
are excluded from persisted state. This example adds no platform dependency.

Exercise this field with Japanese composition, Arabic bidirectional text, combining marks and emoji; move the cursor during pending correction, then deliver a delayed React update. Assert the resulting text and selection, not merely that an event fired.

```tsx
// A listener has explicit cleanup; a password never enters diagnostic payloads.
useEffect(() => {
  const shown = Keyboard.addListener('keyboardDidShow', recordKeyboardBounds);
  const hidden = Keyboard.addListener('keyboardDidHide', clearKeyboardBounds);
  return () => {
    shown.remove();
    hidden.remove();
  };
}, []);
return (
  <TextInput
    secureTextEntry
    autoCorrect={false}
    accessibilityLabel="Password"
    onSubmitEditing={submitCredentials}
  />
);
```

The second fixture tests requested behavior after a real Keyboard provider exists. `recordKeyboardBounds`, `clearKeyboardBounds` and `submitCredentials` are fixture-owned callbacks, not new native APIs. Do not emit fictitious show/hide events when no keyboard exists. Unsupported capabilities must be visible in the support matrix and platform capability layer from Plan 06.

Insets must come from actual window/compositor/keyboard geometry. Avoid double-applying keyboard avoidance, safe areas and display cutouts. Plan 11 supplies display and hinge metrics; simulated hinges remain labeled simulation. Yoga computes the resulting layout; no Qt anchors or parallel layout manager may reinterpret it.

For accessibility, connect semantic props/actions to Qt's accessibility interfaces and events, then identify the available OS accessibility transport/consumer. Provide a capability row for semantic-tree support and another for end-to-end assistive-technology output. If QNX lacks an available reader/service on the target image, keep the latter unverified; a host tree query cannot make it pass.

## Ordered slices and ownership

1. Characterize existing TextInput and focus behavior with host regressions, including controlled values, selection, hardware Tab/Shift-Tab, Enter, Escape and focus after modal dismissal.
2. Add composition and surrounding-text handling through a focused native adapter. Preserve secure-field privacy and reject stale target generations. Add hardware editing on physical QNX before depending on a software keyboard.
3. Implement separate QNX and Marathon keyboard providers, real geometry/events and opt-in keyboard avoidance. Record unsupported provider operations explicitly.
4. Connect Qt accessibility semantics: names, roles, states, values, actions, focus events and traversal. Validate dynamic updates and virtualized/recycled nodes.
5. Exercise the whole shared editor in Plan 12: composition, keyboard covering the caret, modal open/close, device rotation/display changes, background/resume and an assistive-technology session where available.

Proposed options `RNQNX_ENABLE_TEXT_INPUT_METHOD` and `RNQNX_ENABLE_ACCESSIBILITY_BRIDGE` must default OFF until their device gates pass; names remain proposals until CMake implements them. Basic existing TextInput remains available with the flags OFF. Plan 01 supplies runtime/lifetime prerequisites; Plans 05, 06, 11 and 13 supply lifecycle, capability, display and Marathon contracts.

Human assignments remain open. Each implementation slice must name a DRI and an independent reviewer; the following roster specifies responsibilities.

| Role                    | Owned paths and evidence                                                                          | Independent review                     |
| ----------------------- | ------------------------------------------------------------------------------------------------- | -------------------------------------- |
| Qt text-input DRI       | `vnext/src/views/qt/QtQuickItemFactory.cpp`, focused new text adapter, corresponding headers      | RN controlled-edit/event reviewer      |
| RN platform engineer    | `packages/react-native-qnx/Libraries/Components/TextInput/TextInput.js`, Keyboard/inset overrides | Qt focus/IME reviewer                  |
| Accessibility engineer  | Proposed focused Qt accessibility adapter and semantic tests                                      | Accessibility QA reviewer              |
| OS integration engineer | `native/platform/qnx/`, `native/platform/linux-host/` provider implementations                    | Security and shell-interface reviewer  |
| Conformance QA          | `tests/cpp/QtReactHostHostTest.cpp`, JS fixtures, separate device receipts                        | Reviewer independent of implementation |

## Skills applied

These exact sources were read for the plan. The table records source guidance, not local CLI installation or executed runtime tests.

| Skill / source                                                                                                                                                                                                                    | Applied decision                                                                                                                                    |
| --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------- |
| [Margelo api-design, `1e9e17be6f41c838db472d6a4b943fc26355a3be`](https://github.com/margelo/react-native-skills/blob/1e9e17be6f41c838db472d6a4b943fc26355a3be/skills/api-design/SKILL.md)                                         | Keep standard TextInput/Keyboard behavior; make availability and listener cleanup explicit rather than inventing platform-prefixed editing methods. |
| [Margelo build-nitro-modules, same revision](https://github.com/margelo/react-native-skills/blob/1e9e17be6f41c838db472d6a4b943fc26355a3be/skills/build-nitro-modules/SKILL.md)                                                    | Separate RN core registry requirements from application native APIs; do not claim a new generated Nitro keyboard provider.                          |
| [Margelo cpp, same revision](https://github.com/margelo/react-native-skills/blob/1e9e17be6f41c838db472d6a4b943fc26355a3be/skills/cpp/SKILL.md)                                                                                    | Own sessions, focus-target generations and callback cleanup; isolate OS adapters from view code.                                                    |
| [Callstack react-native-best-practices, `59e9d17446a248b21d86f078fcd0bd2d36db1ca4`](https://github.com/callstackincubator/agent-skills/blob/59e9d17446a248b21d86f078fcd0bd2d36db1ca4/skills/react-native-best-practices/SKILL.md) | Profile actual input latency and re-render work before changing controlled-component behavior.                                                      |

Any new application-facing service API must additionally have a pinned v1 spec, valid caller/error/cleanup examples and proven platform codegen. RN core's existing registry exception does not authorize unrelated third-party shims. No fabricated accessibility or device-automation skill is recorded.

## Acceptance and future validation

| Area              | Required behavior / negative case                                                                                                                                     |
| ----------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Composition       | Preedit/commit/cancel and selection match expected text for multilingual fixtures; stale controlled updates cannot revert newer edits.                                |
| Focus             | Modal restore returns to a live intended field; deleted/recycled fields receive no late edit; hardware traversal is deterministic.                                    |
| Privacy           | Secure text is absent from logs/traces/clipboard-by-default and inaccessible surrounding-text payloads; predictions/correction honor the requested secure mode.       |
| Keyboard geometry | Actual show/hide/resize events move the caret into view once; missing keyboard/provider does not generate success events.                                             |
| Accessibility     | Semantic tree and actions reflect live props; focus order survives virtualization; supported reader output is separately demonstrated.                                |
| Lifetime          | At least 100 host mount/focus/unmount cycles with pending events produce no stale callbacks or growing native allocations; physical repeated editing survives resume. |
| Performance       | Measure key/commit-to-visible p50/p95/p99 under a populated screen with Plan 03; record the device-specific budget before claiming improvement.                       |

Existing implementation gates include `pnpm validate:dependencies`, `pnpm typecheck`, `pnpm test:js`, `pnpm validate:architecture` and `cmake --preset linux-host-renderer`, followed by its build and CTest presets. These commands were inspected but not executed for this documentation change. Add tests to actual target names, require a nonzero test count and preserve ON/OFF coverage; do not equate a prop-shape test with behavior.

QNX validation must exercise the current licensed Qt/QNX build on physical hardware, including its actual input devices. Marathon validation must exercise Wayland text-input, real shell keyboard/insets and confinement separately. Reader availability, IME languages and device limitations belong in the receipt. Host screenshots and #44's Hello World cannot satisfy these gates. No new runtime, sanitizer, keyboard or reader result is claimed here.

## Security, deployment and rollback

Limit text and focus information to the authorized application surface. Keep surrounding-text lifetime short and exclude secrets from traces. Any additional Qt module, font or input-method engine needs its own license/deployment inventory; this plan commits no binaries or third-party source. It does not assume the QNX evaluation permits production distribution.

Enable each feature only in the conformance build first. Keep accessibility-tree, keyboard-provider and reader-output support rows independent. Rollback removes this document; later implementation rollback disables the new adapters and restores the prior text/keyboard override bundle together, ensuring focus/session cleanup still runs. The next slice is a reproducible controlled-edit/composition/focus fixture and the exact Qt/RN event contract, followed by hardware-keyboard proof and OS-specific IME work.
