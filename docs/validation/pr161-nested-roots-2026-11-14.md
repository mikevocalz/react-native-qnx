# PR 161 nested SVG continuation

Base: `d38097f38d5a1ebb560627649e95c84d04d32f02` on
`feat/rnsvg-close-gaps`. The dependency lock SHA-256 is
`4198fb84cd0abd9e8da568af4741a12c2318280827d3b823727e92ed98b2b565`.

## Behavior and source boundaries

The earlier outward invalidation fix covers prop changes. A direct child
removed from a nested root still called `update()` on the hidden root. This
continuation sends child additions and removals through the same outward
invalidation path. The real-window regression removes a painted child and
requires the visible root to repaint its former pixels white.

`Bounds::nestedBox` previously skipped a root directly inside another nested
root. An enclosing group with opacity could therefore allocate no paint layer
for that content. Bounds now traverse both element and root children. Each
nested viewBox transform and viewport clip applies at its own level, with the
existing recursion limit. The regression checks the transformed bounds,
translucent pixels and clipped pixels through two nested viewBoxes.

Changes are confined to `native/qt/rnsvg/RnsvgItems.cpp`,
`native/qt/rnsvg/scene/Bounds.cpp` and their existing scene test. Fabric,
Hermes and Qt Quick remain the backend; the SVG option remains default OFF.
There is no public API, dependency, target-toolchain or scheduling change.

## Validation

Local environment: Debian 13 x86-64, Node 26.13.1, pnpm 12.18.0.
Selected command output is in [local-checks.txt](../evidence/pr161-nested-roots-2026-11-14/local-checks.txt).

| Command or check                                       | Result  | Scope                                                                  |
| ------------------------------------------------------ | ------- | ---------------------------------------------------------------------- |
| `pnpm install --frozen-lockfile`                       | passed  | Exact 522-package graph; no lockfile edit.                             |
| `pnpm validate:dependencies`                           | passed  | Manifest pins and installed source contracts.                          |
| `pnpm typecheck`                                       | passed  | Root and adaptive TypeScript projects.                                 |
| `node tests/js/rnsvg-corpus-packing.test.mjs`          | passed  | All 149 fixtures, 16 batches, four nonempty shards.                    |
| `node tests/js/rnsvg-overlay-render.test.mjs`          | passed  | Actual Metro render: 57 QNX probes and 57 unchanged iOS probes.        |
| `git diff --check`                                     | passed  | Changed source and tests.                                              |
| Native scene regression and baseline negative controls | pending | Requires the exact host Qt installation.                               |
| QNX cross-build and physical-device execution          | not run | No licensed SDP or connected target in this Cloud environment.         |
| Marathon/Wayland execution                             | not run | No target-specific implementation change; no target evidence promoted. |

At the original head, [renderer run 43542289743](https://github.com/mikevocalz/react-native-qnx/actions/runs/43542289743)
passed its OFF configuration but timed out on corpus shards 1 and 2 in the ON
configuration, each at the existing 300-second limit while mounting batches
13 and 14. The other eight workflows passed. The production native source
was unchanged from the earlier passing `a52ffff` head. A targeted hosted
rerun is separate evidence; this fix does not claim to explain or repair the
corpus timing. No timeout, fixture, pixel tolerance or required gate changes.
The original failure's [log excerpt](../evidence/pr161-nested-roots-2026-11-14/original-ci-excerpt.txt)
retains its timestamps and job identity.

## Skills and review

The integration agent owns the two native paths and scene regressions in the
Qt Quick / Fabric and QNX regression roles. Independent review is pending.
External CodeRabbit CLI authentication in this Cloud environment returned
`environment_unsupported`; no new service review is claimed.

| Source read                                               | Revision                                   | Applied decision                                                                                                     |
| --------------------------------------------------------- | ------------------------------------------ | -------------------------------------------------------------------------------------------------------------------- |
| Repository `qnx-native-api` and `qnx-platform-validation` | Base above                                 | Preserve the existing owner thread and renderer; label host and target evidence separately.                          |
| Margelo `api-design` → `build-nitro-modules` → `cpp`      | `1e9e17be6f41c838db472d6a4b943fc26355a3be` | Read in order; preserve the public API and generated bindings, use the existing item lifetime and bounded traversal. |
| Callstack `react-native-best-practices`                   | `59e9d17446a248b21d86f078fcd0bd2d36db1ca4` | Require measured evidence before changing the corpus or optimizing the mounting path.                                |

Upstream skills were read through the connected source provider; no local
skill installation or upstream author participation is claimed. The locked
React Native, SVG and Nitro interfaces were inspected locally after install.

## Security, limitations and rollback

No new I/O, permissions, premium payload, SDK files or redistributed Qt
artifacts are introduced. Nested bounds retain the existing depth cap.
No production performance budget or release gate is promoted. Existing
device receipts stay attached to their original revisions. The fresh combined
QNX build and target acceptance recorded in the PR remain outstanding.

Reverting this continuation restores the preceding invalidation and nested
bounds behavior. Rebuild the native app if deploying either revision; no
JavaScript API or bundle-format migration is involved. The next acceptance
step is the changed native scene suite, independent review and exact-head
required CI, followed by the existing combined QNX target qualification.
