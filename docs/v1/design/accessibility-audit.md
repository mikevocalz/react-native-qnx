# Accessibility review of the five native surfaces

Source review on 2026-11-14 at `f87c742`, using WCAG 2.1 AA as the review
framework for this native application. It is not a conformance claim. No
keyboard, screen-reader, zoom, rendered-pixel or device session was run here.
The preflight screen is a proposed design and has no implementation to audit.

WCAG [text contrast](https://www.w3.org/WAI/WCAG21/Understanding/contrast-minimum.html)
uses 4.5:1 for ordinary text; inactive-component exceptions require a reasoned
application, not a blanket exception for every background window title.
[Non-text contrast](https://www.w3.org/WAI/WCAG21/Understanding/non-text-contrast.html)
uses 3:1 for meaningful control/state cues. The project's 44 px target floor
also adopts [SC 2.5.5](https://www.w3.org/WAI/WCAG21/Understanding/target-size.html),
which is AAA in WCAG 2.1; it must not be mislabeled an AA requirement.

## Findings

| ID        | Surface                            | Finding and source                                                                                                                                                                                 | Criterion                                             | Severity                  | Required disposition                                                                                                                |
| --------- | ---------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ----------------------------------------------------- | ------------------------- | ----------------------------------------------------------------------------------------------------------------------------------- |
| G-A11Y-01 | macOS and Windows chrome           | Source focus ring `#0a64d6` on dark active bar is below 3:1.                                                                                                                                       | 1.4.11; visible focus 2.4.7 also needs rendered check | Major                     | Implement a contrast-safe dark focus token; inspect every hovered/pressed adjacent background.                                      |
| G-A11Y-02 | Both chrome layouts                | Inactive title values yield less than 4.5:1. A title still identifies the background window.                                                                                                       | 1.4.3, applicability review                           | Moderate                  | Retain readable title text; document any inactive-component exception explicitly. Proposed values meet the ordinary-text threshold. |
| G-A11Y-03 | Both chrome layouts                | Native button role/name, keyboard activation and focus ring exist in source/tests; actual QNX assistive output is unverified.                                                                      | 2.1.1, 2.4.3, 4.1.2                                   | Major evidence gap        | Run keyboard and available assistive-provider checks on the release target. Do not equate semantic queries with a screen reader.    |
| G-A11Y-04 | Tile                               | Fixed icon/name and measured shaping pipeline exist; actual launcher focus, accessible name and equivalent tile geometry remain unverified here.                                                   | 1.1.1, 2.1.1, 2.4.7, 4.1.2                            | Major evidence gap        | Inspect stock launcher behavior and record icon-only fallback, complete names and keyboard launch.                                  |
| G-A11Y-05 | Preflight                          | No native implementation exists. Missing/mismatched module names cannot yet be reached or announced.                                                                                               | 1.3.1, 2.1.1, 3.3.1, 4.1.2                            | Critical delivery gap     | Implement pre-evaluation native screen and test initial focus, module list, command selection and close.                            |
| G-A11Y-06 | Dev states                         | Inline `DevButton` uses a MouseArea and label text; it lacks its own accessible button role/name and focusable keyboard activation. Error actions have no general Tab traversal in this component. | 2.1.1, 2.4.3, 4.1.2                                   | Major                     | Use native focusable controls with names and Enter/Space behavior; retain menu shortcuts as additional access.                      |
| G-A11Y-07 | Dev states                         | Buttons use implicitHeight 42; project target is 44.                                                                                                                                               | Project target; 2.5.5 AAA                             | Minor                     | Increase targets to at least 44 logical px without requiring larger outer windows.                                                  |
| G-A11Y-08 | Dev states                         | Banner animates y and opacity for 160 ms with no reduced-motion branch in inspected source.                                                                                                        | Motion preference and project design requirement      | Moderate                  | Add a motion preference path using static state transitions; verify no essential information depends on animation.                  |
| G-A11Y-09 | Both chrome layouts and dev states | JavaScript shares the GUI thread, so long JS work prevents native window/overlay operation.                                                                                                        | Operability/recovery requirement                      | Critical architecture gap | Complete runtime ownership work, then test controls with deliberately blocked JS. Styling cannot fix it.                            |

## Source-color calculations

Calculated using sRGB relative luminance. Raw values and method are in
[contrast.json](contrast.json); [raw validation output](validation.txt) includes
unrounded ratios and source excerpts. Rounded display values below do not change
threshold comparisons. These pairs do not cover translucent overlays, all
states, glyph colors, tile pixels or actual device rendering.

| Pair                                                | Ratio    | Assessment                                                              |
| --------------------------------------------------- | -------- | ----------------------------------------------------------------------- |
| Active dark title `#e6edf3` / `#13202f`             | 13.935:1 | Meets ordinary-text threshold.                                          |
| Inactive dark title `#7d8a99` / `#172433`           | 4.465:1  | Below ordinary-text threshold.                                          |
| Inactive light title `#86868b` / `#f6f6f6`          | 3.352:1  | Below ordinary-text threshold.                                          |
| Current dark focus `#0a64d6` / `#13202f`            | 2.985:1  | Below meaningful-cue threshold.                                         |
| Current light focus `#0a64d6` / `#ececec`           | 4.670:1  | Meets meaningful-cue threshold.                                         |
| Proposed dark focus `#79e8dc` / `#13202f`           | 11.283:1 | Meets threshold for this pair; other adjacent states still need checks. |
| Proposed light inactive title `#595963` / `#f6f6f6` | 6.405:1  | Meets ordinary-text threshold.                                          |
| Error text `#fde8e8` / `#7f1d1d`                    | 8.533:1  | Meets ordinary-text threshold for solid pair.                           |
| Dev button text `#f3f6fa` / `#2b3a4f`               | 10.640:1 | Meets ordinary-text threshold for solid pair.                           |

## Keyboard and announcement contract to verify

| Surface        | Keyboard route                                                                                                    | Semantic and recovery checks                                                                               |
| -------------- | ----------------------------------------------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------- |
| macOS chrome   | Tab/Backtab through close, minimize, maximize in physical order; Enter/Space activates.                           | Button names follow maximize/restore state. Keep focus across resize and layout changes.                   |
| Windows chrome | Tab/Backtab through minimize, maximize, close in physical order.                                                  | Same names; do not move focus merely on hover.                                                             |
| Tile           | Launcher-native focus navigation and Enter launch/restore.                                                        | Full app short name plus role; no extra tab stop for decorative icon.                                      |
| Preflight      | Heading announced once, module list reachable, command selectable, then build/copy and close.                     | Do not repeatedly announce unchanged diagnostics. Escape must not allow incompatible JS evaluation.        |
| Dev states     | Error heading/details/actions reachable; Escape closes details only where safe; existing reload shortcut remains. | Announce connection-state changes without frame-by-frame chatter. Restore prior focus after details close. |

All five need 200% text/scale testing, RTL, long identifiers and high contrast
settings where the target supports them. The test must record unsupported
assistive facilities explicitly. No automatic scanner was used: a web scanner
cannot audit this Qt native semantic tree or QNX launcher.
