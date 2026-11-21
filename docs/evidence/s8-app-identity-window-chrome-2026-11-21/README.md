# S8: window chrome host evidence, 2026-11-21

Scope: runtime layout switch, title-bar inset model and contrast fixes in
`native/window-chrome`. Everything here ran on the **host** (macOS, Qt 6.12.0,
offscreen QPA, software Quick backend). No QNX VM, Pi 400 or release board run
was made, so the `window-chrome` and `app-identity` gates stay `pending`.

Base `19fa1b2`. Build: `cmake -S native/window-chrome -DRNQNX_BUILD_WINDOW_CHROME_TESTS=ON`
with Ninja, Debug; a second build with `-DRNQNX_WINDOW_CHROME_SANITIZERS=ON`.

## Results

| Check                                                                       | Result                                                                                       |
| --------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------- |
| `ctest` on the unmodified base                                              | 15 of 15 passed                                                                              |
| `ctest` with the changes                                                    | 15 of 15 passed                                                                              |
| `ctest` with the changes, ASan and UBSan                                    | 15 of 15 passed                                                                              |
| `rnqnx_app_identity` lane on the unmodified base (no identity code changed) | 3 of 3 passed                                                                                |
| Dark focus ring reverted to `#0a64d6`                                       | `focusRingContrastMeetsNonTextMinimum` fails at 2.98491                                      |
| `applyTabOrder()` removed from `setControlsLayout`                          | `runtimeLayoutSwitchKeepsFocusAndTabOrder` fails: Tab enters at `close`, expected `minimize` |

New tests: `runtimeLayoutSwitchKeepsFocusAndTabOrder`,
`runtimeLayoutSwitchRejectsUnknownNames`, `runtimeLayoutSwitchOnQnxKeepsLiveControls`,
`titleBarInsetFollowsStyleAndLayout`, `focusRingContrastMeetsNonTextMinimum`,
`rendersSwitchAndFocusFrames` (Quick) and `titleBarInsetPerStyle`,
`controlOrderIsPhysical` (model).

`focusRingContrastMeetsNonTextMinimum` caught the first dark inactive-title
proposal (`#7d8a99`, 4.465:1, below 4.5:1); the committed value is `#8391a1`.

## Frames

`frames/` holds PNGs from `rendersReferenceFrames` and `rendersSwitchAndFocusFrames`,
480x160, 1x. `focus-macos-*` is the macOS layout with keyboard focus on minimize;
`focus-switched-windows-*` is the same window after `setControlsLayout("windows")`,
focus kept on minimize. A first capture showed the Windows close cell red because
the stationary test cursor was over the old close position; the test now waits for
the hover to clear before grabbing.

## Not covered

- Delivery of the inset and the layout switch to JavaScript (no windows API yet).
- Minimize and restore with `AppState`, close guards, blocked-JS operation, drag and
  resize support per QPA, both layouts on a device.
- Tile label fit and the icon in every location on a device (`app-identity`).
- Rendered contrast on QNX; the ratios are from solid colours, see
  `docs/v1/design/contrast.json` (`s8Pairs`).
