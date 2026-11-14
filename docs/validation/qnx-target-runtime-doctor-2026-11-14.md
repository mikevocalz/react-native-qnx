# QNX offline runtime diagnostics, 2026-11-14

This slice addresses F5 and H27 of the v1 hardening request under the `distribution`
and `device-parity` gates. It adds a separate CLI mode for supplied QNX runtime
observations. It leaves the existing static project doctor and the Linux/Wayland
workflow in place. Both gates remain pending: no target collection or device
receipt was produced.

Base: `f87c742`. Lock SHA-256:
`5d8a369d8bc0102bbb6984567646041596994ab5f816749732244178b8d9a6ff`.
Node `26.13.0`, TypeScript `7.0.2`. Validation used the existing installed graph
through a temporary dependency symlink; no fresh frozen installation is claimed.
`validate:dependencies` confirmed the inspected pins and source contracts.

## Behavior and ownership

`project doctor --target-runtime-report <file>` validates one regular JSON file
of at most 64 KiB and checks reported Screen, logging, temporary directory,
random device, IPv6, runtime paths, fonts, display configuration and physical
size. `--verbose` suggests first-start debug variables without setting them.
Unset environment variables are allowed when resolved Qt/application paths and
default display configuration are reported. Positive Screen dimensions need no
fallback; zero dimensions require a valid millimetre fallback.

The new helpers return discriminated results with a named failed step. They
never run package/config/report code, connect to a device or inspect report
paths on the host. File loading rejects symlinks and nonregular files, uses a
bounded read and rejects unknown fields, invalid dates, control characters,
relative paths and duplicate path entries. The supplied observations remain
untrusted and may be stale or forged. JSON output explicitly reports
`observationOrigin: "user-supplied"`, `collectionVerified: false`,
`evidenceTier: "offline-observation"` and `runtimeVerified: false`.

Path ownership: Metro / Expo developer tools engineer for
`packages/react-native-qnx-cli/src/doctor.mjs` and
`target-runtime-doctor.mjs`; Documentation / developer-experience engineer for
the runbook; QNX regression owner for the fixtures. Independent reviewer `review_doctor`, acting as Security / QA and platform reviewer, inspected the code and receipt and found no blocking issue. The reviewer independently ran the new, static and environment doctor fixtures. No external author
or vendor is assigned as a collaborator.

## Source and skills

Inspected `doctor.mjs`, its argument routing in `bin.mjs`, existing static and
environment doctor tests, the CLI guide, the roster and Plans03/14/15. Qt's
[QNX runtime requirements](https://doc.qt.io/qt-6/qnx-target-requirements.html)
and [QNX support](https://doc.qt.io/qt-6/qnx-support.html) were read as Qt 6.12 on
2026-11-14. The checker distinguishes reported paths from environment settings;
Qt can use installation defaults and application configuration. It does not
claim to resolve target libraries or validate their ABI from a path.

| Skill                     | Source/version                                                                                                 | Decision applied                                                                                                              |
| ------------------------- | -------------------------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------- |
| `testing-strategy`        | engineering plugin 1.2.0, SKILL.md SHA-256 `5c5e95830754bbdd838213fa05fc8f07523f591fd558fd3c86031ffd479f7a9e`  | Test missing observations, malformed input, defaults and file/CLI boundaries with host fixtures; retain device gap.           |
| `documentation`           | engineering plugin 1.2.0, SKILL.md SHA-256 `1d469418f786a05be83d2a05f04d68788aeed584d13863a650f5ad73c6c4cf50`  | Document collection prerequisites, format, exit codes and limits separately from the static doctor.                           |
| `qnx-platform-validation` | repository base `f87c742`, SKILL.md SHA-256 `92012064c52eea4e2d68f92b12d54f6d3952c966e34f594b81be5ce99457c993` | Label every result as supplied observation or host fixture; promote no device/release gate.                                   |
| `code-review`             | local skill 0.1.0, CodeRabbit CLI 0.7.6                                                                        | Read; service review blocked because authentication reports signed out. Independent agent review completed as recorded above. |

No C++, native API, Nitro contract, QML, dependency, workflow or manifest changed.
The Margelo native implementation sequence is not invoked for this tooling-only
slice.

## Validation and remaining evidence

Raw outputs are in [the log directory](logs/qnx-target-doctor-2026-11-14/).
The typecheck emits no output on success.

| Command                                            | Result / scope                                                                              |
| -------------------------------------------------- | ------------------------------------------------------------------------------------------- |
| `node tests/js/qnx-target-runtime-doctor.test.mjs` | Passed 47 observation scenarios plus CLI and file boundaries, synthetic host fixtures only. |
| `node tests/js/qnx-consumer-doctor.test.mjs`       | Passed 55 existing static project-doctor checks, including Marathon metadata fixtures.      |
| `node tests/js/qnx-environment-doctor.test.mjs`    | Passed 14 existing environment prerequisite checks.                                         |
| `node node_modules/typescript/bin/tsc --noEmit`    | Passed root tooling typecheck.                                                              |
| `node scripts/validate-dependencies.mjs`           | Passed pinned dependency/source checks using the cached installation.                       |
| `node scripts/validate-docs.mjs`                   | Passed 24 component and 11 gate documentation validation.                                   |

No hosted CI, native build, QNX cross-build, QEMU, Pi 400, stock QSTI, cold boot,
frame capture, startup or RSS measurement ran. The parser's 64 KiB limit is an
input bound, not a measured performance budget. No performance claim is made.
F5 remains partial until a collector and actual target observations are
qualified; F6's exact SDP/library compatibility remains separate.

Security/licensing impact: the checker reads no SDK payload and adds no
permissions, remote execution or redistribution. Only selected environment
variable names appear in the runbook; it does not request a whole environment
dump. No boot, window dimensions, orientation, graphics or scheduling policy
changed. Rollback removes this CLI option, helper, fixtures and documentation;
it requires no device rollback.
