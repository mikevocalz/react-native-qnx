# Expo Image resize and API device check

Implementation base: `c2badc4`, PR 159. All hosted checks passed at that head,
including renderer OFF (26m32s), renderer ON (31m53s), native consumer,
image-cache, Metro, dependencies and validation. The new gallery driver needs
its own head checks. [Renderer run](https://github.com/mikevocalz/react-native-qnx/actions/runs/43534073125).

The driver uses the installed expo-image 58.0.18 public `Image` API: clear
memory/disk cache, disk prefetch, cache path, loadAsync, blurhash and view
animation commands. The installed `src/Image.tsx` and native module/view
contracts were inspected. No dependency or native API change was needed.
Application state uses a per-instance vanilla Zustand store. View handles and
ImageRef resources stay in refs; cleanup releases the resource and invalidates
pending API results.

## Target and artifacts

QNX SDP 8.0.5 AArch64 cross-build, public-source Qt 6.12.0, Release/O2, two
jobs. This inherited cross graph has warnings-as-errors and sanitizers OFF.
Expo Image and native UI are enabled; SVG, Worklets and Reanimated are OFF.
The ELF requires `libhermesvmlean.so`. The test uses the previously staged
public Qt runtime with image-format/TLS plugins, not an evaluation build.

| Artifact | SHA-256                                                            |
| -------- | ------------------------------------------------------------------ |
| Consumer | `d23d66439e43ae3081bf01c106c7f8beab8f2b8b49efd5a1ea794995d84cfe70` |
| HBC      | `0da7041c179c7f93257697ce8e7b4fcf4e4499444501d0432a465cf5c8474cb7` |

`prepare-native` recorded this HBC hash, app ID `org.rnqnx.showcase`, component
`RNQnxPlayground`, and the unchanged 1280x800 window. The first deployment
loaded the copied owner's older `app.bundle.js` by default; that run is excluded
from the new-driver results. Relaunching with explicit `--bundle .../app.hbc`
selected the recorded candidate. The guarded launcher replaced the existing
instance; no boot, window manager or USB input configuration changed.

The board is the authorized Pi 400 custom image. It does not qualify the stock
Pi 4/5 QSTI, Marathon, or QNX VM lanes. Assets were copied into the consuming
app's resolved asset directory; generic asset packaging remains separate.

## Observed results

The native Screen utility captured the window at 320,140, size 1280x800.
BMP files were converted to PNG without resizing. Evidence is under
`docs/evidence/expo-image-device-2026-11-13/`.

| Action                                              | Result                                                                                       | Evidence                                 |
| --------------------------------------------------- | -------------------------------------------------------------------------------------------- | ---------------------------------------- |
| Initial still, 20x20                                | One load, source metadata 1024x1024                                                          | `initial.png`                            |
| Grow the same mounted image to 240x240              | Two loads; clear enlarged icon                                                               | `grown-apis.png`                         |
| Clear memory, clear disk, disk prefetch, cache path | true, true, true, present                                                                    | `grown-apis.png`                         |
| loadAsync and render ImageRef                       | 1024x1024 resource and visible preview                                                       | `grown-apis.png`                         |
| Generate 4x3 blurhash                               | 28 characters; API sequence completes                                                        | `grown-apis.png`                         |
| Start GIF after autoplay=false                      | Red then green pixels in successive captures                                                 | `gif-running-a.png`, `gif-running-b.png` |
| Stop GIF                                            | Yellow frame persists; the two complete BMP captures compare byte-for-byte equal after 500ms | `gif-stopped-a.png`, `gif-stopped-b.png` |

The source dimensions in onLoad are metadata, not decoded-allocation metrics.
The host regression separately checks the adequate decode after growth and
the initial-load race. The device screenshots prove the second load and
visible behavior. No frame-rate, input-latency or RSS measurement follows.

## Review, limits and rollback

DRI: current integration agent, owning these gallery and evidence paths.
CodeRabbit reviewed both driver files with zero findings; human review is
unassigned.
Repository qnx-native-api and qnx-platform-validation were read. The ordered
Margelo api-design, build-nitro-modules and cpp sources were byte-verified at
`1e9e17be6f41c838db472d6a4b943fc26355a3be`; they kept the repair inside the
existing Qt/Fabric image view. No Nitro generation change was required.
no-ai-slop was applied to this receipt. Host typecheck passes with the driver.

The loader still needs GUI-thread I/O/decode work, corrupt-cache recovery,
bounded redirects and shared network-manager review. Navigation/RTL, keyboard
interaction, full prop conformance and animated WebP remain open. This check
does not close those items or the release distribution/performance gates.
No premium payload or QNX SDK artifact is committed. The test adds no default
runtime feature, permission or production performance cost.

Rollback: revert the two gallery driver files and this receipt/evidence; the
native resize fix is a separate commit. Use the guarded launcher to restore
the recorded owner app, retaining the established window contract.
