# S6 native runner preflight (hardening item A4), 2026-11-21

Every result below is labelled by where it ran.

| Where              | Ran                                         | Not run                                             |
| ------------------ | ------------------------------------------- | --------------------------------------------------- |
| host (macOS arm64) | yes: native unit test, JS tests, formatting | full playground host (needs the Hermes build, CI)   |
| QNX VM             | yes: the same unit test, cross-built        | the playground host and any app (no QNX host build) |
| Pi 400             | no                                          | not touched, by instruction                         |
| release board      | no                                          | no board                                            |

No gate changes. `expo` and `developer-path` stay `pending`.

## What A4 requires

From [hardening-coverage.md](../../v1/hardening-coverage.md), row A4: a native
missing or mismatched-module screen; the stock runner rejects an added-module
fixture and a custom development build accepts it, on a device; depends on
A1 to A3 and G4. Slice S6 in the prompt lists the launch-time comparison on
React Native version, Hermes bytecode version and Nitro ABI as host work; the
[first S6 receipt](../s6-runner-2026-11-20/README.md) left the native half open.

## What this slice adds

- `vnext/src/runtime/RuntimePreflight.cpp`: compares the runner's
  `rnqnx-runtime.json` with the app's new `rnqnx-app.json` before a JavaScript
  runtime exists. Contract, states and the `cpp` review note:
  [API_CONTRACTS.md](../../nitro/API_CONTRACTS.md#s6-native-runner-preflight-internal-no-javascript-api).
- `rnqnx project run --runtime` writes `build/<platform>/rnqnx-app.json`.
- `main.cpp` runs the check at each start and reload; a failure prints
  `RNQNX_PLAYGROUND_PREFLIGHT_FAILED` and exits 1, or shows the dev error
  overlay when dev support is on.
- Fixtures shared by the native and JavaScript tests:
  `tests/fixtures/runner-preflight/` (`runtime-stock.json` lacks the module,
  `runtime-custom.json` links it, `app-added-module.json` needs it).

## Red, then green (host)

With the module comparison loop disabled in `RuntimePreflight.cpp`, the stock
runner accepted the app that adds a module:

```text
CHECK failed at line 48: rejected
exit=1
```

With the code restored: `runtime preflight: all checks passed`, exit 0
([host/native-test.txt](host/native-test.txt)). The test builds with
`-Wall -Wextra -Wpedantic -Werror` and AddressSanitizer plus UBSan (Apple clang
21.0.0, CMake 4.4.5, Qt 6.12.0 from Homebrew).

JavaScript: `node tests/js/qnx-runner-compat.test.mjs` reports
`qnx-runner-compat: 67 checks passed` ([host/js-test.txt](host/js-test.txt)),
61 before this slice (measured on a clean checkout of `main`). With the
`run.mjs` change reverted the same test fails, because `project run --runtime`
no longer writes the file. The test also checks that the file the writer
produces for a fixture app equals `app-added-module.json`, so the writer and the
native reader cannot drift apart.

## QNX VM (QEMU, TCG, software render): unit run

Not Pi 400 evidence and not release-board evidence.

- Guest: `QNX qnxvm 8.0.0 2026/06/05-16:21:14EDT QEMU_virt aarch64le`
  ([vm/guest.txt](vm/guest.txt)).
- Build: aarch64le cross-build in the `qnx805-build` container, `q++` 12.2.0,
  Release, Qt 6.12.0 for QNX, `-Werror` on ([vm/cross-build.txt](vm/cross-build.txt)).
  Binary SHA-256 on the host before `scp`:
  `af571bf488d542a983938f86813559060aa003f8a7e3f3f84d2f3e1aacad36ed`
  ([vm/test-binary.sha256](vm/test-binary.sha256)). The guest has no
  `sha256sum`, so the hash on the guest was not compared.
- Run: `RNQNX_PREFLIGHT_FIXTURES_DIR=/data/s6pf/fixtures ./rnqnx_runtime_preflight_test`
  printed `runtime preflight: all checks passed`, exit 0 ([vm/run.log](vm/run.log)).

This shows the comparison and its text behave the same on QNX aarch64le. It
does not show a runner on QNX refusing an app: no QNX build of the playground
host exists in this slice.

## Not done

- No device run. The A4 acceptance (stock runner rejects the fixture, custom
  build accepts it, on a device) is not met.
- No dedicated screen (G4): no copy-command or close button; the text goes to
  stderr, or to the dev error overlay with `RNQNX_ENABLE_DEV_SUPPORT`.
  Dev-overlay rendering of this message was not run.
- No native-source signature (A2): a same-version native edit is not detected.
- A custom development build writes no `rnqnx-runtime.json` (A1), so it is
  not checked, which is why it "accepts" the app today.
- `project run` still stops at `deploy` (`needs device`), so `rnqnx-app.json` is
  written to `build/<platform>/` but copied nowhere.
- The two new playground-host ctest cases (`rnqnx_playground_rejects_added_module`,
  `rnqnx_playground_accepts_added_module_in_custom_runner`) and the `main.cpp`
  wiring could not be built on this machine; they run in the
  `Qt 6.11 / RN 0.88 / Hermes` jobs. See the PR for the results.
- Review: `ecc:cpp-reviewer` read the C++ and found no critical or high issues.
  Two of its notes were applied (initialized `hermesBytecode`, files re-read at
  each start) and a missing-key test was added. Two are left open: the file
  uses exceptions internally (the VM cross-build compiled it with the
  toolchain defaults; the full host's QNX flags were not checked), and a
  non-dev failure exits with stderr text only, until G4.
