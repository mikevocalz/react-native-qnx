# QNX v1 hardening coverage

All requested work remains required. This record tracks the November 14, 2026
hardening request against source baseline `f87c742`; it does not change any
status in [v1-readiness.json](../releases/v1-readiness.json). Each row below is
pending until its implementation, independent review and acceptance evidence
are linked. PR URLs are pending unless a row names one.

The first distribution slice implements transactional public-runtime staging in
`scripts/qt-public/stage-runtime.py`, with synthetic host fixtures in
`tests/python/test_qt_public_staging.py` and
`tests/js/qnx-public-runtime-staging.test.mjs`. It preserves the previous staged
runtime when input validation or closure verification fails. Its implementation is [PR #163](https://github.com/mikevocalz/react-native-qnx/pull/163). Host fixture evidence applies to the staging tool only; the
`distribution` gate remains pending.

No QNX cross-build, QEMU run, Pi 400 run or stock Pi 4 QSTI run was performed for
this hardening slice. The sandboxed Colima check reported unavailable; a later direct check confirmed the existing QNX build VM was running. The owner confirmed that only the Pi 400 is available; stock Pi 4 QSTI receipts remain blocked by hardware availability. Explore F observed
`CVE-2026-105943-qtbase-6.12.diff`, dated November 13 at 15:07, in the
[official Qt 6.12 download index](https://download.qt.io/official_releases/qt/6.12/).
It is absent from the current patch script. A subsequent source check identifies Windows SSPI-only hunks, and the existing QNX build header records `QT_FEATURE_sspi -1`. The patch is not applied here; the index entry alone does not establish an affected QNX candidate.
The existing public-source Qt receipts retain their original revisions and
scope. The Linux/Wayland target is outside this implementation scope.

## Audit and source record

The input was `react-native-qnx-v1-hardening-prompt.md`, SHA-256
`8557dc52b61bb44af2d1efd17330a6bfcdb762924552bbba5d03318df2a0cc52`.
Eight read-only Explore audits mapped the tree before implementation:
`explore_a`, `explore_b`, `explore_c`, `explore_d`, `explore_e`, `explore_f`,
`explore_g` and `explore_h`. Their findings identify current code and required
follow-up. They do not establish native execution or complete inspection of
every upstream reference in the request. Each later slice must finish its
section 9 source reads before editing.

| Source          | Inspected scope and revision                                                                                                                                                                             | Evidence boundary                                                                                      |
| --------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------ |
| This repository | `f87c742`; applicable instructions, readiness, handoff, ADRs, plans and code paths recorded below                                                                                                        | Existing behavior and gaps; older receipts stay revision-specific                                      |
| Legend Spark    | [project.ts at 422501e](https://github.com/LegendApp/legend-spark/blob/422501e/packages/cli/src/project.ts), read by Explore D                                                                           | Selection, dependency closure and test-only module rules; wider reference group still needs inspection |
| expo-desktop    | [AGENTS.md at fa60f41](https://github.com/shirakaba/expo-desktop/blob/fa60f41/AGENTS.md), read by Explore D; NOTES read by Explore B from the retrieved main page                                        | Result convention and eager globals; no complete pinned port audit                                     |
| Qt              | Qt 6.12 scene-graph, QML-module and disk-cache docs; current QNX support/requirements pages identifying Qt 6.12, read by Explore F                                                                       | Source guidance; versioned QNX page retrieval failed, and no device support was inferred               |
| QNX             | SDP 8.0 official scheduling, event, resource-manager and Screen references read within Explore E's reported scope                                                                                        | Installed SDP headers and target execution remain required                                             |
| QNX docker-mac  | [e2b8d0a4871f5f4284bbf9582aa081f4517eba31](https://gitlab.com/qnx/projects/docker-mac/-/tree/e2b8d0a4871f5f4284bbf9582aa081f4517eba31): README, Dockerfile and both container scripts, read by Explore H | Source evaluation; no new container build                                                              |

The coverage author read `documentation` from the engineering plugin package
`1.2.0` (SKILL.md SHA-256
`1d469418f786a05be83d2a05f04d68788aeed584d13863a650f5ad73c6c4cf50`),
`no-ai-slop` (unversioned installed file, SHA-256
`992b365f51a2f62cf4c1c5ed22049a9ee551dfea677550c1a72b371c0ceadd62`),
and the repository's `qnx-platform-validation` guidance. The documentation
skill led to links to existing plans and a separate unmet-evidence column;
`no-ai-slop` kept the status text concrete. Required design and engineering
skill files were subsequently found under the local Claude plugin cache:
knowledge-work design/engineering `1.2.0` and frontend-design `1.1.1`.
Availability does not establish that their bodies were read or their workflows
executed. Each implementation PR needs its own exact skill/version/decision
table. Native API work must read Margelo `api-design`, `build-nitro-modules`,
then `cpp`, and write TypeScript contracts and caller examples before C++ edits.

## Release gates

| Gate             | Status and current path                                                                                                                                                                        | Required next evidence / reason still pending                                                                                                                                                                                                                                 |
| ---------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `distribution`   | Pending; transactional staging implemented for host fixtures, [PR #163](https://github.com/mikevocalz/react-native-qnx/pull/163); `scripts/qt-public/`, [packaging](../packaging.md), ADR-0018 | Exact public source and security patch audit, selected module/QPA notices, hashes, library/plugin closure, corresponding-source and replacement materials; candidate archive free of SDP/image files; keep built, hashed, attested and clean-device verified records separate |
| `ci`             | Pending; [required checks](../releases/required-checks.md), `.github/workflows/`                                                                                                               | Every required check on the exact candidate; repair current integration failures at their own heads; licensed QNX runner authorization and cross-build workflow remain open                                                                                                   |
| `dependencies`   | Pending; `package.json`, `pnpm-lock.yaml`, [baseline](../releases/v1-dependency-baseline.md)                                                                                                   | Every Bump row landed or justified Hold, raw `pnpm outdated -r`, frozen graph, dependency/audit/typecheck/host/hosted results and regenerated README table                                                                                                                    |
| `cold-boot`      | Pending; [Plan01](../platform-modernization/01-runtime-baseline.md), handoff                                                                                                                   | Required supervised cold boots on the release board/image; app relaunches do not qualify; owner power-cycle procedure applies                                                                                                                                                 |
| `device-parity`  | Pending; [component support](../component-support.md), [Plan12](../platform-modernization/12-conformance-app.md)                                                                               | Every component disposition, physical keyboard/touch and selected interaction scope on stock Pi 4 QSTI; custom Pi 400 evidence remains separate                                                                                                                               |
| `performance`    | Pending; [Plan03](../platform-modernization/03-frame-pacing.md)                                                                                                                                | Candidate startup, presented-frame intervals and RSS for required workloads with raw samples, build identity, sample sizes and capture limits                                                                                                                                 |
| `app-identity`   | Pending; `native/app-identity/`, `app-identity.mjs`                                                                                                                                            | Generated icon locations and measured label fit on the target, identical tile sizes, required script/glyph cases and candidate receipts                                                                                                                                       |
| `window-chrome`  | Pending; `native/window-chrome/`, ADR-0016                                                                                                                                                     | Windows API, inset propagation, close guards, both layouts, minimize/restore AppState and chrome operation while JS is blocked on the target                                                                                                                                  |
| `expo`           | Pending; `packages/expo-*`, CLI runner/project tooling                                                                                                                                         | A-C and relevant D/H acceptance, downloaded-runner clean-host device workflow, external module linking and Router target proof                                                                                                                                                |
| `developer-path` | Pending; getting-started pages, H14/H21                                                                                                                                                        | Versioned template create/build/deploy/run by an independent first-time licensed developer; reconcile Hello World prohibition before benchmark device use                                                                                                                     |
| `documentation`  | Pending; `docs/v1/`                                                                                                                                                                            | Reviewed versioned publication after candidate behavior and other gates are established; this coverage file does not publish v1                                                                                                                                               |

## A. Runner compatibility

All A rows map to `expo`; implementation PR URLs are pending.

| Item | Current path / finding                                                                                     | Dependencies and unmet acceptance evidence                                                                                                        |
| ---- | ---------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------- |
| A1   | `runner.mjs`, `RnqnxConsumerApp.cmake`: distribution manifest exists; embedded `rnqnx-runtime.json` absent | Define API-first runtime metadata, explicit target/arch/mode/fingerprint and compiled module signatures; verify embedded binary contents          |
| A2   | `nitro-modules.mjs`, `prepare-native.mjs`: ADR-0019 metadata has no source signature                       | Canonical hash of package metadata, native sources, CMake and config; same-version native edit must invalidate compatibility                      |
| A3   | `build-native.mjs`: descriptor comparison mixes native and bundle state                                    | Three independent can-run/regenerate/recompile decisions with separate behavior tests; depends on A1-A2                                           |
| A4   | `vnext/probes/playground/main.cpp`: no module preflight before evaluation                                  | Native missing/mismatched-module screen; stock runner rejects added-module fixture and custom dev build accepts it on device; depends on A1-A3/G4 |

## B. Dev loop

All B rows map to `expo` and `developer-path`; implementation PR URLs are pending.

| Item | Current path / finding                                                | Dependencies and unmet acceptance evidence                                                                                                                           |
| ---- | --------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| B1   | `dev-server.mjs` directly owns Metro and raw stdin                    | Pinned Expo CLI supervision adapter and QNX IPC actions; Expo owns stdin/logs/reload; preserve excluded-target behavior                                              |
| B2   | `dev-server.mjs`, `metro-config-qnx`: no compatibility request gate   | A/H1 session contract; deny incompatible QNX bundle/delta requests while mixed-platform requests remain unchanged                                                    |
| B3   | `dev-server.mjs`, `QnxDevSupport.cpp`: no confirmed process ownership | H5 exact executable/PID/start identity; native dependency invalidation stops only owned app with SIGTERM despite an open refresh socket; end-to-end device recording |

## C. Expo config and startup

All C rows map to `expo`; implementation PR URLs are pending.

| Item | Current path / finding                                                      | Dependencies and unmet acceptance evidence                                                                                                                     |
| ---- | --------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| C1   | `expo-qnx-config-plugins/index.js`, `prepare-native.mjs`: extra fields only | QNX base-mod namespace for descriptor/CMake and later security fragments; third-party composition and byte-identical second prebuild; preserve window contract |
| C2   | Config-plugin package has no upstream port README record                    | Read exact expo-desktop source; keep adopted ports faithful and record upstream commit/file differences in each affected package                               |
| C3   | Native Expo installer precedes `host.evaluate` in playground                | Real Hermes first-application-instruction regression must fail when native installation order regresses; H11 required-class audit remains open                 |

## D. Build modes and native selection

All D rows map to `expo` and `performance`; implementation PR URLs are pending.

| Item | Current path / finding                                                               | Dependencies and unmet acceptance evidence                                                                                                                                                                          |
| ---- | ------------------------------------------------------------------------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| D1   | `build-native.mjs`: real QNX uses debug preset; host-preview is a host evidence lane | Separate go/dev/preview/release contracts and directories, with matching Metro, native-set and embedded-bundle behavior                                                                                             |
| D2   | `project.mjs`, `nitro-modules.mjs`: no production-map pruning                        | Metro production source map to package ownership, explicit includes and required closure; unknown native modules retained; depends on A/H4                                                                          |
| D3   | `prepare-native.mjs`, consumer CMake: all discovered modules flow through            | Deterministic `selection-report.json`, per-module reasons and agreement across config/autolinking/codegen/binary/runtime; paired device RSS with/without pruning; Hello World restriction requires resolution first |

## E. QNX runtime correctness

All E rows map to `cold-boot`, `performance` or `device-parity`; implementation
PR URLs are pending. Extend Plans01/03/07/08/14/15 as applicable. A Plan subagent
must precede the new scheduling and security ADRs.

| Item | Current path / finding                                                               | Dependencies and unmet acceptance evidence                                                                                                                            |
| ---- | ------------------------------------------------------------------------------------ | --------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| E1   | `vnext/src/runtime/`, native workers: explicit scheduling map absent                 | Baseline thread/policy/priority traces, Plan/ADR, all named thread owners, explicit tested policy/priority setting; stay at 1-63; no unset explicit scheduling        |
| E2   | QNX runner security type/policy absent                                               | Plan/ADR, non-root target run, `secpolgenerate` abilities reviewed by hand; least privilege and denied-operation tests                                                |
| E3   | `tools/qnx-winmgr/` uses `screen_get_event`, with no existing Neutrino channel       | Decide Screen/pulse multiplexing; registered `PROCMGR_EVENT_PROCESS_DEATH` using `MsgRegisterEvent(SYSMGR_COID)` and lifecycle cleanup; target death/reuse tests      |
| E4   | `/dev/rnqnx` absent; [Plan07](../platform-modernization/07-qnx-ipc.md) owns protocol | Read sample managers, API-first device-control contract, secure attach/no-lock dispatch, bounded validated messages, permissions and reload/status/trace target tests |
| E5   | winmgr and Qt QPA event paths exist; drain headroom unmeasured                       | Audit fixed 200-event default and both queues under drawer load; raw latency/drop/headroom receipt or justified graphics.conf change                                  |
| E6   | No librcheck runner preset                                                           | Add bounded pnpm device harness with LD_PRELOAD/MALLOC_FILE, stack caveat, leak/corruption fixtures and logs; depends on qualified runtime                            |
| E7   | No ISR-style InterruptAttach found in searched production paths                      | Add regression prevention and GPIO client policy; inspect any future GPIO implementation against rpi-gpio resource-manager contract                                   |

## F. Qt Quick and QML

All F rows map to `performance`, `distribution` or `device-parity`;
implementation PR URLs are pending. Extend the existing runtime/frame-pacing
plans. A Plan subagent must precede the ADR-0002 amendment.

| Item | Current path / finding                                                                  | Dependencies and unmet acceptance evidence                                                                                                                                                                                             |
| ---- | --------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| F1   | `QtQuickItemFactory.cpp`, chrome/native-UI/video QML: inline/cached or resource paths   | AOT module experiment, QML_ELEMENT and resource loading, invalid-AOT CI, same-target 1000-item/first-frame comparison and ADR amendment; retain faster mounting path; stop on prohibited regression                                    |
| F2   | No required qmllint lane                                                                | Module-aware lint with unqualified access and missing annotations treated as errors; exact-head CI                                                                                                                                     |
| F3   | No selected-loop receipt in repository                                                  | Pi `qt.scenegraph.general` log; audit QSG ownership/updatePaintNode and afterAnimating worklet boundary; depends on distribution baseline                                                                                              |
| F4   | No list batches/overdraw receipt                                                        | Qualified 1000-row workload, QSG_VISUALIZE captures and target identity; pair with performance traces                                                                                                                                  |
| F5   | `doctor.mjs` is static host metadata; preflight covers only part of target requirements | Explicit target mode checking Screen/slogger2/tmp, library/plugin/QML/font/display env and missing physical-size fallback; H27 verbose diagnostics                                                                                     |
| F6   | `runner.mjs` compares target library names only                                         | Exact SDP installation identity and reliable device library verification before launch; cross-QSTI proof; ask owner if reliability cannot be established                                                                               |
| F7   | `component-support.md`, media experiment                                                | Correct the prompt against current Qt6.12 docs: Multimedia compiles/is supported on SDP8, but its QNX audio playback/recording backend is unavailable; D-Bus/Extension Modules unsupported; preserve separately scoped FFmpeg evidence |

The [current Qt QNX support page](https://doc.qt.io/qt-6/qnx-support.html),
retrieved by Explore F as Qt 6.12, differs from the prompt's blanket Multimedia
assertion. A support-matrix update still needs exact source citations and review;
this record does not promote the existing media experiment to general support.

## G. Design surfaces

All G rows map to `window-chrome`, `app-identity` or `expo`; implementation PR
URLs are pending. [PR #164](https://github.com/mikevocalz/react-native-qnx/pull/164) supplies the design documents, with UI behavior and device receipts still pending. Each screen needs the ordered Mobbin/design/research/copy/
accessibility/critique/handoff workflow from the request. Chrome must remain
operable while JS is blocked; the current shared GUI/JS thread prevents that
acceptance claim.

| Item | Current path / finding                                       | Dependencies and unmet acceptance evidence                                                                                                                   |
| ---- | ------------------------------------------------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| G1   | `native/window-chrome/WindowChrome.qml`: macos layout exists | References, token/copy/focus/contrast/target-size audit, independent critique; both blocked-JS operation and target layout/input receipts                    |
| G2   | Same QML/model has windows layout                            | Same per-screen design workflow, runtime switching and target controls/minimize/restore/close-guard receipts                                                 |
| G3   | `native/app-identity/`, launcher tooling                     | Measured short-name rendered width, overflow failure and defensive elision; icon in all required locations; maximum-fit and short names keep identical tiles |
| G4   | Native module preflight absent                               | A compatibility results, native screen states and recovery/build action, per-screen design outputs; before-bundle target rejection proof                     |
| G5   | Native dev loading/error support exists                      | Connecting/bundle-error/reconnecting states audited and handed off; deterministic recovery and keyboard/focus evidence on target                             |

## H. Additional review items

All H rows remain pending; implementation PR URLs are pending. An existing
partial behavior below must be preserved and tested in its eventual slice.

| Item | Workstream/gate and current path                                           | Dependencies and unmet acceptance evidence                                                                                                                                                                  |
| ---- | -------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| H1   | A/expo; prepared state under `build/<platform>`                            | App-local `.rnqnx/` records with purpose/ownership/lifetime table; compatibility and native state readers agree                                                                                             |
| H2   | A/expo; `build-native.mjs` lacks build lock/reuse records                  | Concurrent same-project graph mutation prevented; successful build reuse requires verified identity; failure/stale lock tests                                                                               |
| H3   | A/expo; `runner.mjs` already filters target before versions                | Preserve host/QNX exclusion; add explicit architecture-first and signature discovery tests                                                                                                                  |
| H4   | A/expo; direct-only native discovery                                       | Required transitive/peer closure, optional-peer rules, duplicate-version hard failure, test-only rejection                                                                                                  |
| H5   | B/developer-path; detached launcher in `dev-server.mjs`                    | Separate bundle URL/packager location, exact executable and confirmed session process identity; B3 invalidation tests                                                                                       |
| H6   | B/developer-path; `add-qnx.mjs` is additive/idempotent                     | Expo composition contract and unrelated react-native.config.js conflict regression; mobile/web commands unchanged                                                                                           |
| H7   | G/window-chrome; no completed multi-root Windows API                       | Shared module state/per-root Zustand state, root props, guards/events lifetime and exactly-once delivery; reconcile older one-window-per-app rule before device work                                        |
| H8   | C/expo; capability subpaths incomplete                                     | Ship API-first windows/system with explicit unavailable results; support matrix records other capability gaps and replaceable backends                                                                      |
| H9   | distribution; runner hashes and publication attestation are separate       | Persist built/hashed/attested/clean-device-verified stages with exact artifacts; current release workflow builds nothing and does not establish compile-machine provenance                                  |
| H10  | C/expo; QNX resolver substitutions are request-scoped                      | Mixed-platform requests/cache regression proves unchanged non-QNX bundles when Expo defaults are introduced                                                                                                 |
| H11  | C/expo; native expo registry plus JS class polyfill                        | Native eager installation of audited required classes before application evaluation; missing-installer/order negative tests                                                                                 |
| H12  | C/expo; app's Expo config copy currently wins                              | Bundle one pinned config/config-plugins pair, record dependency decision and test conflicting app copies                                                                                                    |
| H13  | All tooling; existing helpers throw                                        | Every new CLI operation returns named-step discriminated results and callers handle cases exhaustively; partial-success tests                                                                               |
| H14  | developer-path; source template copies                                     | Versioned bare-minimum/blank-typescript tarballs, create-app template selector, selected version/hash and clean-host receipt                                                                                |
| H15  | E; signal machinery absent                                                 | SIGDOOM slog2/trace flushing and designated sigwaitinfo SIGCHLD owner, child-spawn semantics and shutdown tests                                                                                             |
| H16  | E; Plans07/14/15 already constrain IPC/affinity                            | rcvid_t end-to-end and cluster-valid runmasks in actual implementation; installed-header compile and target checks                                                                                          |
| H17  | E1; priority ADR absent                                                    | Exact-source IST254/255 facts, normal-thread scheduling implications and bounded app priorities in ADR                                                                                                      |
| H18  | device-parity; `tools/qnx-input/rnqnx-inject.c`                            | Read hid-input-demo/usb-to-screen and compare; physical keyboard/touch receipts distinguish synthetic injection                                                                                             |
| H19  | E4; `/dev/rnqnx` absent                                                    | Read rpi-gpio/rpi-thermal/rpi-mailbox before manager implementation and record borrowed conventions                                                                                                         |
| H20  | G; stock-image conventions unverified                                      | Read simple-terminal; compare actual window/input conventions on named QSTI image                                                                                                                           |
| H21  | developer-path; README says Linux host, handoff records Colima             | docker-mac source explicitly uses linux/amd64 and licensed Linux tools; fresh bounded cross-build/docs receipt needed; upstream privileged whole-home script and old CMake are unsuitable to copy unchanged |
| H22  | E/device-parity; winmgr CREATE/POST/CLOSE handlers and posted guards exist | Executable event-sequence regression proves no visibility before first post, including property and manager-message paths                                                                                   |
| H23  | E/plans; existing Plan08/15 still discuss HAM                              | Record exclusions of recursive Make/Qnet/endianness-alignment shims/HAM in existing plans; preserve CMake and aarch64le scope; procmgr recovery still needs implementation                                  |
| H24  | F3/performance; no animation-loop comparison                               | Selected-loop receipt; if basic, measured presented intervals for basic/threaded before choosing policy                                                                                                     |
| H25  | F; `WebGpuItem.cpp` uses updatePaintNode                                   | Per-item scene-graph lifetime/OwnedByParent audit and teardown tests; account for invisible-item updates                                                                                                    |
| H26  | F1; no qmlRegisterType found                                               | Typed QML functions, declared module dependencies, qualified lookups and compiler evidence during AOT conversion                                                                                            |
| H27  | F5; doctor lacks target verbose aids                                       | Print QT_DEBUG_PLUGINS/LD_DEBUG/QT_LOGGING_TO_CONSOLE guidance in explicit verbose target diagnostics                                                                                                       |
| H28  | G; custom-cursor target setting unverified                                 | Enable only where chrome needs it; named-target move/resize cursor receipt and fallback                                                                                                                     |
| H29  | F/distribution; target support record needs update                         | Cite Qt6.12 AArch64le/x86-64 SDP8 support and Extension Module exclusion; align with corrected F7 support scope                                                                                             |

## Required continuation

Use the [existing roster](../platform-modernization/ROSTER.md) and
[contributor roles](../marathon/ROSTER.md) for each small PR. Distribution's DRI
role is Release / dependency engineer; independent review includes Security /
QA and QNX regression roles. Each native API, scheduling/security ADR and design
slice needs its own source audit, required skill outputs and independent review.
Keep device-dependent PRs draft until their target receipts exist. Read
[the handoff](../handoff/HANDOFF_2026-11-13.md) before any device action: it
prohibits software reboots and Hello World on the owner's Pi, and requires
preserving input, one app instance and the established window contract.
