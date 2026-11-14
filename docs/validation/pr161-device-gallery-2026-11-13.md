# SVG gallery and QNX target continuation

Base: `a52ffff0fb7d871d365c64e82c22ee334cdb46f2`, PR 161. Its required checks
passed: renderer OFF 25m42s, renderer ON 29m01s, validation and image-cache.
The enabled runner passed all 49 integrated CTests, eight standalone native
CTests and the final required probe. All four corpus shards passed (4.19,
183.91, 180.40, 4.85 seconds) with the original 300-second per-test limit.
[Exact run](https://github.com/mikevocalz/react-native-qnx/actions/runs/43528086231).
These results belong to that base; this follow-up needs its own head checks.

## Fix and source inspection

The real gallery device run found `SvgCss` throwing from the package root.
Installed react-native-svg 15.16.0 `src/deprecated.tsx`, `src/css/index.tsx` and
`css/package.json` establish the correct import: `react-native-svg/css`.
`XmlCard` now uses it. The Metro regression builds the actual gallery component
and verifies its CSS child is the subpackage implementation. It fails with
the old import and passes after the fix, alongside 57 QNX and 57 iOS probes.
The resolver's 24 overlay files remain QNX-only.

Upstream tag `v15.16.0` resolves to
`876509108002caa993da27d76eacf32b2c050411` (queried from the upstream Git remote).
The npm registry reports integrity
`sha512-uh8yktufYeRU3W9Omi8+SPyLF9BVN4JmLflSgc9oOcKkSKk/S5JUmmTjKZq/L8GHfQUtCvcu/W6X4xhEfBBAjQ==`;
it supplies no gitHead. No version or lock changes were made. Installed RN is
0.88.1, React 19.3.0 and Expo 58.0.24. Lock SHA-256:
`4198fb84cd0abd9e8da568af4741a12c2318280827d3b823727e92ed98b2b565`.

The gallery lacked live ForeignObject input coverage. The new card uses a
per-instance Zustand store and checks a transformed group containing a real
React Native button, including ancestor `pointerEvents="none"`. The image card
also displays the gallery's existing HTTPS JPEG source. The separate nested
invalidation regression creates a real Qt Quick window: a retained descendant
prop change must schedule the visible root and repaint blue. Removing outward
invalidation makes it fail; restoring production code passes all eight native
host CTests. The existing symbol/viewBox and nested-group layer regressions
also pass. All three earlier review threads were resolved against these fixes.
No production native renderer code changes in this follow-up.

## QNX build and deployment

The source-corrected consumer cross-build completed with QNX SDP 8.0.5,
AArch64, public-source Qt 6.12.0, Release/O1, two build jobs, sanitizers OFF.
The inherited native recipe uses strict warnings with selected GCC
warning-as-error exceptions. Qt runtime libraries are O2. An inherited compile
launcher reused upstream objects from the earlier Worklets tree; project
vnext/native/test sources were recompiled. The launcher checks source equality
but does not independently verify flags or header closure. These device bytes
are scoped compatibility evidence; a clean final build without that launcher
is required for combined qualification. Hosted strict checks remain separate. RNSVG, native UI, window chrome and input lifecycle are ON;
Worklets/Reanimated/Forge remain OFF. The final link uses the real lean Hermes
artifact and its ELF names `libhermesvmlean.so`. Older full-VM alias experiments
were retained as historical artifacts and are not this candidate.

| Artifact                  | SHA-256                                                            |
| ------------------------- | ------------------------------------------------------------------ |
| Native SVG consumer       | `87a10e99eb2ef492b630a572426e2d723da21410dbb4be755b546c5c43d8b7e3` |
| Final gallery HBC         | `94ccdea70d031f27544dba7a57ef9bcad8f9fd43aba6b7337934ea62dda98ed0` |
| Selected lean VM          | `c3e02a1453b171d98f93e1ae1763beb3504eaf21d10ce8b555d8f7bb71f4f948` |
| Public Qt runtime archive | `5803d25396f6ed17cb179f66241a209a396e2986fcef07d7a08683f95aa793be` |
| Bundled gallery icon      | `cab067bf7b80610d922299070b649dc414b7939294b16f4f396adbeaf122ff02` |

The 630 selected native/config/app/package files in `source-inventory.json`
match the cross-build source tree. This is a file-hash inventory, not a claim
that every file was manually audited. The runtime stages QtSvg and `qsvg`,
module license texts, third-party notices and an ELF closure report: 191
entries, 21 ELF objects. Its separate QtSvg compiler-read audit found 314 Qt
files with an LGPL option, no Qt file lacking one, and the zlib attribution.
QNX system libraries remain external; no SDP payload or premium component is
redistributed. This scoped build does not close the platform distribution gate.

The authorized Pi 400 runs a custom image, not the stock Pi 4/5 QSTI lane.
Screen is 1920x1080; the established app window stays 1280x800. The standalone
consumer uses `org.rnqnx.svgshowcase` / `RNQnxSvgGallery`. It was launched through
the repository guard after stopping the owner tile, and the tile was restored
after testing. No boot files, window manager or USB input configuration changed.

The gallery bundle and asset were deployed manually. `bundle-qnx` explicitly
builds JavaScript only; the icon was staged at React Native's resolved
`assets/assets/icon.png`. This is not a claim that generic consumer asset
packaging is complete. The native build's original prepared-bundle descriptor
is not evidence for the later manually deployed HBC; the final HBC hash above
identifies the bytes actually run.

## Element and attribute evidence

Screenshots in `docs/evidence/svg-device-2026-11-13/` are direct Screen capture
regions at 320,140, size 1280x800, converted from BMP to PNG without resampling.
They show the app window without desktop/network details. Input uses Screen
virtual-pointer injection; no physical touchscreen or multi-touch claim follows.

| Elements / properties exercised                                                                  | Device observation                                                           | Capture                                   |
| ------------------------------------------------------------------------------------------------ | ---------------------------------------------------------------------------- | ----------------------------------------- |
| Path, Rect, Circle, Ellipse, Line, Polygon, Polyline, G; curves, arcs, closed paths, fill/stroke | Shapes and styled strokes render                                             | `0.png`                                   |
| Defs, LinearGradient, RadialGradient, Stop; units, opacity, transformed paint                    | Gradient fills and strokes render                                            | `0.png`                                   |
| ClipPath, Mask, Use; clipping, luminance mask, references                                        | Clip/mask/reference examples render                                          | `1.png`                                   |
| Text, TSpan; anchors, font, fill/stroke, span positioning                                        | Outlined/gradient/styled text renders                                        | `1.png`                                   |
| Pattern, Symbol, Marker; viewBox, pattern transforms, scaled Use, orient                         | Patterns, scaled checks and arrow markers render                             | `2.png`                                   |
| fillRule/clipRule, dash arrays/offset, caps, joins, vectorEffect, spreadMethod                   | Rule, dash, join and repeat/reflect samples render                           | `2.png`, `3.png`                          |
| TextPath, x/y/dx/dy/rotate, textLength, lengthAdjust, spacing, decorations, baseline shift       | Text follows a wave; spacing and decoration samples render                   | `3.png`                                   |
| Filter primitives, composition and lighting                                                      | Filter swatches render; host reference comparisons remain the precise oracle | `4.png`                                   |
| Image; data PNG, bundled PNG, SVG data, HTTPS JPEG, preserveAspectRatio, clip and opacity        | All source types render, including the remote photo and clipped icon         | `5.png`                                   |
| SvgXml, SvgCss, SvgAst/parse                                                                     | Markup and CSS circles render without the deprecated-export error            | `5.png`, `6.png`                          |
| ForeignObject; transformed live child; ancestor pointerEvents                                    | Enabled count 1, blocked remains 1, enabled again reaches 2                  | `enabled.png`, `blocked.png`, `touch.png` |
| Circle/G onPress, retained prop changes, JS rotation/pause                                       | Circle and group counters reach 1; circle color changes; rotation pauses     | `touch.png`                               |

This is bounded gallery evidence, not every value of every SVG attribute or a
physical-target run of the full 149-case reference corpus. Reanimated animated
props remain the separate PR 149 integration. Frame rate, presentation latency,
memory deltas, Marathon, QNX VM and stock-QSTI results are not qualified here.

## Skills, review and rollback

DRI: current integration agent, acting in the roster's Qt Quick/Fabric,
Metro/Expo developer-tools and QNX regression roles. Ownership is the changed
gallery/test/evidence paths; no other app audit is claimed. Human review is
unassigned. The earlier CodeRabbit review covered the corpus repair with zero
findings; CodeRabbit reviewed this gallery/test/evidence follow-up against a52ffff0
with zero findings. Human review remains unassigned.

| Skill/source                                       | Exact source                                                                        | Applied decision                                                          |
| -------------------------------------------------- | ----------------------------------------------------------------------------------- | ------------------------------------------------------------------------- |
| Repository qnx-native-api, qnx-platform-validation | Read local SKILL.md files                                                           | Retain Fabric/Qt boundary and separate host, cross-build and device proof |
| Margelo api-design → build-nitro-modules → cpp     | `1e9e17be6f41c838db472d6a4b943fc26355a3be`, bytes verified against skills.lock.json | Inspect pinned API; repair its caller and add behavioral regressions      |
| Callstack react-native-best-practices              | `59e9d17446a248b21d86f078fcd0bd2d36db1ca4`                                          | Distinguish retained props, visible pixels and actual input               |
| Installed no-ai-slop                               | Read local SKILL.md                                                                 | Keep results and missing gates explicit                                   |

Security/licensing: no new dependency, permission, premium source or renderer.
Performance budget remains unmeasured; existing texture/filter bounds remain.
Rollback the gallery/import and regression follow-up together; preserve the
corpus sharding repair and historical receipts. Next: exact-head checks and
review, then merge PR 161 before the PR 159 and PR 149 continuations.
