# Required checks for a v1 release candidate

The [Candidate CI gate](v1-readiness.md) passes only when every required
check in this file succeeds on the exact candidate commit, on runners the
owner has authorized. Results fall into three categories, and each is reported
on its own:

- **Hosted**: GitHub-hosted `ubuntu-24.04` runners. Linux host builds,
  source contracts and bundles. None of them compile for QNX.
- **QNX cross-build**: a QNX SDP toolchain build for the target, on a
  self-hosted runner. `qnx-cross-build.yml` does this; it has not run on GitHub
  because no runner is registered.
- **Device**: supervised runs on named QNX hardware. They are recorded as
  evidence receipts under the other v1 gates, not as CI checks.

A local run of the same command never substitutes for a hosted check. A
passing hosted check does not show anything about QNX cross-builds or devices.

## Run the hosted checks on a candidate

Most required workflows filter on paths, so a pull request that touches only
some files does not start them. Dispatch the candidate workflow, which calls
every hosted workflow below on the ref, both QNX lanes, and one aggregate job
(see [QNX cross-build and VM validation lanes](../ci/qnx-vm-lane.md)):

```sh
gh workflow run v1-candidate.yml --ref <candidate-branch>
```

Every run must belong to the candidate's head commit. A run on an earlier
commit does not count, even when only documentation changed after it.

## Hosted checks

The last column records dispatch runs on branch `v1/gate2-ci` at
`0e8e8939a9f257e2624f76fc9eaeb83bac6ede46` (2026-11-12 UTC). That commit is
not a release candidate; the column shows whether each check starts and what
it currently reports.

| Workflow                            | Check names                                                    | Why it is required                                                                                                                                                                                                                                                                             | Result at `0e8e893`                                                                                                    |
| ----------------------------------- | -------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------- |
| `sdk58-validation.yml`              | `validate`                                                     | Frozen install, format, dependency and security-override contracts, QNX bundles, clean checkout and dependency audit.                                                                                                                                                                          | fail: node-forge 1.4.2, fixed by #117 ([run](https://github.com/mikevocalz/react-native-qnx/actions/runs/43145827324)) |
| `host-renderer.yml`                 | `Qt 6.11 / RN 0.88 / Hermes (input lifecycle ON)`, `(... OFF)` | Builds and runs the full Fabric/Hermes/Qt Quick host graph and the conformance and clean-app renders.                                                                                                                                                                                          | pass ([run](https://github.com/mikevocalz/react-native-qnx/actions/runs/43145830131))                                  |
| `image-loader.yml`                  | `image-cache`                                                  | Real local HTTP fetch and decoded-cache tests for the image loader.                                                                                                                                                                                                                            | pass ([run](https://github.com/mikevocalz/react-native-qnx/actions/runs/43145833073))                                  |
| `nitro-v1-host.yml`                 | `source-and-compile`                                           | Compiles the pinned Nitro v1 core and both platform adapters.                                                                                                                                                                                                                                  | pass ([run](https://github.com/mikevocalz/react-native-qnx/actions/runs/43145836157))                                  |
| `nitro-qnx-source-check.yml`        | `source-contract`                                              | QNX Nitro runtime source contract against the installed graph.                                                                                                                                                                                                                                 | pass ([run](https://github.com/mikevocalz/react-native-qnx/actions/runs/43145839290))                                  |
| `expo-modules-qnx-source-check.yml` | `source-contract`                                              | Expo Modules QNX source contract.                                                                                                                                                                                                                                                              | pass ([run](https://github.com/mikevocalz/react-native-qnx/actions/runs/43145842426))                                  |
| `expo-qnx-autolinking.yml`          | `autolinking`                                                  | Expo autolinking and config-plugin behavior for QNX packages.                                                                                                                                                                                                                                  | pass ([run](https://github.com/mikevocalz/react-native-qnx/actions/runs/43145845069))                                  |
| `hermes-qnx-source-check.yml`       | `patch`                                                        | Applies the QNX Hermes transforms to the exact audited Hermes source.                                                                                                                                                                                                                          | pass ([run](https://github.com/mikevocalz/react-native-qnx/actions/runs/43145848024))                                  |
| `react-qnx-source-check.yml`        | `source-contract`                                              | Builds the React probe bundle against the pinned renderer sources.                                                                                                                                                                                                                             | pass ([run](https://github.com/mikevocalz/react-native-qnx/actions/runs/43145851065))                                  |
| `pi-tooling-quality.yml`            | `Shell syntax and lint`                                        | Bash syntax and ShellCheck for the scripts used to build and deploy to the board.                                                                                                                                                                                                              | pass ([run](https://github.com/mikevocalz/react-native-qnx/actions/runs/43145854066))                                  |
| `runtime-appstate.yml`              | `native-runtime (ON)`, `native-runtime (OFF)`                  | Native AppState through Qt, Hermes and React Native, with the feature on and off.                                                                                                                                                                                                              | pass ([run](https://github.com/mikevocalz/react-native-qnx/actions/runs/43145857028))                                  |
| `qnx-create-app.yml`                | `create`                                                       | The source CLI creates an external Expo app and bundles it for both JS targets.                                                                                                                                                                                                                | fail: node-forge 1.4.2, fixed by #117 ([run](https://github.com/mikevocalz/react-native-qnx/actions/runs/43145860136)) |
| `qnx-native-consumer-build.yml`     | `native-consumer`                                              | Builds and runs the native consumers of a created app.                                                                                                                                                                                                                                         | pass ([run](https://github.com/mikevocalz/react-native-qnx/actions/runs/43145862987))                                  |
| `phase6-doctor.yml`                 | `diagnostics`                                                  | The consumer `doctor` command on the full installed graph.                                                                                                                                                                                                                                     | fail: node-forge 1.4.2, fixed by #117 ([run](https://github.com/mikevocalz/react-native-qnx/actions/runs/43145866151)) |
| `phase7-release-dossier.yml`        | `dossier`                                                      | Freezes the candidate dossier and checks that it grants no release approval.                                                                                                                                                                                                                   | fail: node-forge 1.4.2, fixed by #117 ([run](https://github.com/mikevocalz/react-native-qnx/actions/runs/43145869091)) |
| `phase-public-host.yml`             | `render`                                                       | Renders the public React Native import bundles on the host.                                                                                                                                                                                                                                    | pass ([run](https://github.com/mikevocalz/react-native-qnx/actions/runs/43145872208))                                  |
| `app-identity.yml`                  | `identity (ON)`, `identity (OFF)`                              | Native application identity contract, with and without sanitizers.                                                                                                                                                                                                                             | pass ([run](https://github.com/mikevocalz/react-native-qnx/actions/runs/43145875252))                                  |
| `phase4-lifecycle.yml`              | `observer (ON)`, `observer (OFF)`                              | Native Qt lifecycle observer, with and without sanitizers.                                                                                                                                                                                                                                     | pass ([run](https://github.com/mikevocalz/react-native-qnx/actions/runs/43145879620))                                  |
| `performance-budgets.yml`           | `host-budgets`                                                 | Host lane for the `performance` gate: builds the Release playground host with the timing probe, runs Hello World and the 1000-row list 20 times each and fails when a median passes its ceiling in [performance-budgets.json](performance-budgets.json). Host only; it says nothing about QNX. | not run at `0e8e893` (added by S4)                                                                                     |

The four node-forge failures at `0e8e893` came from one step,
`pnpm audit:dependencies`, which rejected node-forge 1.4.2 for
GHSA-w4xr-hqv8-ch44, GHSA-6cv4-p8m7-4r25 and GHSA-j8rw-jp99-435v (all
moderate). #117 (merged as `edefe6d`) pins node-forge 1.4.3 in
`pnpm-workspace.yaml`, and the audit step passes on later commits:
`validate` ([run](https://github.com/mikevocalz/react-native-qnx/actions/runs/43239786456)),
`create` ([run](https://github.com/mikevocalz/react-native-qnx/actions/runs/43239483970)),
`diagnostics` ([run](https://github.com/mikevocalz/react-native-qnx/actions/runs/43239484367))
and `dossier` ([run](https://github.com/mikevocalz/react-native-qnx/actions/runs/43200800044)).
Rerun all four on the candidate as usual.

## QNX cross-build checks

Required. `qnx-cross-build.yml` runs `scripts/validate-qnx-805-cross.sh`
(aarch64 Debug and Release builds of the G0 probe, `qnx_platform` and
`rnqnx_playground`, plus an ELF check) on a self-hosted runner that holds the
licensed SDP, labels `self-hosted, Linux, X64, rnqnx-qnx-sdp`. It runs only when
the repository variable `RNQNX_SELF_HOSTED` is `true`. No runner is registered
today, so the workflow has not run on GitHub and the check is unmet. The gate
cannot pass until it runs green on the candidate commit. Hosted results do not
stand in for it. It does not build Qt or the showcase app.

`qnx-vm-validation.yml` is a second self-hosted lane (`self-hosted, macOS,
ARM64, rnqnx-qnx-vm`). It labels its results "QNX VM (QEMU, TCG, software
render)". It is not device evidence and does not replace the device gates.

(`websocket-host.yml` used to confirm that the build refused WebSockets on QNX;
since the QNX cross-build of Qt WebSockets in the
[dev workflow](../getting-started/dev-workflow.md) it checks that the
development client cannot be configured without the WebSocket backend.)

## Device results

Device results are not CI checks. Cold boot, component and input behavior,
and performance each have their own gate in the
[readiness record](v1-readiness.md), with acceptance evidence named there. The
historical Pi 400 receipts in the [current handoff](../handoff/CURRENT_STATE.md)
belong to earlier revisions and do not cover a candidate.

## Workflows that are not required

| Workflow                | Reason                                                                                                   |
| ----------------------- | -------------------------------------------------------------------------------------------------------- |
| `forge-native-host.yml` | Builds against the separately licensed Forge engine. Core CI must not depend on premium components.      |
| `marathon-metro.yml`    | MarathonOS is a separate Linux/Wayland target, not part of the QNX v1 milestone. Informational.          |
| `marathon-qt-host.yml`  | Marathon native host proof. Informational for the same reason.                                           |
| `phase5-tooling.yml`    | Marathon development launcher. Informational for the same reason.                                        |
| `websocket-host.yml`    | Experimental WebSocket backend that is OFF by default. Informational until it becomes a default feature. |

## Workflow state on `main` before this change

Checked through the GitHub API on 2026-11-11 at `b51b49c`. `main` has no branch
protection and no rulesets, so no check is enforced today.

| Workflow                                                                                      | Last `main` run                                                                                                                |
| --------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------ |
| `sdk58-validation.yml`                                                                        | fail at `b51b49c`, node-forge audit ([run](https://github.com/mikevocalz/react-native-qnx/actions/runs/42929039266))           |
| `host-renderer.yml`, `image-loader.yml`, `pi-tooling-quality.yml`                             | pass at `b51b49c`                                                                                                              |
| `nitro-qnx-source-check.yml`, `expo-modules-qnx-source-check.yml`, `expo-qnx-autolinking.yml` | pass at `f2dc251` (2026-11-08)                                                                                                 |
| `react-qnx-source-check.yml`, `websocket-host.yml`                                            | pass at `f2e2546` (2026-11-08)                                                                                                 |
| `hermes-qnx-source-check.yml`                                                                 | refused at `62d530b` (2026-11-07) for billing ([run](https://github.com/mikevocalz/react-native-qnx/actions/runs/42373917630)) |
| The other 13 workflows                                                                        | never ran on `main`; they trigger on pull requests, dispatch or non-`main` pushes only                                         |

The refused Hermes run carries this annotation: "The job was not started
because recent account payments have failed or your spending limit needs to
be increased. Please check the 'Billing & plans' section in your settings".
Every dispatch run on `0e8e893` started on a hosted runner, so hosted jobs are
no longer refused.

## Pinning and permissions

Each required workflow sets `permissions: contents: read` at the top level and
grants nothing more to any job. Each action reference is a full commit SHA,
resolved through the GitHub API on 2026-11-11 from the tag in use:

| Action                      | Tag in use | Commit                                     |
| --------------------------- | ---------- | ------------------------------------------ |
| `actions/checkout`          | `v5.1.0`   | `fbc6f3992d24b796d5a048ff273f7fcc4a7b6c09` |
| `actions/setup-node`        | `v5.0.0`   | `a0853c24544627f65ddf259abe73b1d18a591444` |
| `actions/upload-artifact`   | `v4.6.2`   | `ea165f8d65b6e75b540449e92b4886f43607fa02` |
| `jurplel/install-qt-action` | `v4.5.0`   | `b344308e4483fc892e0d27b3cd1b173ebdd95f06` |

Node in `setup-node` is 26.13.1, the Node 26 LTS release of 2026-11-10.
Workflows that are not required keep their earlier references.

## Branch ruleset on `main`

Ruleset `main` (id 27120426, active since 2026-11-12) applies to the default
branch. It blocks deletion and force-pushes, requires a pull request, and
requires these checks, which run on every pull request to `main`:

- `validate` from `SDK 58 validation`
- `Qt 6.11 / RN 0.88 / Hermes (input lifecycle ON)` and `(... OFF)` from
  `SDK 58 full host renderer`
- `image-cache` from `Image loader HTTP and cache`

Path-filtered workflows are not required there, because a pull request that
skips them would wait forever. Run them by dispatch on the candidate. The
`Expo QNX autolinking` job is named `autolinking` so that `validate` names one
check only. Repository admins can bypass the ruleset only through a pull
request. No approving review is required while the repository has one
maintainer.

## Owner actions

- Authorize a self-hosted runner with the licensed QNX SDP, give it the labels
  above, and set the repository variable `RNQNX_SELF_HOSTED` to `true`. The
  workflows exist; they have not run.
- After the optional workflows are pinned too, turn on the Actions setting
  that requires SHA-pinned actions (`sha_pinning_required` is `false` today).
