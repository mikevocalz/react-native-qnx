# apps/expo-router-example, host results, 2026-11-22

Base: main `ae0040d3`. Every result in this directory is **host** (macOS arm64, Node 26, pnpm 12.21.0, `pnpm install --frozen-lockfile`). Nothing ran on the QNX VM, a Pi 400 or the release board. The `expo` gate in `docs/releases/v1-readiness.json` stays `pending`, and no acceptance text or budget changed.

## What was added

`pnpm qnx init expo-router-example` created the app shell in `apps/expo-router-example`. The routes come from `template/qnx-router`, with `app.json` (name, scheme, icon, `extra.rnqnx`), the router Metro config used by `apps/component-gallery`, a package manifest and a `pnpm-lock.yaml` importer. `qnx-create-app.yml` now bundles the app and runs `tests/js/expo-router-example.test.mjs` with the real identity tool. `host-renderer.yml` has a render step for the app; see the status below.

## Results

| Check                                                                    | Label | Result                                              | Log                                                                          |
| ------------------------------------------------------------------------ | ----- | --------------------------------------------------- | ---------------------------------------------------------------------------- |
| Expo SDK 58 support drift check                                          | host  | passes, before and after the change                 | `check-expo-sdk58-support-before-change.txt`, `check-expo-sdk58-support.txt` |
| Capability table check                                                   | host  | passes                                              | `check-expo-capability-table.txt`                                            |
| Second prebuild changes nothing, existing projects                       | host  | passes                                              | `test-expo-prebuild-idempotent.txt`                                          |
| Second prebuild changes nothing, this app (copy, stand-in identity tool) | host  | passes, 39 checks                                   | `test-expo-router-example.txt`                                               |
| Router graph tests                                                       | host  | passes                                              | `test-expo-router-qnx.txt`                                                   |
| Project create                                                           | host  | passes                                              | `test-qnx-project-create.txt`                                                |
| Production bundle for `qnx`                                              | host  | 1,433,569 bytes, sha256 in `host-bundle-sha256.txt` | `host-bundle-sha256.txt`                                                     |
| architecture, manifests, dependencies, format, typecheck                 | host  | pass                                                | `check-*.txt`                                                                |

The prebuild check here uses the JS stand-in identity tool. CI runs it again with the real Qt tool in `qnx-create-app.yml`.

## Not shown

- Offscreen render of the app: no host renderer binary was built on the Mac (the renderer lane is Linux only). The CI step in `host-renderer.yml` is the only render attempt; its outcome is recorded in the pull request, not here.
- The app running on the Pi 400, the QNX VM or a release board: pending.
- Fast Refresh on a device and the end-to-end recording: pending (Pi 400 or release board).
- A published runner, the standalone package launch and item 6 (Expo CLI owning Metro): unchanged from `docs/evidence/expo-gate-2026-11-21/README.md`.
