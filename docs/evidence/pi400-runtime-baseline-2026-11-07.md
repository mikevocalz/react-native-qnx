# Pi 400 bounded runtime baseline — November 7, 2026

## Candidate and scope

Base `24019f39a409cd0540c31f4e017b7003201bdca4`, plus this slice's source
changes. Exact source, lockfile, deployed binary and fixture hashes are in the
[machine receipt](pi400-runtime-baseline-2026-11-07.json). The application graph
retains RN 0.88.1, React 19.3.0, Expo 58.0.22, Metro 0.87.1, Hermes
260318099.0.5 (source `9a115f15785851f4ff2747ebb090ddd066fc8ad2`) and Nitro
alpha 105 `g69d365fc9b18`. No ABI, dependency or generated binding changed.

The physical lane used Pi 400, licensed SDP 8.0.5/GCC 12.2 AArch64, MinSizeRel,
Qt 6.12.0 QNX QPA/OpenGL with the existing isolated Pi 4 Screen/DRM stack and Qt
platform plugins. Input lifecycle and owned Nitro dispatcher experiments were
ON in the existing target graph. The existing Hello World Nitro bytecode bundle
was reused. The Qt evaluation remains private and expires November 17; this
receipt grants no production distribution rights.

Target readback hashes cover the deployed application, supervisor, normal HBC,
Qt Core library, QNX platform plugin, boot config and selected recovery image.
The HBC matches the earlier production bundle hash. Runtime/graphics setup is
the unchanged [Pi runbook closure](../getting-started/pi400-hello-world.md) and
[owned dispatcher prerequisite](pi400-nitro-qt-2026-11-07.md). Selected paths and
kernel identity are recorded in the machine receipt; no licensed binaries are
published with it.

The change keeps the shared Fabric/Hermes/Qt renderer. It checks real app startup
and surface-stop errors before publishing screenshot success. A standalone
POSIX supervisor owns one direct child; the host Python harness adds repeated
trials, marker/PNG checks and receipts. Neither changes boot configuration.

## Executed gates

| Lane                       | Command or operation                                                                                                                                       | Result                                                                                                                                              |
| -------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------- |
| Host source                | `pnpm typecheck`, `pnpm test:js`, `pnpm validate:dependencies`, `pnpm nitro:platform:check`, `pnpm validate:qnx-nitro`                                     | Passed; contract checks are source checks, not target proof.                                                                                        |
| Host native                | Build `rnqnx_playground_host` and `rnqnx_runtime_baseline_supervisor`; `ctest --test-dir build/macos-full-host-tests --output-on-failure`                  | 16/16 passed, including existing dispatcher suite and new screenshot/start-error/stop-error tests.                                                  |
| Host supervision           | `node tests/js/runtime-baseline.test.mjs`; `node tests/js/runtime-baseline-qnx.test.mjs`                                                                   | Passed repeated success, missing marker, truncated PNG, failed health, crash, deadline, recovery, stale artifact and TERM/HUP child cleanup checks. |
| QNX cross-build            | Existing guarded CMake build of `rnqnx_playground_qnx` and `rnqnx_nitro_qt_test`; `q++ -Vgcc_ntoaarch64le -std=c++17 -Wall -Wextra -Werror` for supervisor | Passed. POSIX declarations require `_POSIX_C_SOURCE=200809L`, now in the source.                                                                    |
| Physical application       | Native supervisor, 35-second deadline, ten separate fullscreen Hello World trials as app user                                                              | All ten exited zero with Nitro `add=42`, screenshot marker and complete 1920×1080 PNG.                                                              |
| Physical failures          | Real AppRegistry startup and surface-stop exception fixtures                                                                                               | Each exited 1 with its exact failure diagnostic; no PNG or success marker.                                                                          |
| Physical watchdog/recovery | Direct sleeping child with one-second deadline; deliberate ABRT child; subsequent Hello World launch                                                       | Exit 124, exit 134, then normal application exit zero respectively.                                                                                 |
| Physical health            | Process inventory after trials                                                                                                                             | Original continuously running fullscreen app retained; no trial application left running. Network remained reachable.                               |

Raw private logs and captures are retained in the operator's SDK workspace under
`runtime-baseline-capture`, `runtime-baseline-physical.log` and
`runtime-baseline-controls.log`. Target timestamps use its unset January clock;
the collection date above is the Mac's date. No cross-clock measurement follows.

Trial 1 and trial 10 were visually inspected and show the exact title
`React Native QNX: Hello World`. Other captured files passed complete PNG and
dimension checks; their title was not individually inspected. Capture is not a
new observation of HDMI presentation. The generic harness intentionally does not
claim content or presentation verification.

## Limits, ownership and rollback

This implements Plan 01's bounded application tooling and failure-reporting
slice. Cold-boot trials, automatic graphics recovery, physical input, startup
latency, frame pacing, arbitrary native producer cancellation and a threading
decision remain open. The Python host harness and native physical trials are
separate lanes; an authenticated remote transfer adapter is operator supplied.
The native supervisor supports direct executables, not daemonized descendants.

The host graph uses its existing partial ASan/UBSan instrumentation and disabled
leak checks. This is not full third-party instrumentation, a TSan result or a new
Linux Wayland/Marathon-shell run. Those environments remain not run. Hosted CI
was previously refused before startup by GitHub billing; local and physical
results above do not claim hosted CI success.

Implementation roles: root owns supervision, device trials and evidence;
`runtime_harness` owns host supervision; `runner_failure` owns runner error
handling and failure fixtures. `renderer_build_audit` provides independent
source review. Human DRI/reviewer assignments remain unassigned.

Applied skills: repository `qnx-native-api` and `qnx-platform-validation`;
Margelo `api-design`, `build-nitro-modules`, `cpp` at the source pins in the
[skills lock](../platform-modernization/skills.lock.json), preserving the actual
v1 API and explicit lifetime boundary; `no-ai-slop` for the evidence prose. No
new binding or API shape was invented. No premium Forge code or private licensed
runtime artifact is included in this change.

Rollback is to redeploy the recorded prior application executable and omit the
new trial tools. The working continuous app, recovery image, graphics libraries,
default 640×480 window and explicit fullscreen contract were preserved. The next
runtime slice must address a measured or directly tested missing behavior;
passing these trials does not complete every Plan 01 acceptance row.
