# Chrome, tile and runner design-system audit

Source review at `f87c742`, 2026-11-14. The audit includes both control layouts,
the launcher identity pipeline, the absent preflight surface and the existing
native dev overlay. Proposed token names below are documentation decisions;
no QML or C++ token migration has been made.

## Tokens and naming

| Proposed token                                   | Source value                      | Owner and decision                                                                          |
| ------------------------------------------------ | --------------------------------- | ------------------------------------------------------------------------------------------- |
| `chrome.surface.active.dark`                     | `#13202f`                         | WindowChrome.qml `barColor`; retain.                                                        |
| `chrome.surface.inactive.dark`                   | `#172433`                         | Retain background; keep readable identity text.                                             |
| `chrome.text.primary.dark`                       | `#e6edf3`                         | Active title/glyph; also use for readable inactive title in proposal.                       |
| `chrome.text.inactive.light`                     | proposed `#595963`                | Replaces source `#86868b` on `#f6f6f6` after implementation review.                         |
| `focus.ring.dark`                                | proposed `#79e8dc`                | Reuse dev menu accent for dark focus; source chrome `#0a64d6` is below 3:1 on its dark bar. |
| `focus.ring.light`                               | `#0a64d6`                         | Keep on the light chrome surface.                                                           |
| `runner.surface`                                 | proposed `#13202f`                | Same app chrome family for native compatibility and connection panels.                      |
| `runner.text`                                    | `#e6edf3`                         | Main instructions.                                                                          |
| `runner.error.surface`                           | source `#7f1d1d`                  | Existing error screen; proposed compact error region within runner surface.                 |
| `control.target`                                 | 44 logical px                     | Existing chrome minimum; apply to preflight/dev actions.                                    |
| `chrome.height`                                  | 44 logical px                     | Preserve established bar/inset contract.                                                    |
| `chrome.icon`                                    | 20 logical px, sourceSize 40 px   | Existing title icon; no new source corner masks.                                            |
| `type.chrome`                                    | 14 px DemiBold                    | Existing title; one line with elision and full accessible name.                             |
| `type.runner.title`                              | 24 px bold                        | Existing error heading; use for preflight.                                                  |
| `type.runner.body`                               | proposed 16 px, 24 px line height | Readable instructions; diagnostics retain 14 px monospace with wrapping.                    |
| `space.inline` / `space.control` / `space.panel` | 8 / 16 / 24 logical px            | New-panel scale; existing dev overlay uses 28 px, to migrate deliberately.                  |
| `motion.feedback`                                | source 160 ms                     | Existing banner y/opacity transition; reduced-motion branch still missing.                  |

The source hardcodes these values across QML strings and files. Extract tokens
only with behavioral tests and dark/light screenshots. Retain names by role,
such as `runner.error.surface`, instead of naming colors by hue.

## Component coverage

| Surface          | Existing states                                                                                   | Missing states or contract                                                                                               |
| ---------------- | ------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------ |
| macOS controls   | Active/inactive, hover, pressed, keyboard focus, maximize/restore, optional unavailable controls. | Runtime layout change; close-guard pending/cancel/proceed; blocked-JS operation.                                         |
| Windows controls | Same state set, full-cell hover fills, trailing actions.                                          | Same lifecycle gaps; contrast fix for dark focus.                                                                        |
| Launcher tile    | Generated icons, measured short-name fit, desktop Singleton entry.                                | Actual per-target grid/focus/label receipt; tile belongs to QNX launcher, not this QML component.                        |
| Runner preflight | No implementation.                                                                                | Missing/mismatched modules, malformed metadata, build instructions, long/many names, keyboard copy/close actions.        |
| Dev overlay      | Loading banner, menu, bundle/network error, connected flag.                                       | Unified reconnect state contract, native accessible action semantics, full keyboard traversal and enlarged-text receipt. |

Use `action.close`, `action.minimize`, `action.maximize`, `action.restore`
consistently in the public design contract. Existing implementation action
`maximize` also handles restore; preserve its internal API until the native
contract is reviewed. A visual label change must not create a second action.

## Priority implementation work

1. Correct dark focus contrast and retain full accessible identity names.
2. Give native dev/preflight buttons focusable button semantics and at least
   the existing 44 px project target; preserve Escape/reload behavior.
3. Use one bounded diagnostic component with copy support in preflight and
   bundle-error states; avoid storing diagnostic payloads in serialized state.
4. Add close-guard and runtime-layout states only with the windows API and
   independent lifetime review. Keep resources and imperative handles in refs
   if a React consumer renders these contracts; use per-root Zustand stores.

No numerical component score is assigned: source inventory cannot measure
rendered quality, usability or assistive-technology output. Contrast
calculations are separately recorded in [contrast.json](contrast.json).
