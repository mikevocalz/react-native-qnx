# QNX cross-build and VM validation lanes

Two workflows run on self-hosted runners, and one workflow chains them with the
hosted checks. They are not required checks and they never run on a pull
request, so a repository with no self-hosted runner is not blocked by them.

Every VM result is labelled **QNX VM (QEMU, TCG, software render)**. It is not
Pi 400 evidence and not release-board evidence, and TCG timings are not
performance evidence. No lane here changes a gate in
[v1-readiness.json](../releases/v1-readiness.json).

| Workflow                                                                 | Runs on                                   | What it runs                                                                                                                                     |
| ------------------------------------------------------------------------ | ----------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------ |
| [`qnx-cross-build.yml`](../../.github/workflows/qnx-cross-build.yml)     | `self-hosted, Linux, X64, rnqnx-qnx-sdp`  | `scripts/validate-qnx-805-cross.sh`: aarch64 Debug and Release builds of the G0 probe, `qnx_platform` and `rnqnx_playground`, with an ELF check. |
| [`qnx-vm-validation.yml`](../../.github/workflows/qnx-vm-validation.yml) | `self-hosted, macOS, ARM64, rnqnx-qnx-vm` | `scripts/validate-qnx-vm.sh`: attach or boot the VM, run the component captures and the JS-thread test.                                          |
| [`v1-candidate.yml`](../../.github/workflows/v1-candidate.yml)           | hosted, plus the two lanes above          | The 19 hosted required workflows as reusable workflows, both QNX lanes, and one aggregate job.                                                   |

## Runner labels

A job that names a label no runner carries waits until GitHub gives up on it.
The two QNX jobs avoid that by checking a repository variable first.

| Item                      | Value                                                                                                                      |
| ------------------------- | -------------------------------------------------------------------------------------------------------------------------- |
| Repository variable       | `RNQNX_SELF_HOSTED` set to `true`. Any other value, or no value, skips both QNX jobs.                                      |
| Cross-build runner labels | `self-hosted`, `Linux`, `X64`, `rnqnx-qnx-sdp`. The runner needs the licensed QNX SDP 8.0 (ADR-0007), CMake and Ninja.     |
| VM runner labels          | `self-hosted`, `macOS`, `ARM64`, `rnqnx-qnx-vm`. The runner needs `qemu-system-aarch64`, the VM directory and its ssh key. |
| Optional variable         | `RNQNX_QNX_SDP_ENV`: path of `qnxsdp-env.sh` on the cross-build runner. Default `/work/qnx800/qnxsdp-env.sh`.              |

`.github/actionlint.yaml` lists the two custom labels so `actionlint` accepts them.

Register the runners from Settings, Actions, Runners, New self-hosted runner,
then add the custom labels with `--labels rnqnx-qnx-sdp` or `--labels rnqnx-qnx-vm`
when running `config.sh`. The SDP and its license stay on the runner. The
workflows copy only evidence files (versions, ELF headers, hashes) into the
artifact. Do not put SDP files in it.

State on 2026-11-21: `gh api repos/mikevocalz/react-native-qnx/actions/runners`
returned `total_count: 0`. No self-hosted runner is registered, so neither QNX
workflow has run on GitHub. See [the S5 receipt](../evidence/s5-vm-lane-2026-11-21/README.md)
for what was run by hand instead.

## Run the VM validation by hand

```sh
scripts/validate-qnx-vm.sh --out build/qnx-vm-validation/manual
```

The script attaches to the VM if ssh answers on `127.0.0.1:2222` and boots it
with `$RNQNX_VM_DIR/v1/run.sh -daemonize -pidfile /private/tmp/qnxvm.pid` if
not. It exits 0 when every step passes, 1 when a step fails, 2 for bad usage
and 3 when the VM cannot be reached or booted.

| Option                                | Effect                                                                                                             |
| ------------------------------------- | ------------------------------------------------------------------------------------------------------------------ |
| `--out DIR`                           | Output directory. Default `build/qnx-vm-validation/<run id>`.                                                      |
| `--boot auto\|never\|always`          | `auto` attaches or boots. `never` fails when the VM is down. `always` stops a running VM first, then boots.        |
| `--stage DIR`                         | Push `DIR/app`, `DIR/js-thread` and `DIR/bundles` to the guest, compare every file hash, and use them for the run. |
| `--expect-sha256 FILE`                | Lines `<sha256>  <guest path>`. Fails when a guest file hashes differently.                                        |
| `--keep-running`                      | Leave the VM up even when this run booted it.                                                                      |
| `--shutdown`                          | Stop the VM at the end even when it was already up.                                                                |
| `--skip-captures`, `--skip-js-thread` | Leave a step out.                                                                                                  |

By default the script stops a VM that it booted itself and leaves an attached
VM running. Defaults for every path and port are in the header of the script
and are overridable with `RNQNX_VM_*` variables, listed in
[environment variables](../../docs-site/content/docs/reference/environment-variables.mdx).

### Steps

| Step        | Pass condition                                                                                                                                                                                                                     |
| ----------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `attach`    | ssh answers, after a boot if needed.                                                                                                                                                                                               |
| `identity`  | `uname -a` contains `QNX` and `QEMU_virt`. Anything else (a Pi, for example) stops the run with exit 3.                                                                                                                            |
| `deploy`    | `scripts/vm-proof/capture.sh` and `js-thread.sh` copied to the guest. The app, the JS-thread test binary and the bundles exist and are hashed with `openssl` on the guest. With `--stage` or `--expect-sha256`, hashes also match. |
| `captures`  | Per card: `capture.sh` reports exit 0, the PNG is a real PNG of 1280x800 and at least 8000 bytes, the app log has the `RNQNX_PLAYGROUND_SCREENSHOT` marker and no error text, and no two cards produce the same PNG.               |
| `js-thread` | The test binary exits 0, the log has no `[FAIL]` line, at least 11 `[PASS]` lines and `All N JS-thread tests passed.`                                                                                                              |
| `shutdown`  | Only when the run stops the VM.                                                                                                                                                                                                    |

The captures and the JS-thread test are the ones from
[qnx-vm-2026-11-21](../evidence/qnx-vm-2026-11-21/README.md) and
[s2-vm-2026-11-21](../evidence/s2-vm-2026-11-21/README.md). The script copies
`scripts/vm-proof/*.sh` to the guest and does not duplicate their logic.

### Output

```text
result.json          machine-readable result, schema rnqnx.qnx-vm-validation/1
SHA256SUMS           sha256 of every other file in the directory
guest-uname.txt      uname -a from the guest
guest-sha256.txt     sha256 of the files the run used, hashed on the guest
captures/            one PNG and one .log.txt per card, plus capture-run.txt
js-thread/           run.txt (the test log), stdout.txt, shots/
```

`result.json` carries `label`, `notEvidenceFor`, `status`, `exitCode`, the repo
commit and whether the tree was dirty, the guest `uname`, one entry per step
with its seconds, per-card PNG size and hash, the JS-thread pass count and its
`[RESULT]` lines, and the guest hashes. Verify a saved directory with
`cd <dir> && shasum -a 256 -c SHA256SUMS`.

Step seconds are TCG wall time. They show that a step finished; they say
nothing about Pi performance.

## Dispatch the candidate

```sh
gh workflow run v1-candidate.yml --ref <candidate-branch>
```

The workflow file must exist on the default branch before GitHub accepts a
dispatch. Inputs:

| Input               | Default | Meaning                                                                          |
| ------------------- | ------- | -------------------------------------------------------------------------------- |
| `run_hosted`        | `true`  | Call the 19 hosted workflows. `false` runs only the QNX lanes and the aggregate. |
| `require_qnx_lanes` | `true`  | Fail the aggregate when a self-hosted QNX lane was skipped.                      |

The aggregate job lists every lane with its result in the run summary. It fails
when any lane failed or was cancelled, when a requested hosted lane was skipped,
and, with `require_qnx_lanes`, when a QNX lane was skipped. With
`require_qnx_lanes: false` and no runner it passes with a warning that the
status does not cover QNX. Every called workflow runs on the commit of the
dispatched ref, which is what [required-checks.md](../releases/required-checks.md)
asks for.

Each hosted workflow gained `workflow_call:` next to its `workflow_dispatch:`
trigger. Their own triggers and path filters are unchanged.

## Unverified until a runner exists

- `qnx-cross-build.yml` and `qnx-vm-validation.yml` have not run on GitHub.
  `actionlint` accepts them. The commands they call ran by hand, on the
  maintainer's machine, as listed in the receipt.
- The cross-build lane covers the G0 probe, `qnx_platform` and
  `rnqnx_playground`. It does not build Qt or the showcase app. The VM lane runs
  the showcase and JS-thread binaries already installed on the guest unless
  `--stage` pushes others, so the two lanes are not yet chained by artifact.
- The `ci` gate needs the full required set on one candidate commit. This
  change does not claim it.
