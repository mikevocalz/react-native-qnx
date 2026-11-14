# Design critique before engineering handoff

This is an author critique of the source and proposed design at `f87c742` on
2026-11-14. It is separate from the independent PR review still required.
The roster's designers and agencies are reference standards, not participants
or endorsers. No user-study result or new device screenshot informs this review.

| Surface        | First impression and useful existing behavior                                                                                  | Problem and severity                                                                                                                      | Decision for handoff                                                                                                                                                                                                          |
| -------------- | ------------------------------------------------------------------------------------------------------------------------------ | ----------------------------------------------------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| macOS chrome   | The left lights and centered app identity communicate the configured layout; permanent glyphs on QNX avoid color-only actions. | Moderate: dark focus is too faint, and background title text loses readability. Critical: blocked JS also blocks chrome.                  | Keep the layout and 44 px targets; correct focus/text tokens; make runtime thread separation a required dependency. This follows Kare's small-symbol clarity and Norman's feedback standard.                                  |
| Windows chrome | Trailing full-height actions and left identity have a clear hierarchy; close has a distinct hover response.                    | Moderate: a constant layout property prevents the required runtime switch. Critical: close guards and independent operability are absent. | Preserve control order and focus across a future layout switch; require tested cancel/proceed ownership. Use Ording's interaction consistency as a review lens.                                                               |
| Launcher tile  | Generated icon and measured short name support quick scanning. Equal cell geometry is the correct grid contract.               | Major evidence gap: stock launcher focus, rendering and maximum-name fit have not been inspected on target in this slice.                 | Keep the existing measurement rejection and fixed tile; prove actual font and bounds instead of choosing a character count. Rams' restraint and Pentagram's identity consistency support this choice.                         |
| Preflight      | Proposed module rows put the cause beside its status, followed by a specific build action.                                     | Critical delivery gap: the screen and compatibility handshake do not exist. A copy action also needs a verified backend.                  | Gate evaluation first; show actual module names; offer selectable command text when copy is unavailable. Apply Nielsen's recoverability and IDEO's observed-task approach.                                                    |
| Dev states     | Existing native loading/error UI can preserve context and show real Metro diagnostics.                                         | Major: error buttons lack complete native keyboard/semantic behavior; generic connection/error presentation obscures different remedies.  | Separate connection retry from bundle reload, retain accessible native chrome, and keep technical details beneath a plain cause/action. Use TAT's device conventions and Work & Co's complete-flow standard as review lenses. |

## Hierarchy and consistency

Read app identity first during normal use; read the failed prerequisite first
when the app cannot start. Module status belongs next to the module name, and
one primary action follows the explanation. A technical hash or stack trace
must remain selectable without displacing the heading and recovery action.
The launcher and title bar use the same generated source icon, with each
surface's size set; the launcher short name and full window name have distinct
documented jobs.

Keep transient connection feedback small while useful content remains. A
native incompatibility requires a persistent panel until the developer changes
the runner/build. Avoid modal animations and decorative status badges that
make every event look equally urgent. Ustwo's interaction-craft standard is
applied here through explicit transitions and focus preservation, not added
motion.

## Readiness for handoff

The proposed layout, strings, states and research script are concrete enough
for implementation planning. Delivery remains incomplete: A supplies native
compatibility, B owns connection/session invalidation, the windows API supplies
close/layout lifecycle, and the runtime thread split supplies blocked-JS
operability. The accessibility findings are open implementation work. Verify
both layouts, all five surfaces, reduced motion and keyboard recovery on the
named release image before accepting the design implementation.
