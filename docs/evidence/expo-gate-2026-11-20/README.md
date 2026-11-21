# Expo gate slice S7 receipt, 2026-11-20

Branch `s7/expo-gate`, based on main `b88f2a7b`. Lane D (tooling). All results below are **host** (macOS arm64, Node 26.13.0, pnpm 12.18.0). Nothing here ran on a QNX VM, a Pi 400 or a release board. The `expo` gate in `docs/releases/v1-readiness.json` stays `pending`: it needs a prebuilt runner (S6), Fast Refresh on a device and a standalone package launch, none of which this slice produces.

## What changed

| Item                                      | Files                                                                                                                                                                                |
| ----------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| Second prebuild changes nothing, asserted | `tests/js/expo-prebuild-idempotent.test.mjs`                                                                                                                                         |
| Capability table generator                | `scripts/expo-capability-table.mjs`, output `docs/expo-delivery/sdk58-capability-table.md`                                                                                           |
| Docs-site wiring                          | `docs-site/scripts/generate-data.mjs` (writes `content/docs/reference/expo-capabilities.mdx`), `docs-site/content/docs/reference/meta.json`, `.github/workflows/docs-site.yml` paths |
| CI `--check`                              | `pnpm validate:expo-capability-table` in `.github/workflows/sdk58-validation.yml`; `npm run data:check` in `docs-site.yml` covers the site page                                      |
| Test for the generator                    | `tests/js/expo-capability-table.test.mjs`                                                                                                                                            |
| Stale doc sentences fixed                 | `docs-site/content/docs/reference/modules/expo-image.mdx`, `expo-router.mdx` said the support list marks them `unsupported`; the list marks both `partial`                           |

No file under `packages/react-native-qnx-cli/src` changed, so there is no overlap with S6.

## Second prebuild

"Prebuild" on this platform is `project prepare-native` (Expo config resolved through `@expo/config`, plugins included, then `build/<platform>/native-app.json` and the icon set). The test runs it twice on three projects (one-command create, create with `--with-marathon`, add-to-existing) and compares every file outside `node_modules` by SHA-256, inode and mtime. Byte comparison alone would miss a run that rewrites identical content.

Red-green proof, both logs committed here:

- Green, unmodified source: `prebuild-test-green.txt`, 14 checks pass, exit 0.
- Red, `prepare-native.mjs` made non-idempotent (the `unchanged` branch rewrites the descriptor, `mutation-1.diff`): `prebuild-test-red.txt`, `AssertionError: created-qnx: second run changed a file`, exit 1.
- The first review pass noted that the 15 ms delay in that mutation was doing the work on filesystems with coarse timestamps. The test now pins every generated file to an mtime in 2001 before the second run, and snapshots directories and modes too. Re-proved without any delay: `mutation-2-no-delay.diff`, `prebuild-test-red-no-delay.txt` (fails, exit 1), `prebuild-test-green.txt` (14 checks, exit 0). `prebuild-test-red.txt` is from the earlier version of the test.
- A mutation that re-stages the icons in the `unchanged` branch failed the same assertion, in an interactive run whose output is not committed. All mutations were reverted; `git diff` on `prepare-native.mjs` is empty.

Existing coverage that this adds to: `expo-config-resolution.test.mjs` already compared a tree hash across two runs for fixtures, without mtimes and without the created or added projects.

Limits: the identity tool is the JS stand-in from `tests/js/helpers/identity-tool.mjs` (no Qt on this host), and `node_modules` holds a symlink to the plugin package instead of a pnpm install. `qnx-create-app.yml` runs the real install and the real Qt tool on CI. Set `RNQNX_IDENTITY_TOOL` to run this test against the real tool.

## Create and add-to-existing from a clean host (host)

The first three rows ran in a fresh clone of `746f52d` after `pnpm install --frozen-lockfile` (1m41s). The idempotence test did not exist there; its row ran on this branch (base `b88f2a7b`):

| Command                                           | Result                      |
| ------------------------------------------------- | --------------------------- |
| `node tests/js/qnx-project-create.test.mjs`       | passed, real local archives |
| `node tests/js/qnx-add-to-existing-expo.test.mjs` | 36 checks passed            |
| `node tests/js/expo-config-resolution.test.mjs`   | 56 checks passed            |
| `node tests/js/expo-prebuild-idempotent.test.mjs` | 14 checks passed            |

The real `pnpm install` inside a created app plus both bundles and `prepare-native` with the Qt identity tool is the `qnx-create-app` workflow. Its result is the CI run on this PR, not a local claim.

## Capability table

`docs/expo-delivery/sdk58-capability-table.md` and the docs-site page are written by `scripts/expo-capability-table.mjs` from `docs/expo-delivery/sdk58-support.json`. Mapping, with 124 packages:

| Support-list status      | Capability   | Count |
| ------------------------ | ------------ | ----- |
| `platform`, `qnx-native` | works        | 1     |
| `js-only`                | JS-only      | 8     |
| `partial`                | shim         | 4     |
| `unsupported`            | unsupported  | 101   |
| `host-tooling`           | host tooling | 6     |
| `no-op`                  | not bundled  | 4     |

`host tooling` and `not bundled` are not among the four words in the brief. Forcing those ten packages into one of the four would be wrong, so they have their own rows. `works` has one member (`react-native`) because the support list is a published-contents scan and no Expo package declares `qnx` yet; the `shim` rows (`expo-constants`, `expo-image`, `expo-modules-core`, `expo-router`) are the ports this repository supplies. The table says in its header that no row is a device run.

`--check` fails when the file differs (red: a hand-edited table exits 1, `capability-check-red.txt`; green after restoring it, `capability-check-green.txt`). `tests/js/expo-capability-table.test.mjs` also asserts a label exists for every status, one row per package, counts add up, five known rows, pipe escaping and the CLI exit codes.

## Expo Router

Not run: no board. `apps/expo-router-example` is still a `.gitkeep` scaffold; the Router app that ships is `apps/component-gallery` (drawer plus tabs, `@rn-qnx/expo-router-qnx`). Host results:

- `node tests/js/expo-router-qnx.test.mjs` passes (resolver, tab bar model, swipe exclusion).
- `bundle-qnx --app component-gallery` produced a production Metro bundle for the `qnx` platform: 3,398,900 bytes, sha256 `04db26a1...a00dc` (`component-gallery.bundle.sha256`, `router-gallery-bundle.txt`). This proves the Router graph resolves for `qnx`. It does not prove it renders.
- Router on the Qt Quick host renderer was not run: the host renderer preset is Linux and building Hermes and Qt for it was out of scope here.

Device run: **not run: no board.** A QEMU VM is being built by another agent.

## Environment note

Watchman hangs on `/Volumes/QNX` (`Waiting for Watchman watch-project` past 270 s), which stalls every Metro run. Runs here put a PATH without `watchman` in front so Metro falls back to its node crawler. This is a host setup issue, not a repository change.

## Skill table

| Skill                          | Source, SHA                                                                                | Read                                   | Decision it changed                                                                                           | Artifact                             | Rejected alternative                                     |
| ------------------------------ | ------------------------------------------------------------------------------------------ | -------------------------------------- | ------------------------------------------------------------------------------------------------------------- | ------------------------------------ | -------------------------------------------------------- |
| no-ai-slop                     | `~/.claude/skills/no-ai-slop` (not pinned by SHA)                                          | full                                   | Receipt and generated-table prose use plain statements and say what was not run                               | this file                            | none                                                     |
| expo-router                    | expo/skills plugin `expo` 1.13.9 (installed copy; pinned `a9e5bd3f...` not checked out)    | `SKILL.md`; references not opened      | None. Its guidance targets native Expo Router; this repo ships `@rn-qnx/expo-router-qnx` and a Metro resolver | bundle hash above                    | Following its NativeTabs layout: not available on QNX    |
| expo-module                    | same                                                                                       | `SKILL.md`                             | None. No module was authored; one capability row per package comes from the support list                      | capability table                     | Hand-written per-package rows                            |
| expo-upgrade                   | same                                                                                       | `SKILL.md`                             | SDK 58 is pinned already; the support list drift check (`--check`) stays the guard                            | `validate:expo-sdk58-support` passes | Re-running `--write` (needs network; nothing drifted)    |
| expo-dev-client                | same                                                                                       | `SKILL.md`                             | None. EAS dev clients do not apply to a QNX runner                                                            | none                                 | n/a                                                      |
| upgrading-react-native         | callstack agent-skills plugin cache `fa8694ceaa30` (pinned `59e9d174...` not checked out)  | `SKILL.md`; reference files not opened | None this slice: no RN version change. The `dependencies` gate check against QNX overrides is not done here   | none                                 | Treating a table change as a dependency check            |
| react-native-best-practices    | `~/.claude/skills/react-native-best-practices` (Software Mansion), not the Callstack skill | `SKILL.md` index only                  | None: no performance change in this slice                                                                     | none                                 | n/a                                                      |
| writing-user-docs              | not installed                                                                              | n/a                                    | Procedure followed by hand: every command in this receipt was run from a fresh clone                          | logs in this directory               | n/a                                                      |
| systematic-debugging           | `~/.claude/skills/systematic-debugging`                                                    | full                                   | Watchman stall and the `pnpm` PATH failure were traced to the environment before any change                   | Environment note                     | Raising test timeouts                                    |
| verification-before-completion | `~/.claude/skills/verification-before-completion`                                          | full                                   | Red-green run for both new checks                                                                             | `prebuild-test-red.txt`              | Reporting the test as passing without a failing mutation |

The installed skill copies differ from the pins in `docs/expo-delivery/skills.lock.json`. No claim is made that the pinned revisions ran.

## Full JavaScript suite

`node scripts/run-js-tests.mjs` on this branch after merging main `b88f2a7b`: all 95 test files pass, exit 0 (`test-js.txt`). A first run failed on host setup only: Watchman hang (see above) and `pnpm` missing from the trimmed PATH in `braces-security.test.mjs`.
