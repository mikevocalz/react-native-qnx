# `dependencies` gate re-check, 2026-11-21

Candidate: `main` at `fb20c3adad0cb2aacbc92eb97e0d5a5acc04bf3e` (merge of #205). The re-check
changes no manifest or lockfile, so these results describe that dependency graph.

Label for every result below: **host** (macOS, Node 26.13.0, pnpm 12.21.0) or **hosted**
(GitHub-hosted ubuntu-24.04). Nothing here ran on the QNX VM, the Pi 400 or the release board.

## Commands (host)

| Command                               | Exit             | File                                                    |
| ------------------------------------- | ---------------- | ------------------------------------------------------- |
| `pnpm install --frozen-lockfile`      | 0                | not saved (tail showed `Done in 1m 22.3s`)              |
| `pnpm validate:dependencies`          | 0                | validate-dependencies.txt                               |
| `node scripts/audit-dependencies.mjs` | 0                | audit-dependencies.txt (0 advisories, 646 dependencies) |
| `pnpm typecheck`                      | 0                | typecheck.txt                                           |
| `pnpm validate:expo-sdk58-support`    | 0                | validate-expo-sdk58-support.txt                         |
| `pnpm validate:expo-capability-table` | 0                | validate-expo-capability-table.txt                      |
| `pnpm validate:qnx-expo-modules`      | 0                | validate-qnx-expo-modules.txt                           |
| `pnpm validate:qnx-expo-autolinking`  | 0                | validate-qnx-expo-autolinking.txt                       |
| `pnpm validate:docs`                  | 0                | validate-docs.txt                                       |
| `node scripts/run-js-tests.mjs`       | 0                | test-js-exit.txt, test-js-tail.txt                      |
| `pnpm outdated -r`                    | 1 (items listed) | pnpm-outdated.txt                                       |

`pnpm audit:dependencies` through the global pnpm shim on this machine failed with
`spawnSync pnpm ENOEXEC` because the shim is a broken v10. The audit script was run directly
with pnpm 12.21.0 first on PATH. The hosted `sdk58-validation / validate` job also runs the audit.

## Hosted

`v1-candidate.yml` dispatched on `main` with `run_hosted=true` and `require_qnx_lanes=false`:
[run 44951214656](https://github.com/mikevocalz/react-native-qnx/actions/runs/44951214656),
head `fb20c3adad0cb2aacbc92eb97e0d5a5acc04bf3e`, conclusion `success`. All 21 hosted jobs and the
aggregate passed (hosted-run-44951214656-jobs.txt). The `qnx-cross` and `qnx-vm` lanes were skipped
because no self-hosted runner is registered; they belong to the `ci` gate, not this one.

## Hold reasons checked against npm and the installed packages

hold-verify.txt. Figures that differ from the 2026-11-11 baseline: react-native `next` is
`0.89.0-rc.2` (was rc.1); hermes-compiler `latest-v1` is `260318099.0.8` (was .0.7).

## Not closed by this gate

- Qt host floor: `Decide` row, owner decision.
- Qt device: `Rebuild` row, belongs to the `distribution` gate.
- QNX SDP 8.0.5: `Re-check` row, not verified here.
