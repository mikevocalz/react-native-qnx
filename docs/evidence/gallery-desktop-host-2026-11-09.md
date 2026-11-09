# Gallery input and desktop window repair: host evidence, 2026-11-09

Base: PR #104 head `7fd26983b04e44d15536144077b9b2a3e7d81082`.
The owner reports that the installed gallery renders, but buttons and tabs do
not activate, wheel movement is reversed, and the Switch is drawn incorrectly.
That is a physical rendering observation with interaction failures. No QNX
machine, VM or physical board was contacted for this repair.

The historical installed ELF comes from PR #102's native snapshot
`1a294310612228139c19dca78fb0ec4ffab77ac6`; PR #104's JavaScript gallery alone
does not identify the native binary. Its recorded ELF SHA-256 is
`47ae39c5f00f7e2c9e35a3dcd5b111733a675915b97f87d7fbeafbf9d337ce26`.
Replacing only an HBC file cannot deliver these native fixes. Preserve the
historical install receipt separately from a newly rebuilt executable.

## Source findings and changed behavior

| Area               | Observed source problem                                                                                                                                                                             | Repair and boundary                                                                                                                                                                                                                               |
| ------------------ | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Pointer delivery   | Production `main.cpp` constructed and connected `QtTouchDispatcher` only inside the default-OFF input lifecycle flag. QML Flickable scrolling did not establish that Fabric/React received presses. | Install baseline input unconditionally; keep the existing experimental lifecycle behavior behind its unchanged flag. Hit testing follows QML decorations to their Fabric ancestor and respects actual visual stacking.                            |
| Client coordinates | A desktop frame gives the React root a nonzero window origin. Window-relative page coordinates and unconditional responder targeting would admit chrome events.                                     | Map page coordinates into the React client; retain global screen coordinates and local target coordinates. Reject new contacts outside or while the client is disabled.                                                                           |
| Switch             | The painting root obscured the rounded track/thumb with a white square; the default off track also lacked contrast.                                                                                 | Share production Switch QML between the app and tests. Use a nonpainting root, a gray off track and a white thumb; retain Yoga dimensions, controlled React value, custom colors and disabled behavior.                                           |
| Modal              | Generic upstream C++ Modal screen size is zero; a Modal under scrolled content also inherited that visual parent and could leave native controls interactive behind it.                             | Feed per-instance Modal state from the current client viewport, keep logical Fabric child order, portal only the visual item above the client, and block background native pointer/wheel input. Escape sends the existing `onRequestClose` event. |
| Resize             | Startup constraints alone did not update Yoga when the native client changed size.                                                                                                                  | Add an internal native `resizeSurface` operation using the existing SurfaceHandler constraints. Coalesce window changes on the Qt loop and preserve prior error state.                                                                            |
| Desktop frame      | The consumer provided a content window without desktop menus or controls.                                                                                                                           | Add default-OFF `RNQNX_ENABLE_DESKTOP_WINDOW` and opt-in `--desktop`: Qt Quick Basic menus, close/maximize/restore, shortcuts, focus feedback and resizing. System-frame mode leaves title controls to an existing manager.                       |
| Wheel reversal     | Qt already converts QNX Screen wheel sign. A blanket second inversion would break conforming input.                                                                                                 | Add explicit app-local `--invert-wheel`; default input is unchanged. Preserve event metadata and acceptance, reverse wheel deltas once, and leave mouse dragging/touch alone.                                                                     |

The existing Fabric/Yoga/Hermes/JSI/Qt Quick renderer remains in use. These are
native host integration repairs, not a new app-facing JavaScript window API.
No replacement renderer, Qt Widgets dependency, paid Forge dependency, new
TurboModule API, platform identity change, ABI upgrade or lockfile change was
introduced. New public services remain subject to the pinned Nitro v1 policy.

The relevant changes are in `vnext/probes/playground/main.cpp`,
`vnext/src/fabric`, `QtReactHost`, the three Qt view helpers and desktop QML,
their CMake registration, real-window tests, the host workflow and the
[desktop consumer guide](../getting-started/desktop-window.md).

## Host validation completed before publication

Host: Linux x86_64, Qt 6.11.1, GCC 13.3 for the focused native fixtures,
CMake 3.31.10, Ninja 1.13.0, Node 26.0.0 and pnpm 12.8.1. The full native
graph is being built separately with Ubuntu Clang 18.1.3, the compiler family
selected by the Ubuntu 24.04 CI workflow.

| Command / gate                                                                                | Actual result                                                                | Scope                                                                                                         |
| --------------------------------------------------------------------------------------------- | ---------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------- |
| `pnpm validate:dependencies`                                                                  | Passed                                                                       | Frozen dependency and source contracts.                                                                       |
| `pnpm typecheck`                                                                              | Passed                                                                       | JavaScript/TypeScript checks, not native execution.                                                           |
| `pnpm validate:docs`                                                                          | Passed on the candidate documentation, including this receipt.               | Generated records and local links.                                                                            |
| `node tests/js/qnx-component-conformance.test.mjs`                                            | Passed                                                                       | Direct JS fixture registration/state checks; no native input claim.                                           |
| `pnpm qnx bundle-qnx --app component-conformance --out build/component-conformance.bundle.js` | Passed                                                                       | Production QNX Metro bundle.                                                                                  |
| Qt `qmllint` on all four desktop QML files                                                    | Passed with zero warnings                                                    | Actual Qt 6.11.1 tool.                                                                                        |
| Qt C++ review linter on desktop/wheel wrappers                                                | Passed with zero findings after review fixes                                 | Static review, not a runtime gate.                                                                            |
| Standalone shared Switch QML probe                                                            | Candidate passed; exact PR #104 Switch failed the same corner/contrast check | Real Qt pixels and mouse events, including disabled and re-enabled controls.                                  |
| `ctest -R '^rnqnx_wheel_direction$' --output-on-failure -V`                                   | Passed, 0.35 seconds after final review fixes                                | Actual repository target, strict C++20, ASan/UBSan, real Flickable wheel events.                              |
| `ctest -R '^rnqnx_desktop_window$' --output-on-failure`                                       | Passed, 1.16 seconds; ASan-enabled test passed, 1.24 seconds                 | Real Qt window mouse/keyboard events and rendered host frame.                                                 |
| `qtquick_mounting_layer_host_test`                                                            | All 17 cases passed, 0.15 seconds                                            | Actual repository Qt mounting target, ASan/UBSan; includes scrolled Modal portal/order/positive-z regression. |
| Strict syntax of all seven changed native integration translation units                       | Passed against configured RN/Qt/Hermes headers                               | Compilation syntax only; not linking or runtime proof.                                                        |
| Full Hermes/Fabric/React native graph                                                         | Pending at this publication checkpoint                                       | Fresh Clang build, followed by OFF and ON lifecycle/desktop lanes.                                            |

Observed native success output:

```text
RNQNX_WHEEL_DIRECTION_OK default metadata acceptance lifetime mouse touch flickable
RNQNX_DESKTOP_WINDOW_OK mouse keyboard popup-blocking resize capability system-frame
```

The wheel regression exercises normal, explicit-off and inverted behavior,
both axes and signs, pixel metadata, acceptance, single delivery, other windows,
lifetime, extreme deltas, mouse/touch preservation and a real Flickable. The
desktop regression checks maximize/restore/close, F11, F10 with Space/Down/
Return/Enter, Left/Right menu navigation, Escape, client blocking and recovery,
viewport resizing, unavailable minimize and the system-frame contract. It
rejects QML warnings. These tests caught and verified repairs to the QML
window reference type and menu keyboard activation.

The desktop test executable uses ASan and allocator interception; not every
linked Qt/library translation unit is instrumented. Wheel and mounting
fixtures have their recorded ASan/UBSan coverage. Leak detection is disabled
for Qt process-lifetime singletons, following the repository's host convention.
None of this is a physical Screen/QPA test.

Frozen lock SHA-256:
`b854ebe2e50a42b2e8e07124df065592d99d43b1d1a3a984b66d3835ad86c3e4`.
Production gallery bundle SHA-256:
`cf621bd1cbf52f235b3ac399bcfa707d1f05bd16e313bc397b42407d5d5e723c`.

### Full-build failure retained and next gate

The initial full GCC build stopped in the installed React Native source:
Clang-specific diagnostic pragmas, then `NetworkIOAgent.h` anonymous-namespace
member linkage under `-Werror=subobject-linkage`. No warning exception is
committed. The attempt and local diagnostic logs remain available separately;
it is not a passing full host build.

A fresh Clang build uses verified official Ubuntu 24.04 packages extracted
into a task-local prefix, with no system package or compiler-policy change.
Ubuntu-signed main/universe indexes, archive SHA-256 and sizes were checked.
The selected 13 archives total 83,846,644 bytes. The compiler/library closure
and a C++20 ranges/`std::jthread` program with `-Werror`, ASan and UBSan passed.
This qualifies the host compiler setup, not the product.

The full native test adds actual production `main.cpp` window events through
Hermes, React and Fabric, plus a negative interception case that must fail
before React receives a press. Tests cover text and padding clicks, repeated
and disabled presses, tab switching, a Modal under an actually scrolled view,
background pointer/wheel suppression, native Switch interaction, resizing,
modal close/reopen/Escape, and subsequent app use. The host workflow runs
both lifecycle modes; the ON lane also enables and tests the desktop frame.
The validator requires these tests to exist instead of accepting an empty or
partial test inventory. Their final runtime result is still required.

## Sources, skills and review ownership

| Skill/source                                                                   | Exact revision/version                                                             | Decision applied                                                                                                                                          |
| ------------------------------------------------------------------------------ | ---------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Margelo `api-design` → `build-nitro-modules` → `cpp`                           | `margelo/react-native-skills` at `1e9e17be6f41c838db472d6a4b943fc26355a3be`        | Keep public contracts separate from internal host plumbing, use actual pinned Nitro v1 for new app services, and preserve explicit Qt/JSI ownership.      |
| Callstack `react-native-best-practices`, `writing-user-docs`, `github-actions` | `callstackincubator/agent-skills` at `59e9d17446a248b21d86f078fcd0bd2d36db1ca4`    | Validate React state through native events, document real launch/deployment boundaries, and extend the existing CI matrix with exact candidate artifacts. |
| Qt `qt-qml`                                                                    | `TheQtCompanyRnD/agent-skills` at `062f7c9f0e16b7d2f66cd55e449a3ed85fba699e`, v1.3 | Use scoped Basic controls, explicit bindings, a nonpainting layout root and native focus feedback.                                                        |
| Qt `qt-ui-design`                                                              | Same revision, v1.1                                                                | Respect the connected desktop display and keyboard/mouse interaction while leaving application layout to Yoga.                                            |
| Qt `qt-cpp-review`                                                             | Same revision, v3.0                                                                | Review RAII, window/engine lifetime, thread ownership and QML integration; fix a release-elided assertion and macro-sensitive limits.                     |
| Repository `qnx-native-api`, `qnx-platform-validation`                         | PR #104 base `7fd26983b04e44d15536144077b9b2a3e7d81082`                            | Keep source, host, installed historical binary and physical acceptance evidence separate.                                                                 |
| Neutrino Programmer's Guide and System Architecture                            | `Himmele/My-Blog-Repository` at `b1fd3e9410c1a6a0d7217c3be7e324e900470d2a`         | Inform lifetime, bounded work and recovery review; these QNX 6.3-era books are not QNX 8 API authority.                                                   |

The Qt skills were read from their pinned upstream checkout and applied to
this change. No claim of a personal skill installation, vendor review or
upstream endorsement is made.

Automated execution roles follow the repository roster: the platform
integrator owns desktop production code, CI integration and evidence; the RN
input/runtime agent owns the dispatcher, Modal, resize and production-window
driver; the Qt/RN reviewer owns Switch and desktop/React integration tests and
independently reviews the Modal implementation; the QNX regression agent owns
wheel qualification, current QNX sources and host toolchain setup. The
integrator independently reviewed all native seams and the wheel wrapper.
Human DRI and maintainer acceptance remain unassigned.

Primary current platform sources are linked in the
[desktop consumer guide](../getting-started/desktop-window.md). In particular,
QNX Everywhere lists Raspberry Pi wheel reversal as QNXE-1445; that is a
possible match for the user's image, not a diagnosis. Qt 6.12 QNX explicitly
does not implement native minimize. The app disables that operation there;
complete minimize/restore belongs to a verified desktop manager integration.

## Remaining target, compatibility and performance gates

- Physical QNX: rebuild native ELF and HBC, stage matching target Qt Quick
  Controls/Basic dependencies, identify Screen/QPA/image/window manager, and
  execute every case in the desktop guide. No target receipt is upgraded by
  this host work.
- Marathon/Linux Wayland: shared native source is exercised on Linux offscreen,
  but no Wayland compositor, shell integration, app permission or installed
  Marathon package is qualified here. Platform identity and resolver pins are
  unchanged.
- Desktop completeness: native file pickers, app-specific Open/Save commands,
  full React keyboard/IME/accessibility behavior, complete Modal lifecycle
  animations and window-manager minimize/restore remain separate work.
- Performance: resize updates are coalesced and unchanged wheel input installs
  no filter. No new frame-time, input-latency, memory or throughput budget is
  claimed; measure on the selected QNX hardware before performance acceptance.
- Security/licensing: no credentials, privileged IPC, boot changes, permission
  bypass or paid component are added. Qt Quick Controls deployment must retain
  the matching open-source module/plugin notices. QNX runtime terms remain
  separate. Existing earlier evaluation receipts remain historical.

Keep PR #104 draft until the missing full-native and physical gates are
resolved. The next implementation slice is determined by the first actual
device failure, with desktop manager integration qualified separately.

## Rollback

Omit `--invert-wheel` to restore standard wheel interpretation. Omit
`--desktop` or configure `RNQNX_ENABLE_DESKTOP_WINDOW=OFF` to retain the ordinary
host window. These choices do not roll back the baseline input and Modal
repairs. To restore the exact old implementation, revert the repair commit
and rebuild the native executable; preserve the original ELF/HBC and launch
command as a separately identified fallback. Do not mix a new bundle receipt
with an old native binary hash.
