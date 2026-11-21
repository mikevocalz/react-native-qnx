# S10 host developer path and documentation checks, 2026-11-21

Label: host only. Nothing here ran on a QNX VM, the Pi 400 or a release board,
and no gate changes status because of it.

Host: macOS 27.0 arm64, Node 26.13.0, pnpm 12.18.0 installed with the README's
prefix route (`npm install --prefix ~/pnpm12 pnpm@12.18.0`, then
`node ~/pnpm12/node_modules/pnpm/install.js`). Source: a fresh `git clone` of
`main` at `19fa1b25`. The run was made by an agent that did not write the docs
and followed the README quick start and `docs/getting-started/qnx-cli.md`
literally.

## Commands run and results

| Command                                                                                  | Result                                                                                                    |
| ---------------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------- |
| `pnpm install --frozen-lockfile`                                                         | exit 0, 7.5 s                                                                                             |
| `pnpm qnx init dashboard`                                                                | exit 0, "Created apps/dashboard"                                                                          |
| `pnpm install --no-frozen-lockfile`                                                      | exit 0, 4.2 s; `pnpm-lock.yaml` changes and `apps/dashboard/` is untracked, as the doc says               |
| `pnpm qnx bundle-qnx --app dashboard --out build/dashboard.bundle.js`                    | exit 0, "Done writing bundle output"                                                                      |
| `pnpm qnx doctor`                                                                        | exit 1 on this host: no QNX SDP. It names the fix (`source <qnx-installation>/qnxsdp-env.sh`) and the log |
| `pnpm qnx project create --project ../ext/notes-qnx ... --dry-run`                       | exit 0, JSON plan with `nativeBuildVerified: false`                                                       |
| `pnpm qnx analyze --project apps/component-gallery`                                      | matches `runner-tiers.md`: two Expo modules, two Fabric components, `Stock runner: undetermined`          |
| `pnpm qnx runner fetch --help`                                                           | prints usage; states that no manifest is published                                                        |
| `pnpm validate:dependencies`, `pnpm audit:dependencies`, `pnpm typecheck`                | all exit 0                                                                                                |
| `pnpm test:js`                                                                           | "All JavaScript tests passed.", exit 0 (host)                                                             |
| docs-site: `data:check`, `lint`, `types:check`, `build`, `check:content`, `check:search` | all pass; `content ok: 163 pages`, `search ok: identical search triggers on 6 pages`                      |
| `pnpm validate:manifests`                                                                | 14 manifests, 7 scaffold-only directories                                                                 |
| `npm pack --dry-run --json` in each package directory                                    | see [the pack record](s10-distribution-2026-11-21/pack-dry-run.txt); no SDK, key, env or archive paths    |

Main was green at `19fa1b25` on the hosted workflows the `gh run list` call
returned (SDK 58 validation, host renderer, docs pages, Expo, Nitro and Hermes
source checks, performance budgets). That is a hosted result, not a QNX
cross-build.

## Defects found and fixed

1. Two getting-started pages linked to README anchors that no longer exist:
   `qnx-cli.md` to `#start-from-source` and `build-and-deploy.md` to
   `#know-what-is-supported`. Both now point at the current headings.
2. Root cause: `scripts/lib/docs-model.mjs` checked that link targets exist as
   files, for a fixed list of 11 documents, and never checked fragments. It now
   covers every Markdown file under `docs/getting-started` and `docs/v1`, skips
   fenced code, and fails when a `#fragment` is not a heading in the target
   Markdown file. Red and green were both run: with the two pages restored to
   their old text, `pnpm validate:docs` fails with
   `build-and-deploy.md links to #know-what-is-supported, which is not a heading in README.md`.
3. Three docs under `docs/v2` and the ADR template have dead links. `docs/v2`
   is frozen for v1 and the ADR template links to a placeholder on purpose, so
   they are left alone and stay outside the checker.

## Not run

- The CMake configure, build and offscreen render of the quick start. A full
  Hermes, folly and Boost build used about 5.7 GiB in the
  [2026-11-19 receipt](quickstart-host-2026-11-19.md); free disk here was 13 GiB
  against an 8 GiB stop line. That receipt (commit `8d7deaa`) is the only proof
  of those three commands. They were not re-proved on `19fa1b25`.
- `project create` without `--dry-run`, `project add-qnx`, `project prepare-native`
  and `project build-native`, which need the install of a generated app.
- Host C++ tests (`pnpm test:cpp:host`).
- Any step that needs the QNX SDP, QEMU, the Pi 400 or the T9 (`/Volumes/QNX`).
- The timed create, build, deploy and run on a board. The `developer-path` gate
  needs a deploy and run on the release board, so it stays pending.
