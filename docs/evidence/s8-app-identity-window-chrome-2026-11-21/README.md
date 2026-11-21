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

## Reviews (lanes other than the author)

| Reviewer            | Target                                   | Result                                                                                                                                                                                                                                                                                           |
| ------------------- | ---------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| `ecc:cpp-reviewer`  | `native/window-chrome` diff              | No critical or high findings. Folded in: layout-name signal ordering, GUI-thread assertion, dead-window guard, two comments. Rejected: dedicated inset signal (no consumer yet), `light` test hook kept with a comment.                                                                          |
| `ecc:code-reviewer` | docs, contrast.json, evidence, readiness | One high (stale generated docs-site API pages) and one medium (how-to page) fixed; wording nits fixed.                                                                                                                                                                                           |
| `ecc:architect`     | windows API contract, revisions 1 and 2  | Revision 1: five blocking findings. Revision 2: one critical (a cancelled system close under rnqnx-winmgr still ends in SIGTERM), one regression, a SafeAreaView plan that missed the JS shim, ordering gaps. All folded into revision 3, which still needs a third read before any spec or C++. |

`pnpm validate:docs` and the docs-site `generate-api --check`, `generate-data --check` and
`check-content` pass. `node scripts/run-js-tests.mjs` was started and stopped at
`braces-security.test.mjs`, which needs a corepack-installed pnpm 12.18.0 that this host lacks; the
tests before it passed.
