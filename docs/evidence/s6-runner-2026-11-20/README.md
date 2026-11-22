# S6 runner: analyze, packaging and launch-time compatibility check (2026-11-20)

Scope: `rnqnx analyze`, `rnqnx runner pack`, `rnqnx runner check`, and
`rnqnx project run --runtime`. Every result below is labelled by where it ran.

| Where              | Ran | Not run                               |
| ------------------ | --- | ------------------------------------- |
| host (macOS arm64) | yes | -                                     |
| QNX VM             | no  | not run: no VM available to this lane |
| Pi 400             | no  | not run: board offline                |
| release board      | no  | not run: no board                     |

No QNX runner was built: this lane has no QNX SDP. The one runner built is a
macOS arm64 host runner, for exercising the tooling. Nothing was published; no
tag, release or npm publish was made.

## Host environment

macOS 27.0 arm64, Homebrew Qt 6.12.0, Node 26.13.0, hermesc from
`hermes-compiler` 260318099.0.5 (the React Native 0.88.1 pin). Source commit for
the native build: `746f52d0670837f4e69df9cccecbafd46fcab14d` (no native source
changed in this slice).

## Host runner build (host)

```text
cmake -S . -B build/playground -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DRNQNX_BUILD_HERMES=ON -DRNQNX_REQUIRE_FULL_HOST_RENDERER=ON
cmake --build build/playground --target rnqnx_playground_host
```

The build linked `bin/rnqnx_playground_host`. Metro's bundling steps hang
waiting for Watchman on this machine (the log shows `Waiting for Watchman
watch-project`); the build was finished with Watchman off the `PATH`. This is a
machine problem, not a repository change.

The stage holds `bin/rnqnx_playground_host`, `lib/libhermesvm.dylib`,
`lib/libjsi.dylib` and a `NOTICE.txt`, with the build-tree rpaths replaced by
`@executable_path/../lib` and re-signed ad hoc. Qt is not in the archive; the
binary loads Homebrew Qt by absolute path. It runs on this machine only.

## Packing and fetching (host)

- `runner pack` produced `macos-arm64-dev-0.1.0-host-test.tar.gz`, SHA-256
  `566ac418e7658ed267ddb51e23ba7c2d13ceccb55997a0644e6a28f9ee6a3734`,
  3774408 bytes, 5 entries (`pack.log`). The bytecode version in its
  descriptor, 99, was measured from the stage's own `hermesc`.
- `runner fetch` against a local HTTP server verified size and SHA-256 and
  kept the archive (`fetch.log`). The server and archive are on
  loopback; no GitHub Release exists.
- The archive unpacked with the system `tar`.

## Compatibility check (host)

- Matching app: `rnqnx runner check` compared 3 of 3 fields and exited 0
  (`check-match.log`). `rnqnx analyze` of `apps/rn-playground`
  with the runner's descriptor printed `Stock runner: sufficient`.
- Mismatch: a bundle whose header says bytecode version 98 made the check exit 1
  with both sides named (`check-mismatch.log`). That bundle
  is the real bundle with four header bytes edited; it is not a bundle from an
  older compiler.
- The runner itself rejects the same edited bundle on load: `Compiling JS
failed: Wrong bytecode version. Expected 99 but got 98`
  (`runner-run-stale.log`). This is Hermes's own check,
  seen on the host runner. The CLI check runs first and gives the message the
  developer reads.
- The runner rendered `apps/rn-playground` from a compiled `.hbc`
  (offscreen, 640x480, exit 0): [runner-host-hbc.png](runner-host-hbc.png),
  `runner-run-hbc.log`.

## Red-green proof (host)

`tests/js/qnx-runner-compat.test.mjs`, 61 checks, passes
(`green.log`). Each mutation below was applied to the source, the
test was run, and the source was restored (`green-restored.log`).

| Mutation                                         | Exit | First failing assertion                                     | Log |
| ------------------------------------------------ | ---- | ----------------------------------------------------------- | --- |
| `compareRunnerToBundle` never reports a mismatch | 1    | `React Native mismatch found`                               | log |
| `project run` skips `launchCheck`                | 1    | `Missing expected rejection`                                | log |
| The `.hbc` header is never read                  | 1    | `hermesc output header carries the pinned bytecode version` | log |
| A check that compared nothing is accepted        | 1    | `Missing expected rejection`                                | log |
| The Nitro ABI comparison is removed              | 1    | `Nitro ABI mismatch found`                                  | log |

## Wider suite (host)

`node scripts/run-js-tests.mjs` passed ("All JavaScript tests passed.") with
Watchman off the `PATH` and a `pnpm` shim on it. With Watchman on, the Metro
tests hang on `Waiting for Watchman watch-project` on this machine, so they were
not run that way. `tsc --noEmit`, `prettier --check .` and
`scripts/validate-docs.mjs` also passed.

## Not done

- Device launch: not run, no board. `project run` stops at `deploy` with
  `needs device` as before.
- The native runner does not read `rnqnx-runtime.json` or show the
  `preflight.*` screen (hardening item A4). That is native C++ and needs the
  Margelo `api-design`, `build-nitro-modules` and `cpp` steps with review before
  code; this slice adds no native API, so none of them ran. See the PR.
  Follow-up, 2026-11-21: [s6-preflight-2026-11-21](../s6-preflight-2026-11-21/README.md).
- No QNX runner archive, so the `targetLibraries` path of `pack` ran only
  against a synthetic stage that must be refused (unit test).
- No gate closes. `expo` and `developer-path` stay `pending`.

## Logs are not in the repository

The log files named above (`pack.log`, `fetch.log`, the `check-*.log` files,
`runner-run-*.log`, `green*.log` and the five `mutation*.log` files) were not
committed: `.gitignore` excludes `*.log`, and this folder holds only this
README and `runner-host-hbc.png`. The commands, exit codes and first failing
assertions in the tables above are the author's record of those runs. Nobody
has re-read the logs since, so treat those figures as unverified until the
runs are repeated and the logs committed with a `SHA256SUMS` file.
