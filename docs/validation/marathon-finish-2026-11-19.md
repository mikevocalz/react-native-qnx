# Marathon documentation finish: evidence ledger, 2026-11-19

This ledger records what was checked while the Marathon documents were brought up to date, and where each statement comes from. It makes no claim about a MarathonOS device. Base commit of the clone: `8d7deaa3c61ed58ff35dbc5887b78c455adcb205` (main).

## Pull request states (`gh pr view`)

| PR                                                              | State              | Merged (UTC)              | Subject                                                                                                             |
| --------------------------------------------------------------- | ------------------ | ------------------------- | ------------------------------------------------------------------------------------------------------------------- |
| [#33](https://github.com/mikevocalz/react-native-qnx/pull/33)   | merged             | 2026-11-02 20:42          | Marathon architecture, roster, skills, upstream RFC docs                                                            |
| [#34](https://github.com/mikevocalz/react-native-qnx/pull/34)   | merged             | 2026-11-02 20:43          | `marathon` Metro platform                                                                                           |
| [#35](https://github.com/mikevocalz/react-native-qnx/pull/35)   | merged             | 2026-11-03 01:39          | Opt-in Linux Qt Wayland host                                                                                        |
| [#38](https://github.com/mikevocalz/react-native-qnx/pull/38)   | merged             | 2026-11-02 20:45          | QNX native module on Nitro 1.0 alpha                                                                                |
| [#41](https://github.com/mikevocalz/react-native-qnx/pull/41)   | merged             | 2026-11-02 20:48          | Separate QNX and Marathon Nitro v1 adapters                                                                         |
| [#93](https://github.com/mikevocalz/react-native-qnx/pull/93)   | closed, not merged | 2026-11-08 04:24 (closed) | Development launcher. Closed as redundant: commit `e30bfdfa64aa4db39c043c0c88dbcfc8f3e75d50` is in main through #96 |
| [#100](https://github.com/mikevocalz/react-native-qnx/pull/100) | merged             | 2026-11-08 04:21          | `qnx create` with optional Marathon target                                                                          |
| [#138](https://github.com/mikevocalz/react-native-qnx/pull/138) | merged             | 2026-11-12 18:27          | `qnx`/`marathon` package export conditions in Metro                                                                 |
| [#149](https://github.com/mikevocalz/react-native-qnx/pull/149) | merged             | 2026-11-19 09:51          | Worklets, Reanimated, Gesture Handler; head `cbe27bf7` carries the CI runs below                                    |

## CI runs (all success, PR #149 head `cbe27bf7`)

| Workflow               | Run                                                                                    | Job result                                                                                                                                           |
| ---------------------- | -------------------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------- |
| `marathon-qt-host.yml` | [44492351468](https://github.com/mikevocalz/react-native-qnx/actions/runs/44492351468) | `native-qt-host` success: frozen install, `pnpm test:js`, `pnpm typecheck`, Qt 6.11.1 install, host build, offscreen screenshot, asset-rebuild check |
| `marathon-metro.yml`   | [44492351488](https://github.com/mikevocalz/react-native-qnx/actions/runs/44492351488) | `metro` success: tests, `validate:dependencies`, QNX and Marathon production bundles                                                                 |
| `nitro-v1-host.yml`    | [44492351495](https://github.com/mikevocalz/react-native-qnx/actions/runs/44492351495) | `source-and-compile` success: Nitro/JSI source checks, QNX and Marathon adapters compile                                                             |

None of the three workflows has a run on `main` itself, because each is path-filtered and main's latest commits touch only docs. Earlier success: [41519519981](https://github.com/mikevocalz/react-native-qnx/actions/runs/41519519981) (PR #35 head `206b0e51`, 2026-11-02). One failure exists in the same window (head `7c3a3c75` on `feat/expo-image-painted-view`, 2026-11-19 05:53); the next push, `0f3fb181`, passed all three.

## QNX device evidence that exists

Physical Pi 400 (custom QNX 8.0.0 image, cross-built with SDP 8.0.5): [pi400-host-validation-2026-11-07](../evidence/pi400-host-validation-2026-11-07.md), [pi400-nitro-qt-2026-11-07](../evidence/pi400-nitro-qt-2026-11-07.md), [pi400-public-qt-o2-2026-11-12-a79e16a](../evidence/pi400-public-qt-o2-2026-11-12-a79e16a.md), [pr149-pi400-run1-2026-11-19](../evidence/pr149-pi400-run1-2026-11-19/README.md), plus run 2 and a key-hold run in the same directory family. Not run: stock Pi 4 QSTI board and QNX VM ([pi400-public-qt-2026-11-12-b51b49c](../evidence/pi400-public-qt-2026-11-12-b51b49c.md), line "Pi 400 only; no stock Pi 4 QSTI, QNX VM or Marathon run"). [qnx-target-runtime-doctor-2026-11-14](qnx-target-runtime-doctor-2026-11-14.md) is an offline diagnostic and states that no target collection was produced. None of this is Marathon evidence. No receipt anywhere in `docs/evidence` or `docs/validation` records a Wayland-compositor or MarathonOS run of `rnqnx_marathon_host`.

## Upstream state (read-only `gh` queries)

- Upstream `patrickjquinn/Marathon-Shell` `main`: `1f845160f0c249cb29da127fb66abaad380b5438`, committed 2026-11-19T18:05:30Z. The RFC base `c2c1b2fe32aeaa51edf75a45045cbfbbc08c4ac5` (2026-10-30) is 1023 commits behind it.
- `gh pr list -R patrickjquinn/Marathon-Shell --author mikevocalz --state all`: empty. `gh search issues` for "react native", "native" and "mikevocalz": nothing about this work.
- Fork PR [mikevocalz/Marathon-Shell#1](https://github.com/mikevocalz/Marathon-Shell/pull/1): open, draft, mergeable, head `209485b7d23c4468ab67c45348af5c3e7ef7df12`, base `c2c1b2fe`, no comments, no reviews, updated 2026-11-09, checks `build-and-test`, `format-check`, `commit-subjects` all success. The RFC commit is `d16e6db81f5bafde099c5452995f8f46bd05a51c`. The fork has 21 open PRs, none of them a second managed-client RFC.
- Docs read at `1f845160`: `docs/APP_MANIFEST_SCHEMA.md` (last change `863a6c6c`, 2026-11-15), `docs/NATIVE_APP_LIFECYCLE.md` (last change `9cc1e334`, 2026-10-23), `docs/SECURITY_MODEL.md` (last change `27f7e1f3`, 2026-11-05), and `schemas/marathon-app-manifest-v1.schema.json`. The `.qml`-only `entryPoint` claim holds: the schema requires `entryPoint` matching `\.qml$`, allows `toolkit` of `keel` or `marathonui`, and sets `additionalProperties: false`.
- Third-party citation pin `marathon-shell-567f1fa` is `patrickjquinn/Marathon-Shell@567f1faa8bfdf31611dd401134c293daa160221c` (2026-11-09). It resolves. It appears in `packages/react-native-qnx-cli/src/app-identity.mjs` and in evidence logs, not in the documents edited here.

## Stale statements found and what changed

| Document                                                | Stale statement                                                                             | Change                                                                                             |
| ------------------------------------------------------- | ------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------- |
| `docs/marathon/ARCHITECTURE.md`                         | Status "design approved ... Last reviewed 2026-11-02"                                       | Status rewritten with verified and unverified parts, date 2026-11-19                               |
| same                                                    | "`native/platform/linux-host`: existing directory, currently only scaffold"                 | Lists the Nitro adapter that lives there and names what is not implemented                         |
| same                                                    | "QNX SDP 8.0.5 Pi/QSTI run pending"                                                         | Pi 400 receipts listed; QSTI and VM stated as not run                                              |
| same                                                    | Nitro "stable npm latest remains 0.37.1"                                                    | npm `latest` is `0.37.2`; alpha pin `1.0.0-alpha.nightly.136.gdb81de61bd32`                        |
| same                                                    | Milestone table without status                                                              | Status column added; "Verified in CI" and "Open items needing owner or upstream" sections added    |
| `docs/marathon/METRO.md`                                | "JS build configuration added", "RN 0.88 rc baseline is inherited", evidence list as future | Merged state, `react-native` 0.88.1 / `expo` 58.0.24, CI run URL, open items list                  |
| `docs/marathon/NATIVE_HOST.md`                          | "First merge the Marathon Metro platform PR"                                                | PR #34 merged 2026-11-02; prerequisite removed                                                     |
| same                                                    | "Build and run, commands to be validated in CI/device"                                      | Commands match `marathon-qt-host.yml`; offscreen run marked verified, Wayland run marked not run   |
| same                                                    | Validation gates "target work pending"                                                      | Per-gate status table with run URLs                                                                |
| `docs/marathon/UPSTREAM_RFC.md`                         | "not yet submitted upstream"                                                                | Exact fork PR #1 and upstream state, doc SHAs, owner action                                        |
| `docs/marathon/ROSTER.md`                               | Status "proposed"                                                                           | Dated, with progress on the dependency order                                                       |
| `docs/marathon/SKILLS.md`                               | Proof table all TODO; wrong `Argent/agent-device`; "no react-native-testing or dogfood"     | Table filled from the pinned SHAs; corrections section                                             |
| `docs/adr/0015-...`                                     | Status "proposed"                                                                           | Accepted for the Qt/Fabric reuse and Marathon host; delivery route stays proposed pending upstream |
| `docs/expo-delivery/phase-05-marathon.md`               | "proposed implementation plan; documentation only"                                          | Status per step, upstream facts, open items                                                        |
| `docs/getting-started/marathon-development-launcher.md` | Launcher test and PR status unspecified                                                     | PR #93/#96 history and the missing compositor run stated                                           |

## Open items needing owner or upstream

The full list is in [ARCHITECTURE](../marathon/ARCHITECTURE.md#open-items-needing-owner-or-upstream). In short: a Wayland compositor run, a Marathon VM or device run, a device-toolchain build, an upstream decision on native-executable delivery, and lifecycle, back and IME behavior.
