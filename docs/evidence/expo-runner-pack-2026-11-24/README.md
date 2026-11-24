# Expo gate: runner pack, dry-run publish and clean-directory create (2026-11-24)

Scope: what the `expo` gate can show on one Mac with no board and no SDP. Nothing was
published. No GitHub release, tag or npm package was created and the Pi 400 was not touched.

| Where                 | Ran | Not run                                  |
| --------------------- | --- | ---------------------------------------- |
| host (macOS arm64)    | yes | -                                        |
| QNX VM, Pi 400, board | no  | out of scope for this lane               |
| GitHub Actions run    | no  | the attestation workflow needs a release |

Host: macOS 27.0 arm64, Node 26.13.0, Expo CLI 58.1.21, `hermesc` from `hermes-compiler`
260318099.0.5 (SHA-256 `692d632c...950e`). Source commit: `b9d4071c` (main, includes #231).

## 1. `rnqnx runner pack`, twice

The stage is the real QNX aarch64 output of an earlier build on this machine
(`~/qnx-pi400-8.0.5/pr149b-out`, built 2026-11-19): `org.rnqnx.showcase`, `libhermesvm.so`,
`libQt6Svg.so.6.12.0` and `plugins/imageformats/libqsvg.so`. `libhermesvm.so` has the SHA-256
`a48934c6...f62f`, the same value as the `expodev` runner's library on the board
(`docs/evidence/pi400-v1-gates-2026-11-24/host/first-link-sha256.txt`).

This stage is **not** the board's `expodev` development runner. The `org.rnqnx.routerexample`
binary with SHA-256 `03604588...87d4` exists only on the board and the VM, so it could not be
packed here. The stage executable is the component-gallery showcase, and the stage holds no Qt
libraries other than Svg, so the archive would not start a board by itself. It proves the packing
path on real QNX binaries, not a usable runner.

Both runs used `--published-at 2026-11-24T00:00:00Z`, id `pi400-showcase-stage-0.1.0`,
target `qnx-aarch64-pi400`, SDP `8.0.5`, Qt `6.12.0`, Hermes `hermes-v260318099.0.5`
(`pack1.log`, `pack2.log`).

| File                                                | SHA-256                                                            | Both runs |
| --------------------------------------------------- | ------------------------------------------------------------------ | --------- |
| `pi400-showcase-stage-0.1.0.tar.gz` (7563925 bytes) | `e48619e15818869268c3484d242df8f2ee191314cd92e6c6c49c60ac86c57d22` | identical |
| `rnqnx-runner-manifest.json`                        | `82262a7b221a421b20bb4499de1be3b282ccc14463c64d60a28f5dc8b6457a7f` | identical |
| `rnqnx-runtime.json`                                | `f1c726b42f618e3e7242d84baf50dee3f45506a8f818132210f1a8b0099f8fcc` | identical |

`diff -r out1 out2` reported no differences (`reproducibility.txt`). The archive is not committed;
the manifest and descriptor are, next to this file. The measured bytecode version is 99. The
archive holds five entries and no SDP library: `libc`, `libm`, `libscreen` and the other eight
target libraries are listed in `targetLibraries` and left out of the stage.

## 2. `publish-runner-release.mjs` dry run

`node scripts/publish-runner-release.mjs --dir <out1> --tag runner-pi400-showcase-stage-0.1.0`
exited 0 and printed the three asset hashes above and the `gh release create ... --draft`
command it would run (`dryrun.log`). It created nothing. `gh release list` for the repository
returned no releases before and after.

## 3. Create and run on a clean directory

All of this ran against the archive from step 1, served from `127.0.0.1`.

- `rnqnx project create --app-id org.example.notes --name Notes` into an empty directory
  succeeded (`create.log`, template `qnx-router` 1.0.0).
- `pnpm install` of that project succeeded with `--ignore-scripts` and a fresh store.
- A change made only in `app.json` (`expo.name` to `Notes Renamed`) was read back by
  `expo config --type public --json` (`expo-config.log`). The icon came from the same file.
- `rnqnx runner fetch` pulled the archive over HTTP, verified size and SHA-256 and cached it
  (`fetch.log`). `tar tzf` of the cached copy lists the five entries.
- `rnqnx project start` served `GET /index.bundle?platform=qnx&dev=true` with HTTP 200 and
  5905965 bytes (`project-start.log`).
- `pnpm run bundle:qnx` wrote `build/qnx/app.bundle.js` (SHA-256 `19e458eb...ccdaf`).
  `hermesc` compiled it to a 2199620-byte `.hbc` (SHA-256 `5332bbbf...2262`).
  `rnqnx runner check` against the descriptor returned `ok: true`, 2 fields compared, with the
  Nitro ABI reported as not checked because the app has no Nitro dependency (`check.log`).
- `rnqnx project dev --dry-run` printed the Metro settings and the `ssh` command it would run
  (`dev-dryrun.log`). It did not connect to anything.

### Expo CLI does not serve a QNX bundle

`expo start --offline` starts Metro, but a request for `platform=qnx` fails with HTTP 500:
`No support package is known for platform "qnx"`, thrown by `expo-modules-autolinking` 58.1.0 in
`build/platforms/index.js` (`expo-cli-qnx-bundle-error.txt`, `expo-start-bare-qnx.log`). The
repository's own `rnqnx project start` serves the same project. So on this clean project the
gate item "Expo CLI owning Metro, logs and reload" is not met for QNX: the QNX platform runs
under `rnqnx`, not under `expo start`. The cause was not investigated further here.

## What the gate still lacks

1. **A runner for the board that someone can download.** No release exists. The `expodev` runner
   binary lives on the board and the VM only. Creating the draft is the owner's decision
   (`RNQNX_RELEASE_OWNER_APPROVED=<tag>` plus `--confirm`).
2. **The attestation run.** `.github/workflows/runner-release.yml` is on `main` and has never
   run, because it needs a draft release that holds the archive and manifest. No attestation
   has been verified with `gh attestation verify`.
3. **Create and run through Expo CLI on a clean host with the downloaded runner.** The run in
   step 3 used `rnqnx` and a local HTTP server, and it stopped before any device. `expo start`
   cannot bundle for `qnx` (above).
4. **A custom development build and a standalone package on a device**, with the SDP.
5. **The end-to-end recording** of the acceptance sentence in `v1-readiness.json`.
6. **A prebuilt runner for macOS and Linux hosts.** The only host binary on this Mac is an
   AddressSanitizer test build linked to `libclang_rt.asan_osx_dynamic.dylib`. A clean Release
   host build needs `pnpm install` at the repository root and a Hermes build, and the disk had
   about 8 GB free, so it was not attempted.

The gate stays `pending`.
