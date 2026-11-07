# Phase 3: ordinary Expo navigation and native application interaction

Status: proposed implementation plan; this PR contains documentation only.
Source baseline: main `cfc28165525e0d7c54192daf58b9b4318a26e83a`, 2026-11-07.
Predecessors: [Phase1](https://github.com/mikevocalz/react-native-qnx/pull/82) external app and [Phase2](https://github.com/mikevocalz/react-native-qnx/pull/83) registration/compatibility contracts for affected dependencies. Pure shared UI work may begin before every Phase2 module is finished.

## Outcome and existing work

The same non-premium Expo application navigates routes, edits a form, scrolls a substantial list and uses an interruptible native gesture drawer on QNX. Reuse those screens and shared Qt/Fabric behavior for Marathon; do not maintain a target-specific copy of the app. QNX stays primary/default and the platform name is unchanged.

Use [component support](../component-support.md), [input/animation map](../reanimated-gesture-handler-qnx-map.md), [source audit](../reanimated-qnx-source-audit.md), existing workstreams02/03/04/11/12 and [conformance PR75](https://github.com/mikevocalz/react-native-qnx/pull/75). Preserve scoped Pressable, TextInput and accessibility receipts without promoting them into physical HID, IME, assistive-reader or Reanimated results. A passing initial screenshot is not interaction acceptance.

## Public behavior contract

One Expo Router route tree serves both targets. Qualify stack push/pop, tabs, route parameters, direct cold/warm links, modal dismissal and root Back. A Back request closes the topmost dismissible UI before popping navigation; unsupported/unhandled root behavior returns once to the OS adapter. Timeout and late reply cannot perform two pops. A repaint/animation never counts as a semantic acknowledgment.

Use public component APIs and implement their observed behavior, not silent compatibility stubs. Define measured safe areas, keyboard geometry, focus movement, controlled editing, UTF-16 selection and composition. Unknown provider data is unknown, not a fabricated zero. Normal list virtualization must preserve selection and release off-window cells. Keep Yoga layout authoritative; Qt paints and implements native interaction through the existing backend.

Worklets, Reanimated and RNGH require their audited real runtime/bootstrap/property/gesture integration. Do not replace them with a lookalike custom animation library or a Nitro wrapper. Respect reduced-motion preferences and user interruption; correct visual state and callback ordering must survive cancellation, remount, deactivation and reload.

## Roster and path ownership

Proposed responsibilities only; actual human DRIs and independent reviewers remain unassigned.

- Router/navigation engineer: public JS overrides, route integration and proposed shared reference-app routes; reviewer: RN navigation/runtime engineer. Deliverable: one route graph, semantic Back and deep-link behavior.
- Fabric/Qt input engineer: `vnext/src/fabric/`, `vnext/src/views/qt/`, associated `cmake/`; reviewer: QNX input/lifetime engineer. Deliverable: cancellation, hit testing, pointer ownership, scrolling and no duplicate action delivery.
- Worklets/animation engineer: runtime seams, platform overrides and the existing animation audit; reviewer: independent Hermes/JSI engineer. Deliverable: actual library integration and compatible generated/native contracts.
- Text/accessibility engineer: TextInput backend, keyboard/focus/accessibility adapters; reviewer: text-platform and accessibility QA. Deliverable: composition, editing and observed assistive technology behavior.
- Adaptive UI engineer: shared reference app and platform metrics consumers; reviewer: layout/input QA. Deliverable: narrow/wide/rotation/occlusion layouts with preserved state; native metrics provider remains Phase4 ownership.
- Test/measurement engineer: registered JS/C++ interaction tests and physical receipts; reviewer: independent performance/device QA. Deliverable: positive/negative controls, videos/logs, exact flags and target limitations.

Serialize overlapping Fabric/runtime edits through the Phase0 integrator. Proposed reference-app/test paths must be selected against the implementation checkout, not presumed to exist.

## Skills and source requirements

Read repository `qnx-native-api`, `qnx-platform-validation`, the [v1 policy](../platform-modernization/NITRO_V1_API_POLICY.md) and existing [skills/roster](../platform-modernization/SKILLS.md).

- Margelo ordered `api-design` -> `build-nitro-modules` -> `cpp`, revision `1e9e17be6f41c838db472d6a4b943fc26355a3be`, paths `skills/<name>/SKILL.md`: explicit native operation/cleanup contracts, supported generated shapes and Qt/JS ownership. Apply to new native APIs, not as a substitute for actual Worklets/RNGH integration.
- Callstack `react-native-best-practices`, revision `59e9d17446a248b21d86f078fcd0bd2d36db1ca4`: measure the actual interaction; include its animation/list/memory references after source audit. Do not treat mobile `agent-device` commands as QNX support.
- Expo `expo-module`, revision `a9e5bd3fa00b4e61f632e43f85da73aee1942582`, `plugins/expo/skills/expo-module/SKILL.md`: native view/event registration and explicit platform capability.
- Callstack `github-actions` at the pinned revision for affected CI; `writing-user-docs` only when documenting consumer APIs/limits.
- Before navigation implementation, discover and read the exact available Expo Router/navigation skills and current Router/screens/safe-area/Worklets/Reanimated/RNGH sources. Record commit/path and actual supported native hooks. Existing roster names are selection requests, not proof those skill paths exist. UI/design-review skills are conditional on actual component design; verify sources rather than inventing them.

Planning source guidance has been read; no external installation, mobile automation, new native implementation or device test is claimed. Version changes require explicit compatibility work and a matched lockfile; generic older Nitro examples do not override actual v1 contracts.

## Implementation slices

1. Inventory Router's actual dependencies and required public RN exports. Port/qualify the smallest stack and Back path first, with truthful incompatibility diagnostics. Do not mark all Router modes supported from one screen.
2. Add shared tabs, modal and deep-link tests; verify app/surface identity and cross-app isolation. Coordinate OS Back delivery with Phase4/5 without circularly requiring their entire completion.
3. Finish controls, text/focus/selection and actual keyboard/IME providers; attach physical keyboard/touch evidence separately from synthetic Qt input. Connect accessibility semantics to the target's real reader path where available.
4. Integrate actual Worklets/Reanimated/RNGH on the existing host. Test interrupted drawer movement, cancellation on target removal, callbacks after unmount, gesture conflict and event timestamps. Keep experiments default OFF until each target gate passes.
5. Run the same reference app at narrow/wide sizes and through rotation/keyboard occlusion; preserve route, text and selection. Use Phase4 device metrics rather than hard-coded device models.

## Tests and exit gate

Existing frozen dependency, typecheck, JS, architecture/codegen and full native renderer checks remain required. Add registered tests that drive actual React handlers and native event delivery, with disabled/intercepted-delivery controls that fail at the intended assertions. Run OFF/ON regressions and relevant sanitizers. Keep automated host, Wayland, QNX physical and Marathon shell evidence separate.

- [ ] One shared Router tree supports the declared stack/tab/modal/link/Back scenarios.
- [ ] Controls, list virtualization, edits and focus remain correct across remount/reload.
- [ ] Physical input and real IME/accessibility disposition are recorded without synthetic substitution.
- [ ] Actual gesture/animation libraries perform the drawer interaction and interruption correctly.
- [ ] No callbacks target destroyed runtime/view state; no duplicate semantic navigation.
- [ ] Adaptive layouts preserve application state and report unsupported providers honestly.
- [ ] QNX passes independently; Marathon receives its own interaction receipt when available.

## Performance, security and rollback

Measure input-to-presentation where instrumentation supports it, frame intervals, allocations and retained view counts on the named target. A JS timer, queued frame or screenshot is not a presented-frame benchmark. Do not invent a universal FPS budget; derive display budgets from measured refresh and compare the same workload/revision settings.

Treat deep links as untrusted input; validate route/parameter scope and prevent cross-app activation. Do not bypass permissions or package checks to make gestures/navigation work. No premium assets/components are required; optional NYC-Tron UI consumption cannot replace core behavioral proof.

This plan runs no native or device tests and promotes no support rows. Roll back bounded native slices using their default-OFF flags, restore matching JS/native contracts and preserve app data. Documentation rollback alone removes this file. Phase4 adds service/lifecycle providers; Phase5 qualifies Marathon's shell side; neither should fork the shared UI.
