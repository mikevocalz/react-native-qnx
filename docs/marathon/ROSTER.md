# MarathonOS integration: multi-agent / contributor roster

Status: proposed responsibilities; **not** claims of people hired, assigned, or having completed work. Every slice must have a DRI and reviewer named in its actual PR. Never invent collaborator identities.

| Role / agent                                  | Owns                                                                        | Primary source paths                                           | Deliverable                                             | Blocking reviewer            |
| --------------------------------------------- | --------------------------------------------------------------------------- | -------------------------------------------------------------- | ------------------------------------------------------- | ---------------------------- |
| Platform architect / integrator               | target boundaries, shared Qt ABI, contract, upstream coordination           | docs/adr, docs/marathon, CMakeLists.txt                        | migration map, no renderer fork, staged sequence        | Qt lead + QNX maintainer     |
| Qt Quick / Fabric engineer                    | preserve Yoga geometry, shadow mutation, rendering, input                   | vnext/src/views/qt, vnext/src/fabric, cmake/RnqnxReactQt.cmake | Linux Qt build + conformance images                     | RN runtime lead              |
| Hermes / JSI engineer                         | runtime bootstrap, event beat, timers, errors                               | vnext/src/runtime, cmake/dependencies/Hermes.cmake             | RN 0.88.1 pinned host and release JS run                | Qt engineer                  |
| Linux / Wayland engineer                      | QGuiApplication/QQuickWindow, xdg-shell app_id, Qt Wayland env, musl, build | native/platform/linux-host, CMakeLists.txt                     | native Marathon client and reproducible device package  | upstream Marathon engineer   |
| QNX regression owner                          | maintain SDP, QNX-specific image/Screen/Nitro/Expo paths                    | native/platform/qnx, scripts/pi-*, CMakePresets.json           | unaffected QNX bundle and independent device probe      | platform architect           |
| Metro / Expo developer tools engineer         | `marathon` JS platform, resolver, bundling, CLI, codegen, Expo config       | packages/metro-config-qnx, packages/expo-*, tests/js           | deterministic QNX+Marathon JS bundles                   | release lead                 |
| Nitro 1.x migration engineer                  | v1 alpha generator/runtime JS/C++ ABI, threading, exceptions                | packages/qnx-nitro-runtime, cpp, scripts                       | compile-tested migration report and gate                | Hermes engineer              |
| Expo Modules API 2.0 engineer                 | distinguish v2 Swift/Kotlin beta from Linux QNX JSI platform                | packages/expo-modules-core-qnx, expo-qnx-autolinking           | capability matrix and honest unsupported branches       | dependency lead              |
| Marathon shell integration engineer           | permissions/runner, D-Bus, lifecycle, keyboard, app manifest                | native/platform/linux-host, docs/marathon, upstream RFC        | upstream-approved native runner/install contract        | upstream Marathon maintainer |
| Release / dependency engineer                 | semver, exact npm dist-tags, pnpm lock, Expo/RN/Hermes ABI                  | package.json, pnpm-lock.yaml, packages/*/package.json, scripts | frozen install + exact source checks                    | QNX regression owner         |
| Security / QA engineer                        | sandbox, denied capability, code signing, conformance, performance          | tests, .github/workflows, docs/marathon                        | real logs + trace + failure tests                       | platform architect           |
| Documentation / developer-experience engineer | quick start, examples, support matrix, migration, known gaps                | README.md, docs/getting-started, apps/*                        | install and run tutorials verified by a new contributor | QA owner                     |

## Integration dependency ordering

- Foundation contract + skill review -> npm/ABI audit (can occur in parallel with Linux host discovery).
- Metro resolver and platform identity -> host runtime wiring -> Wayland view and input -> permissions and lifecycle -> launcher/packaging.
- Nitro/Expo migrations are deliberately isolated from the baseline host render path, until compile and runtime proofs exist.
- QNX smoke/regression is a gate for changing shared Qt or JS overrides, not a reason to misrepresent incomplete device evidence.
- Upstream Marathon compatibility review is required before saying "out-of-box" or including official store packaging claims.

## Handoff / stop conditions

Each agent records: goal; relevant exact files; version/skill source + commit; proposed diff; assumptions; commands run; raw logs/screenshot paths; failures; next reviewer; rollback. If skill unavailable, blocked native toolchain, no device, or unexplained RN/Expo ABI mismatch: stop that scope and open a draft with the blocking evidence rather than inventing a green result.

## Review matrix

| Workstream   | Review must include                                                       |
| ------------ | ------------------------------------------------------------------------- |
| Renderer     | RN Yoga/Fabric tests, screenshot diff, event dispatch, memory lifetime    |
| Wayland      | xdg-shell identity, compositor dimensions/scale, input/text, pause/resume |
| Packages     | exact lockfile + peer check + Expo tested RN/React + Hermes provenance    |
| Nitro / Expo | upstream prerelease API diff, C++ compile, codegen, async error semantics |
| Security     | bwrap permission-denied, secrets/storage restrictions, no privileged IPC  |
| Release      | zero regressions on original QNX Metro output, separate evidence labels   |

## Nitro/API-first owner pairing

- **SDK/API design:** Own the TypeScript contract and 2–3 caller examples before C++ code is changed.
- **Nitrogen/codegen:** Verify that the platform generator supports QNX/Marathon. Until upstream support exists, a hand-built `HybridClass` is only an isolated proof, not a public generated Nitro module.
- **Qt/JSI lifetime:** Review dispatcher shutdown, module disposal, and no callbacks after Hermes teardown.
- **Expo Modules:** Separate SDK 58 API 2.0 beta Swift/Kotlin annotation bindings from the QNX and Marathon JSI host.

All owners must use the exact Margelo `api-design`, `build-nitro-modules` and `cpp` skills where applicable. Do not silently substitute a generic native-module tutorial. See `docs/marathon/SKILLS.md` and Nitro v1 PR #38.
