# Keep the drawer destination selected across tabs

Implementation `1f809ce`, based on `4cbb774`, in PR 159. The owner reported
that the drawer lost its selection after moving to another page. The Pi was
on Controls; reopening the drawer showed no selected row. The earlier fill
repair cleared stale backgrounds but did not cover this route-matching error.

`DrawerContent` matched the leaf URL against five drawer URLs. Advanced,
Documents, Controls and More are child routes of the Home `(tabs)` destination.
The component now reads `state.routes[state.index].name` from the drawer
navigator and selects the corresponding destination. The router remains the
single source of state; no React or Zustand state is added.

Mouse testing also found that the native hover rectangle obscured the row in
`QtTouchDispatcher`'s hit test. The hover and focus rectangles now declare an
internal `rnqnxInputTransparent` property. The dispatcher skips those decoration
subtrees, while ordinary native overlays still shield content. This adds no
public JavaScript/Nitro API and does not change renderer or app window size.

## Validation

- `node tests/js/qnx-gallery-drawer-selection.test.mjs`: passed all drawer
  destinations, all five Home child tabs, stale URL versus navigator state,
  navigation/close callbacks and reopen. Running against the old component
  fails on `/advanced` with no selected destination.
- `pnpm typecheck` with pinned pnpm 12.18.0: passed. Exact lock hash is in the
  manifest. Metro production bundle and Hermes 260318099.0.5 bytecode compile
  passed. Compiler warnings are not treated as a clean warning result.
- `rnqnx_qt_native_ui_test`, macOS Release/Qt 6.12.0: 34 cases passed. The test
  checks both actual QML decoration properties as well as existing pointer,
  keyboard, focus and gesture behavior.
- Linux Qt/Hermes host graph: the new
  `inputTransparentDecorationPreservesTouchTarget` regression passed. It fails
  when the dispatcher skip is disabled. The broader transplanted graph retains
  its two recorded pixel/Modal fixture failures; no whole-host or sanitizer
  pass is claimed.
- `node tests/js/qnx-gallery-yoga-layout.test.mjs`: passed after extending its
  explicit native-component stand-ins to the newly stacked SVG gallery cards
  and CSS subpackage. These stand-ins verify layout, not SVG rendering.
- CodeRabbit CLI 0.7.6 reviewed the final seven-file change with zero findings.
  Human reviewer is unassigned; exact-head hosted checks remain required.

The QNX SDP 8.0.5 AArch64 consumer rebuild passed. This is the inherited
Release/O1 build with selected warning exceptions and upstream object reuse;
public Qt 6.12.0 libraries are O2. Project dispatcher/QML changes are compiled.
It does not close the separate fresh combined-build gate.

On the custom-image Pi 400, actual pointer clicks visit Media, Settings, About,
Components and Home. Reopening selects exactly that destination. Advanced,
Documents, More and Controls keep Home selected when the drawer reopens.
Nine captures each contain exactly one teal selected row, checked visually
and at the five row background sample points. The before capture has none.
One showcase process and the USB input endpoint remain after testing.
The 1280x800 window contract is unchanged. These are physical Pi results;
stock QSTI, Marathon, QNX VM and performance qualification were not run.

Artifacts, source/lock/native/HBC hashes and logs are under
`docs/evidence/drawer-route-2026-11-13/`. The native binary is
`ba817a8b1685a3cac87cb297dea773ad88ef1701c2b4aa04760aca79742f4459`;
the optimized bundle is
`59460927f65c26732edabcd985f08d12bf5cb3a73f587c4e47bc9a4353988c12`.
Other components in this exploratory binary are not qualified by this drawer
check. In particular the newly stacked SVG cards report that their native
feature is OFF in this consumer.

## Ownership, skills and rollback

DRI: current integration agent, owning the gallery drawer, native decoration
hit testing and regressions. Independent tool review is complete; human review
is unassigned. Repository qnx-native-api/qnx-platform-validation and local
no-ai-slop were read. The pinned Margelo api-design → build-nitro-modules → cpp
sources at `1e9e17be6f41c838db472d6a4b943fc26355a3be` keep route selection in the
navigator and the native decoration flag internal; no new generated binding or
runtime dependency is needed. Existing React Native, Hermes and Qt versions
are preserved. No permissions, premium payload or license terms change.
Performance deltas are unmeasured.

Rollback: revert `1f809ce`, rebuild native consumer and HBC together, and use
the existing guarded launcher. Retain the before capture and negative tests.
The owner launcher tile still awaits the separately qualified clean combined
build. Merge order and remaining handoff gates are unchanged.
