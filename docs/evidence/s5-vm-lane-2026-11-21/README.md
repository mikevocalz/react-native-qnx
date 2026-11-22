# S5 VM lane: validate-qnx-vm.sh runs, 2026-11-21

**Target: QNX VM (QEMU, TCG, software render). Not the Pi 400, not the release
board.** TCG timings are not performance evidence. No gate in
`docs/releases/v1-readiness.json` changes status, and no budget changes.

The script, the workflows and the runner labels are described in
[QNX cross-build and VM validation lanes](../../ci/qnx-vm-lane.md).

## What ran

`scripts/validate-qnx-vm.sh` at repo commit `6d573871`, tree clean, on the
maintainer's Mac against the QEMU VM (`QNX qnxvm 8.0.0 2026/06/05-16:21:14EDT
QEMU_virt aarch64le`). Each directory holds `result.json` and `SHA256SUMS`;
verify with `cd <dir> && shasum -a 256 -c SHA256SUMS`.

| Run                                                      | Command                                                  | Result | Notes                                                                                                        |
| -------------------------------------------------------- | -------------------------------------------------------- | ------ | ------------------------------------------------------------------------------------------------------------ |
| [run1](run1/result.json)                                 | `--boot always --keep-running`                           | pass   | Stopped the running VM, booted it, ssh answered after 16 s.                                                  |
| [run2](run2/result.json)                                 | no options                                               | pass   | Attached to the running VM.                                                                                  |
| [run3](run3/result.json)                                 | no options                                               | pass   | Attached.                                                                                                    |
| [extra-stage](extra-stage/result.json)                   | `--stage <copy of the guest app, js-thread and bundles>` | pass   | Pushed 62 MB with tar over ssh, host and guest hashes matched for every file, then ran from the pushed copy. |
| [extra-shutdown](extra-shutdown/result.json)             | `--shutdown --skip-captures --skip-js-thread`            | pass   | ssh stopped answering and the step passed.                                                                   |
| [extra-boot-from-down](extra-boot-from-down/result.json) | `--keep-running`                                         | pass   | VM was down. Booted by the script, ssh answered after 11 s. All steps passed.                                |

The three counted runs are run1 to run3. Every run exited 0.

## Numbers, runs 1 to 3

Seconds are TCG wall time from `result.json`.

| Step      | run1 | run2 | run3 | What passed                                                                                                     |
| --------- | ---- | ---- | ---- | --------------------------------------------------------------------------------------------------------------- |
| attach    | 16   | 0    | 0    | run1 includes stopping the old VM and booting a new one                                                         |
| deploy    | 3    | 3    | 3    | 11 guest files hashed (app, test binary, both `libhermesvm.so`, both runners, 5 bundles)                        |
| captures  | 11   | 13   | 10   | 5 of 5 cards: app exit 0, 1280x800 PNG, 22984 to 45722 bytes, screenshot marker in the app log, 5 distinct PNGs |
| js-thread | 18   | 18   | 18   | 11 `[PASS]`, 0 `[FAIL]`, `All 11 JS-thread tests passed.`, binary exit 0                                        |

The five PNG hashes are the same in all three runs (`virtualized-list` 51c12dc8,
`virtualized-section-list` 1818fb36, `button` b4e67b3c, `safe-area-view` b7d35980,
`image-background` b9565d3a), so the render is deterministic on this VM. The
showcase binary hash on the guest is `4f465406...f69cf`, the one recorded in
the [2026-11-20 deploy](../qnx-vm-2026-11-20/vm/deploy.sha256).

JS blocked 5 s, flag on, from the JS-thread test's own `[RESULT]` line:

| Value                     | run1 | run2 | run3 |
| ------------------------- | ---- | ---- | ---- |
| Block length (ms)         | 5021 | 5019 | 5019 |
| 10 ms heartbeats by 2.5 s | 197  | 197  | 198  |
| Pan duration (ms)         | 96   | 102  | 93   |
| Minimize control (ms)     | 12   | 12   | 12   |
| Close control (ms)        | 6    | 6    | 6    |

With the flag off, the same block produced 0 heartbeats during the block and
the queued close click ran at 5005 ms in all three runs. These match the
[S2 receipt](../s2-vm-2026-11-21/README.md) within a few milliseconds. Timings
under TCG are not performance evidence.

## Checks on the script itself

Each of these made the script exit non-zero, and was run by hand:

| Mutation                                                    | Exit | Failing step and detail                                     |
| ----------------------------------------------------------- | ---- | ----------------------------------------------------------- |
| `RNQNX_VM_JS_THREAD_EXPECTED=99`                            | 1    | `js-thread`: `11 [PASS] lines, expected at least 99`        |
| `RNQNX_VM_CARDS="button no-such-card"`                      | 1    | `captures`: `no-such-card: no PNG; app exit 1; 0 bytes ...` |
| `--expect-sha256` with a wrong hash for the showcase binary | 1    | `deploy`: sha256 mismatch                                   |
| `RNQNX_VM_SSH_PORT=2299 --boot never`                       | 3    | `attach`: VM is down and `--boot never` was given           |

These four runs were made on the working tree before the last commit and their
output is not kept here.

A first `--stage` run failed: macOS `tar` added AppleDouble `._*` members, so
the guest hashes differed from the host list and `deploy` failed. The script now
sets `COPYFILE_DISABLE=1` for that tar, and extra-stage is the passing rerun.

## Cross-build, run by hand

[cross-build-local](cross-build-local/validate-qnx-805-cross.output.txt) is
`scripts/validate-qnx-805-cross.sh` with `RNQNX_QNX_CONFIGS=Release`, run in the
`qnx805-build` container on a `git archive` of this branch copied to the
container's `/tmp` (deleted afterward). It configured and built
`rnqnx_g0_cxx20_probe`, `qnx_platform` and `rnqnx_playground` for
`qnx-aarch64-release` and checked that both executables are AArch64 ELF. It
passed, with `_NTO_VERSION=800` and `gcc_ntoaarch64le` available in `qcc -V`.
The Debug configuration, which the workflow also builds, was not run. This is a
local run, not the self-hosted workflow, and it proves cross-compilation only.

## GitHub Actions

| Item                                           | State                                                                                                                    |
| ---------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------ |
| Self-hosted runners registered                 | None. `gh api repos/mikevocalz/react-native-qnx/actions/runners` returned `total_count: 0`.                              |
| `qnx-cross-build.yml`, `qnx-vm-validation.yml` | **Never ran.** Parsed by `actionlint` and skipped in the `v1-candidate` runs below because `RNQNX_SELF_HOSTED` is unset. |
| `v1-candidate.yml`                             | See the run links in the pull request. The result of each dispatch is recorded there.                                    |

## Not run, and why

- Both self-hosted jobs on GitHub: no runner exists. Their commands ran by hand, as above.
- Debug cross-build: only Release was built by hand.
- Cross-build to VM artifact hand-off: the cross-build lane does not build Qt or the showcase app, so the VM lane uses the binaries already on the guest.
- Pi 400, release board, real pointer or touch input, any performance claim.

## Finding

History: when this lane was written, the [S2 receipt](../s2-vm-2026-11-21/README.md)
linked `build/` files and listed `build/` and `vm/run1` to `run3` entries in its
`SHA256SUMS`, but those files were not in the repository: `*.log` is git-ignored,
and the `build/` directory had not been added. PR #205 recovered them, and
`shasum -a 256 -c SHA256SUMS` in the S2 folder passes for all 16 entries
(host check at f61b61b4). This lane's change did not touch that receipt.
