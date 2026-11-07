# Qt input lifecycle validation — 2026-11-07

## Later follow-up: JavaScript delivery

The [raw Fabric JavaScript-delivery receipt](../evidence/pi400-input-js-2026-11-07.md)
now records12 callbacks on physical QNX and an interception control that fails
at the intended JS assertion. This supersedes only the raw JS-delivery gap below;
USB input, React component interaction and gesture recognition remain unverified.
The earlier native-only results and artifact hashes remain historical evidence.

## Completed follow-up: macOS and physical QNX

DRI `/root`; independent read-only reviewer `/root/renderer_build_audit` reviewed
terminal-state ordering, decorated hit testing, regressions and the target probe.
Input code `843730f` plus fixture `a58196d`, integrated with main/PR42/PR44/PR45 at
`634ef0ac0069a23aab0d75a7d7e5e4ed47c7331f`. No upstream/vendor approval is implied.

The previously unrun host suite exposed a real fixture bug: lexical `const`
bindings from a separate Hermes evaluation were unavailable to the sibling
setup. Explicit fixture globals fix that setup without changing application state.
Independent review also found mouse capture cleared after the terminal callback
and an unregistered decorative QML child swallowing its parent View target.
Capture now clears before dispatch; the opt-in hit test selects the nearest
registered visual ancestor. Added tests cover synchronous native-listener target
removal during release and nested unregistered decorations. OFF retains its
legacy leaf-only hit lookup.

### Actual results

- macOS / AppleClang21 / Qt6.12.0 / MinSizeRel / pinned RN0.88.1 and Hermes:
  full build and **14/14 CTests ON**, including the added target probe; full
  build and **13/13 CTests OFF** (the new probe is omitted). Existing host
  ASan/UBSan settings remain enabled; this is not full third-party instrumentation,
  a Rust sanitizer run, leak check or TSan claim.
- Dependency/source contracts, TypeScript, JavaScript tests, architecture,
  manifests, codegen and production Metro bundle passed locally. The full build
  includes the optional tested Qt/Nitro dispatcher from PR44.
- Licensed SDP8.0.5 / GCC12.2.0 / Qt6.12.0 AArch64: input probe, playground and
  Nitro test all cross-built. The new fixture uses pinned host hermesc260318099.0.5
  bytecode for the lean target Hermes runtime.
- Physical Pi400 as qnxuser: deterministic injected Qt input passes decorated
  View targeting, terminal coordinates/timestamps, retained second contact,
  repeat cancellation/ID reuse, deactivation, removed target/responder, hover
  rejection, mouse ungrab and synchronous native-listener removal during release.
  Marker: `RNQNX_INPUT_LIFECYCLE_OK decorated-hit terminal multi-contact cancel deactivate unmount mouse-reentrant`;
  exit0, before the external35-second watchdog.
- A separate input-enabled candidate rendered the exact Hello World title at
  1920×1080 and printed `RNQNX_HELLO_NITRO_V1_OK platform=qnx architecture=aarch64 add=42`;
  screenshot capture exited0. Probe, HBC fixture and candidate were copied back
  from the Pi and SHA256 matched their local artifacts. Image inspected directly.
  [Receipt](../evidence/pi400-input-lifecycle-2026-11-07.json) and
  [capture](../evidence/pi400-input-lifecycle-2026-11-07.png).

This is **synthetic QObject-filter input on physical QNX**, not hardware USB,
touchscreen/stylus delivery or a gesture-recognizer proof. The target probe consumes
real Fabric payloads in a native listener; target JavaScript handler delivery is
not proved by it. The host regressions separately check Fabric-to-Hermes payloads.
Global coordinates, real pressure, synthesized mouse deduplication, transformed
clipping, target clock-origin equivalence and Marathon/Wayland remain unverified.
No RNGH, Worklets or Reanimated implementation is added. No performance improvement
or target frame-budget measurement is claimed.

The target fixture uses Fabric directly; its bytecode stop hook retires that tree
rather than depending on an absent React AppRegistry. The final run has no missing
stop-surface error. Pi wall clock is unset (January1 in native logs); collection date
above is from the Mac. The running fullscreen app and recovery boot remain intact.

### Reproduce and rollback

```sh
cmake -S . -B build/macos-full-host-tests -DRNQNX_ENABLE_INPUT_LIFECYCLE=ON
cmake --build build/macos-full-host-tests --parallel 4
ctest --test-dir build/macos-full-host-tests --output-on-failure
# Repeat with OFF; expect no input_lifecycle_probe in the OFF inventory.
```

These commands reuse the already configured full RN/Hermes/Qt host graph;
a new checkout must first configure the documented full renderer prerequisites.
The Linux preset below is the clean-host path and is not a macOS preset.

For the existing licensed full QNX graph, additionally configure
`-DRNQNX_ENABLE_INPUT_LIFECYCLE=ON`
`-DRNQNX_INPUT_TEST_HERMESC=/path/to/pinned/260318099.0.5/hermesc`, then build
`rnqnx_input_lifecycle_probe` and `rnqnx_playground_qnx`. Deploy the probe and
`input-lifecycle-setup.hbc` as the unprivileged app user, with the same Qt QPA,
plugin/import/library paths as the validated Hello World. Set
`RNQNX_INPUT_FIXTURE` to the deployed HBC, then run the probe with an external
watchdog. Do not feed JavaScript source to the lean QNX Hermes build.

The feature remains default OFF; rollback is disabling that option or reverting
this additive slice. It changes no dependency pins, native module API, renderer,
window contract, boot files, permissions or licensing basis. Existing private
Qt evaluation and QNX licenses apply; no vendor binaries/sources are committed.
Hosted CI remains refused before runner startup by GitHub account billing/spending
limits; local results are explicitly separate, and no branch-rule override is used.

## Historical interrupted Linux attempt

Historical first attempt: native runtime validation was interrupted. The completed follow-up below supersedes only those missing results.

## Built and checked

Environment: Linux x86_64, Node 26.10.0, pnpm 12.8.1, CMake 3.31.10,
Clang 18, Qt 6.11.1, repository-pinned RN 0.88.1 and Hermes.
The existing host sanitizer preset was enabled; its leak-detection exclusions
were retained. This is not evidence of complete sanitizer instrumentation of
third-party libraries.

Passed:

- Frozen dependency installation and dependency validation.
- Typecheck, architecture and manifest validation.
- All JavaScript tests, rerun after playground wiring.
- QNX React/Nitro/Expo source contract checks (not target execution).
- QNX Metro playground bundle.
- Strict dispatcher and host-test syntax compilation with lifecycle ON and OFF.
- Full ON renderer build: all 854 build steps completed, including the new
  dispatcher, host test and playground runner.
- Diff checks and formatting checks for changed Markdown.

Commands used the exact tools above on PATH:

```sh
pnpm install --frozen-lockfile
pnpm validate:dependencies
pnpm typecheck
pnpm validate:architecture
pnpm validate:manifests
pnpm test:js
pnpm validate:qnx-react
pnpm validate:qnx-nitro
pnpm validate:qnx-expo-modules
cmake --preset linux-host-renderer -DRNQNX_ENABLE_INPUT_LIFECYCLE=ON \
  -DCMAKE_PREFIX_PATH=/tmp/qnx-qt/6.11.1/gcc_64
cmake --build --preset linux-host-renderer
```

Observed end of the successful build:

```text
[849/854] Linking CXX static library librnqnx_react_host.a
[850/854] Linking CXX executable bin/shadow_view_mutation_adapter_host_test
[851/854] Linking CXX executable bin/rnqnx_react_render_host_probe
[852/854] Linking CXX executable bin/qt_react_host_host_test
[853/854] Linking CXX executable bin/rnqnx_playground_host
[854/854] Linking CXX executable bin/shadow_tree_mounting_host_test
```

## Interrupted runtime run

Running the repository validator after this build caused another large rebuild
after reconfiguration. The completed binaries were therefore tested directly:

```sh
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software QSG_RHI_BACKEND=software \
  ctest --preset linux-host-renderer --output-junit /tmp/qnx-ctest-on.xml
```

The last observed output was:

```text
1/11 rnqnx_g0_cxx20_probe                              Passed 0.00 sec
2/11 qnx_screen_surface_host_test                      Passed 0.01 sec
3/11 rnqnx_hermes_probe                                Passed 0.06 sec
4/11 rnqnx_react_render_host_probe                      Passed 1.36 sec
5/11 rnqnx_react_render_host_probe_rejects_bad_bundles  Passed 0.18 sec
Start 6: qt_react_host_host_test
```

The execution environment then became unavailable. No final result could be
retrieved for test 6 or the remaining tests. The six new input cases are within
test 6, so **their runtime results are unverified**, despite successful compilation.
Neither a full CTest pass nor a validator pass is claimed. A full OFF runtime run
was not completed. Temporary logs were not recoverable after disconnection;
the excerpts above were observed before it.

GitHub Actions run 42348107736 also reported failure for both matrix jobs with
zero executed steps. Job logs were unavailable (404). Its cause was not established
from this run; this is not a successful CI result.

## Required follow-up

Rerun the documented full host gate for both ON and OFF on an available host,
inspect all test results, and fix any failures. Then validate actual QNX input and
Marathon/Wayland input independently. No QNX device or Wayland result was obtained
for this change. Keep the PR draft and the option OFF until the required evidence
is available.

The playground flag connects its window filter, mounting adapter and responder
delegate; it does not implement RNGH or Reanimated. New synthetic-input tests use
a QObject filter source to avoid secondary mouse synthesis, then inspect real
Fabric-to-Hermes payloads. Target-removal tests verify native emission/cleanup
while the emitter is live, not delivery to an already unmounted React component.
