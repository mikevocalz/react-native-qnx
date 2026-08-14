# TEST_STATUS.md

## Latest host validation — 2026-08-13

- Host: macOS 26.6.1, Darwin 25.6.0 arm64, Apple Clang 21.0.0.
- Repository package manager: pnpm 11.21.0.

| Check                                 | Result         | Evidence                                                                                       |
| ------------------------------------- | -------------- | ---------------------------------------------------------------------------------------------- |
| `pnpm validate:macos`                 | Passed         | All eight host stages completed with exit 0.                                                   |
| Prettier                              | Passed         | All supported files match Prettier 3.9.6.                                                      |
| TypeScript                            | Passed         | TypeScript 6.0.3 `tsc --noEmit`, strict JS checking enabled.                                   |
| Installed dependency/source contracts | Passed         | Exact pins, Expo graph, Hermes tag, and 19 named installed-source checks passed.               |
| Scoped `xcode>uuid` compatibility     | Passed         | uuid 11.1.1 resolves; xcode's call site and CommonJS `uuid.v4` export are present.             |
| `pnpm audit --audit-level moderate`   | Failed         | Two high `image-size` DoS advisories remain; no fixed registry release exists.                 |
| JavaScript tests                      | Not applicable | `tests/js` contains no test files; the runner reported an explicit skip.                       |
| QNX Screen C++ host target            | Passed         | Strict C++20, ASan, and non-recovering UBSan; CTest 1/1 passed.                                |
| Internal Screen wrapper cases         | Passed         | Eight cases cover setup, ARGB fill, buffer refresh, cleanup, moves, and init/runtime failures. |
| Workspace package build               | Not applicable | No app/package manifests exist yet; 15 directories are intentionally scaffold-only.            |
| Manifest validation                   | Passed         | 0 manifests and 15 scaffold-only directories reported accurately.                              |
| Architecture validation               | Passed         | Required tracked scaffold directories and production files are present.                        |
| Nitrogen generated-output check       | Not applicable | No `nitro.json` exists; QNX generation has not been designed.                                  |
| `git diff --check`                    | Passed         | No whitespace errors.                                                                          |

The CTest count is one executable; that executable reports eight independent
test cases. Host tests compile against `tests/cpp/qnx-screen-fake`, never an
installed QNX header or `libscreen`.

## Expected unsupported-host diagnostic — 2026-08-13

`bash scripts/configure-qnx.sh aarch64 Debug` exited 1 on macOS as expected. The
primary CMake error was:

```text
Environment variable QNX_HOST is not set.
```

CMake also emitted downstream compiler/generator errors after the toolchain
fatal error. This is not a QNX build attempt and is not a pass.

`bash scripts/doctor.sh` also exited 1 as expected: eight QNX environment errors
and five host/target warnings. Its CMake, Ninja, Node, exact pnpm, Python, and
installed dependency/source checks all passed.

## QNX validation status

| Suite                                      | Status  | Reason                                                                     |
| ------------------------------------------ | ------- | -------------------------------------------------------------------------- |
| Installed QNX `screen/screen.h` audit      | Not run | QNX SDP 8.0.5 is not installed in this workspace.                          |
| QNX AArch64/x86-64 cross-compilation       | Not run | No `QNX_HOST`, `QNX_TARGET`, `qcc`, `q++`, or licensed sysroot.            |
| `libscreen` linkage                        | Not run | QNX Screen development library is unavailable.                             |
| Virtual/physical QNX target runtime        | Not run | No QNX target is configured.                                               |
| Target resource, presentation, and cleanup | Not run | Requires the installed Screen service, BSP/driver, compositor, and target. |

Do not promote any QNX queue state from host evidence. The exact continuation is
recorded in `QNX_VALIDATION_QUEUE.md`.

## Dependency-audit exception

The original audit also reported vulnerable uuid 7.0.3 through xcode. A scoped
override to uuid 11.1.1 removed that moderate advisory, and its installed call
shape is part of `validate:dependencies`. The two remaining high findings are
`UPSTREAM-DEP-001`; they are recorded rather than ignored. Consequently, “full
host validation passed” does not mean “registry security audit is clean.”
