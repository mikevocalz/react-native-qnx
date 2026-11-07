# Qt input lifecycle validation — 2026-11-07

Status: draft; native runtime validation interrupted by loss of the execution environment.

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
