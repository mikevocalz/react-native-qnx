# Hardware-free window API work, 2026-11-22

This candidate adds a default-OFF Nitro v1 single-window API. It does not close
the window-chrome release gate. Linked candidate CI and QNX runtime/device
acceptance are separate requirements.

## Implementation

- Independent native window handles expose cached typed snapshots, asynchronous
  title, size, position, minimize/restore, visibility, focus and close commands.
  They never create a second window or change the configured initial dimensions.
- The repository generator validates the exact reviewed TypeScript resource
  contract and emits v1 HybridClass descriptors plus matching public types. This
  is a constrained repository adapter, not an upstream Nitrogen QNX target.
- GUI objects stay on their owner. Pending commands and listener admission close
  through a QtReactHost teardown hook before its owned queue closes. Listener
  mailboxes retain an ordered initial snapshot and coalesce later changes into
  one bounded slot while JavaScript is busy.
- Native guards veto close without synchronously entering JavaScript. Opaque
  request IDs identify each veto; cancellation with a stale ID returns false.
  Removing a guard never closes the window automatically. Every active guard
  must be removed before confirmation can close it.
- The QNX manager advertises `close-veto` and accepts cancellation only through
  an owner-written window property. A payload cannot select another window or
  PID. Old managers fail capability discovery; a failed acknowledgement emits
  `RNQNX_WINDOW_CLOSE_VETO_FAILED`. A local Qt veto does not prove receipt by the
  manager. Forced process termination/replacement is outside this guard.
- Native content dimensions and insets reach the QNX core SafeAreaView and
  safe-area-context shim using per-instance Zustand stores and ref-owned handles.
  Default chrome places the surface below the bar and reports zero inset;
  hidden chrome keeps the full surface and reserves the top 44 logical pixels.
  Native chrome input shields and root-relative touch coordinates fix the
  hidden-control and title-bar offset paths when input lifecycle is enabled.

The [API-first contract](../../validation/window-api-contract-2026-11-22.md)
contains caller, error and cleanup examples. Public interfaces are generated
from the same specs without importing React Native globals into Node tooling.

## Evidence labels and checks

| Check                                        | Result and scope                                                                                                                                                                                                                      |
| -------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Isolated Qt chrome/controller suite          | Host: 17/17 CTests; Qt 6.12.0, AppleClang 21, macOS; ASan+UBSan enabled, leak checks disabled; system Qt libraries are uninstrumented                                                                                                 |
| Generated v1 binding compile                 | Host object compilation against actual Nitro alpha136 and RN 0.88.1 JSI headers; this alone is not linked runtime behavior                                                                                                            |
| `window-api-codegen.test.mjs`                | Deterministic generation and negative controls for unsupported signatures/modifiers                                                                                                                                                   |
| `window-insets.test.mjs`                     | Mounted React provider/core view: native changes, cleanup, exceptional setup, insets and content size; simulated native source                                                                                                        |
| Public-entry/router/Nitro source regressions | Host passes; these do not prove target behavior                                                                                                                                                                                       |
| Manager close-veto C policy                  | Host compiled/executed test: cancellation affects only the matching property-window, preserving the other PID/deadline and refusing payload-selected identity                                                                         |
| Window manager QNX build                     | Fresh scratch cross-build with existing licensed SDP 8.0.5/QNX aarch64 GCC 12, `-O2 -Wall -Wextra -Werror`; not run on QNX                                                                                                            |
| Qt WindowHost/WindowSession QNX objects      | Cross-compile passes with public Qt 6.12.0 headers and the repository _QNX_SOURCE toolchain macro; relocatable objects only, no full app link/runtime                                                                                 |
| Documentation and TypeScript                 | `validate:docs` and `tsc --noEmit` pass                                                                                                                                                                                               |
| Linked Hermes API fixture                    | Required ON-lane CI: default/hidden, GUI/dedicated JS owners, actual descriptors, structured getters, errors, handles, cancellation IDs, callback removal and pending command rejection; result must be taken from final candidate CI |

The ON hosted renderer lane uses a copied pinned Nitro core and requires the
window API and dispatcher CTests. The OFF lane requires no window API CTest.
The main playground retains its GUI/JS owner default; blocked-JS proof in the
new fixture does not establish an application/device result.

## QNX protocol source inspection

The QNX 8 documentation says
[MANAGER_STRING is owner-writable with default `rw-r--r--` privileges](https://qnx.com/developers/docs/8.0/com.qnx.doc.screen/topic/screen_8h_1Screen_Property_Types.html)
and sends property-change events. The veto uses that owner's property, with the
window identity supplied by Screen. The manager does not accept an injected
manager-message cancellation. This avoids requiring an application to gain the
privileged context described by
[screen_inject_event](https://qnx.com/developers/docs/8.0/com.qnx.doc.screen/topic/screen_inject_event.html).
Default owner-only write permissions must be preserved. A privileged manager or
client that can alter another window remains outside this boundary. Actual
Screen event delivery and acknowledgement timing need QNX runtime acceptance.

## Skills, review and remaining work

API-first, Nitro and C++ source skills were read in that order from Margelo
`1e9e17be6f41c838db472d6a4b943fc26355a3be`, matching repository skill-lock hashes.
Applied decisions: explicit resources/cleanup and cheap cached getters; actual
v1 alpha136 by-value Promise/Callback APIs rather than older skill examples;
GUI-owned mutation and one dispatcher crossing. Repository native API and
validation skills preserve experimental defaults and separate evidence lanes.
No skill installation or generic upstream generation support is claimed.

DRI: window software agent. Independent reviewer: documentation/release agent.
Review corrections include Wayland variant capability checks, exceptional setup
cleanup, bounded removed-subscription/guard retention, default/hidden geometry
checks, close request cancellation IDs and owner-scoped manager acknowledgement.

Physical board, QNX API execution, device focus/minimize/AppState receipts,
physical keyboard/touch and blocked-JS device acceptance remain open. The API
stays opt-in. Roll back this software slice to remove the API, shim integration
and manager protocol; retain the earlier default Qt recipe and window contract.
No boot, device, publication or deployment action is part of these checks.

[QNX compile record](qnx-compilation.txt) and [source hashes](source-sha256.json) identify these checks.

The recorded generator hash identifies the original compile receipt. A later
entry-point refactor exports the same generation check for the root codegen gate;
its generated C++ and declaration outputs are unchanged. Both standalone and
root generation checks pass after that refactor.
