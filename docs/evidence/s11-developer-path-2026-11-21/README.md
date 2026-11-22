# `developer-path` gate: host and QNX VM run, 2026-11-21

Base: `main` at `4a4c4818`. **The gate stays `pending`.** This receipt records what
host and QNX VM evidence can show and lists, item by item, what is still missing.
`docs/releases/v1-readiness.json` is unchanged.

Every result is labelled by where it ran.

| Where                        | Ran                                                                     | Not run                                    |
| ---------------------------- | ----------------------------------------------------------------------- | ------------------------------------------ |
| host (macOS 27 arm64)        | README quick start end to end; package pack check; npm registry lookups | any QNX build (no SDP on this host)        |
| QNX VM (QEMU, TCG, software) | bundle push and run on the already-deployed runner; three timed rounds  | a native build of the created app          |
| Pi 400                       | no, not touched, by instruction                                         |                                            |
| release board                | no board                                                                | create, build, deploy and run on the board |

The VM is not Pi 400 evidence and not release-board evidence, and TCG timings are
not performance evidence.

## Closure criteria, item by item

Sources: the gate in `docs/releases/v1-readiness.json` (acceptance text, empty
`evidence`), the gate definition in
`docs-site/content/docs/reference/release-gate-definitions.mdx`, `TODO.md` (gate 6,
order step 5, and the two carried-over items tagged `developer-path`), and rows
`developer-path`, H14 and H21 in `docs/v1/hardening-coverage.md`.

| #   | Criterion (source)                                                                                      | State on host / VM evidence                                                                                         | State for the gate    |
| --- | ------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------- | --------------------- |
| 1   | Create an app from documented prerequisites (acceptance)                                                | Met on host: `pnpm qnx init dashboard` exit 0, 1 s                                                                  | Met on host only      |
| 2   | Build it (acceptance)                                                                                   | Host render built, exit 0, 524 s. QNX native build of the created app: not run                                      | Open                  |
| 3   | Deploy it (acceptance)                                                                                  | VM: bytecode pushed by hand to the prebuilt runner. Not a documented command. Board: not run                        | Open                  |
| 4   | Run it (acceptance)                                                                                     | Host offscreen render exit 0. VM render exit 0 on the prebuilt runner. Board: not run                               | Open (board)          |
| 5   | "from documented prerequisites" (acceptance)                                                            | Host prerequisites followed as written. SDP, Linux build host and public-source Qt for QNX not exercised            | Open                  |
| 6   | "a new licensed developer", "run by someone who did not write the docs" (acceptance, TODO order step 5) | Not met. One agent ran this on a Mac that already had Qt, CMake and a warm pnpm store. No licensed first-time human | Open, needs a person  |
| 7   | Verify package contents before any npm publication (acceptance)                                         | Met at `4a4c4818`: fresh `npm pack --dry-run` on every package directory, 0 flagged paths                           | Met for contents      |
| 8   | Verify actual package ownership before any npm publication (acceptance)                                 | Not verifiable here: all names return 404 (nothing published) and `npm whoami` returns `ENEEDAUTH`                  | Open, needs the owner |
| 9   | Permanent npm organization replaces provisional `@rn-qnx` (TODO, carried over)                          | Not done. Owner decision. No publish was attempted                                                                  | Open, needs the owner |
| 10  | Rebuilt hello-world round trip below 30 seconds (TODO, carried over)                                    | JS-only on the VM: 29.0 s cold, 10.0 s and 9.7 s warm. A native rebuild was not timed                               | Partly met, VM only   |
| 11  | H14: versioned template tarballs, template selector, selected version and hash, clean-host receipt      | Not implemented: `init <name>` copies the in-repo template, no selector or version output                           | Open, needs code      |
| 12  | H21: Linux/amd64 licensed cross-build host, fresh bounded cross-build and docs receipt                  | Not done. The `qnx805-build` container exists, but no fresh cross-build was run for this gate                       | Open                  |
| 13  | Reconcile the Hello World prohibition before benchmark device use (hardening row)                       | Not examined                                                                                                        | Open                  |

Items 2 to 6, 8, 9 and 11 to 13 block `passed`. The gate also needs a deploy and
run on the release board, which no host or VM result can stand in for.

## 1 to 4: host quick start, `4a4c4818`

Host: macOS 27.0 arm64, Node 26.13.0, Homebrew Qt 6.12.0, CMake 4.4.5, Ninja
1.13.2. pnpm 12.21.0 came from the README prefix route (`npm install --prefix ... pnpm@12.21.0`,
then `node .../pnpm/install.js`). Source: a fresh `git clone` of `main`. The pnpm
store was the shared store under `/Volumes/QNX/cache`; the install log shows
`downloaded 0`, so the 187 s install is a warm-store time, not a cold-network one.

| Step                                                                  | Result                                                                             | Log                                                |
| --------------------------------------------------------------------- | ---------------------------------------------------------------------------------- | -------------------------------------------------- |
| `npm install --prefix ... pnpm@12.21.0`, then `install.js`            | exit 0, 27 s; `pnpm --version` prints `12.21.0`                                    | [host/01](host/01-pnpm-install-prefix.log), 02, 03 |
| `pnpm install --frozen-lockfile`                                      | exit 0, 187 s                                                                      | [host/04](host/04-install-frozen.log)              |
| `pnpm qnx init dashboard`                                             | exit 0, 1 s, `Created apps/dashboard.`                                             | [host/05](host/05-qnx-init.log)                    |
| `pnpm install --no-frozen-lockfile`                                   | exit 0, 10 s                                                                       | [host/06](host/06-install-nofrozen.log)            |
| `pnpm qnx bundle-qnx --app dashboard --out build/dashboard.bundle.js` | exit 0, 15 s                                                                       | [host/07](host/07-bundle.log)                      |
| `cmake -S . -B build/playground -G Ninja ... -DRNQNX_BUILD_HERMES=ON` | exit 0, 47 s                                                                       | [host/08](host/08-cmake-configure.log)             |
| `cmake --build build/playground --target rnqnx_playground_host`       | exit 0, 524 s, 857 steps                                                           | [host/09](host/09-cmake-build.log)                 |
| host renderer with `-platform offscreen --screenshot`                 | exit 0, `RNQNX_PLAYGROUND_SCREENSHOT build/dashboard.png 640x480`                  | [host/10](host/10-run-offscreen.log)               |
| `pnpm qnx doctor`                                                     | exit 1: the SDP environment is not loaded. It names the fix. Expected on this host | [host/11](host/11-doctor.log)                      |

The PNG ([host/dashboard-host.png](host/dashboard-host.png), 11958 bytes) shows
"dashboard on React Native QNX" on a dark background. Per-step wall times are in
[host/steps.txt](host/steps.txt). This repeats the
[2026-11-19 receipt](../quickstart-host-2026-11-19.md) on the current main, with
all steps timed. Both agree: 854 build steps then, 857 now.

## 3 to 4: QNX VM, prebuilt runner

Guest: `QNX qnxvm 8.0.0 2026/06/05-16:21:14EDT QEMU_virt aarch64le`, the same VM as
the [S5 lane receipt](../s5-vm-lane-2026-11-21/README.md). The native runner is
`/data/vm/app/org.rnqnx.showcase`, deployed on 2026-11-20 and not rebuilt here. Its
SHA-256 on the guest is `4f465406...f69cf`, equal to the line in
[the 2026-11-20 deploy record](../qnx-vm-2026-11-20/vm/deploy.sha256).

What ran, by hand, because no documented command does it:

1. The host's `hermes-compiler` `hermesc` turned `build/dashboard.bundle.js` into
   `build/dashboard.hbc` (bytecode version 99, 727373 bytes). `hermesc` printed
   warnings about `Promise` and exited 0 ([head of its output](vm/rt1-hermesc.head.txt)).
2. The bytecode went to `/data/dp/bundles/proof-dashboard.hbc` over ssh. SHA-256 on
   the host and on the guest, from `openssl dgst`, are both
   `97eff832aff2362512ec9be7bff1ebc0295db038d00c7acaeffdb59fd3f05545`
   ([guest-bundle.sha256](vm/guest-bundle.sha256)).
3. `capture.sh` (identical to `scripts/vm-proof/capture.sh`) started Screen and ran the
   runner with `--fullscreen --bundle ... --screenshot`. Exit 0, 1280x800 PNG,
   17913 bytes ([dashboard-vm.png](vm/dashboard-vm.png), [app log](vm/dashboard-app.log)).

The app registers `RNQnxPlayground`, the component name in the runner's
`native-app.json`, so the created app ran unchanged. The screenshot shows
"dashboard on React Native QNX".

### Timed round trip (item 10)

[roundtrip.sh](vm/roundtrip.sh) edits the app's text, then times bundle, `hermesc`,
push, run and PNG pull. Each round's PNG shows its own text
([rt3.png](vm/rt3.png) reads "dashboard on React Native QNX round 3").

| Round | bundle-qnx | hermesc | push  | run on VM | pull  | total      |
| ----- | ---------- | ------- | ----- | --------- | ----- | ---------- |
| 1     | 12.8 s     | 2.8 s   | 2.3 s | 10.7 s    | 0.3 s | **29.0 s** |
| 2     | 4.0 s      | 1.0 s   | 0.6 s | 4.1 s     | 0.3 s | **10.0 s** |
| 3     | 4.0 s      | 0.8 s   | 0.5 s | 4.0 s     | 0.3 s | **9.7 s**  |

All three are under 30 seconds, round 1 by 1.0 s. This measures a JavaScript change on
a prebuilt runner. The TODO item says "rebuilt"; a native rebuild was not timed, so the
item is partly met. Raw output: [roundtrip.txt](vm/roundtrip.txt).

### Why the created app cannot go further

`pnpm qnx init` makes a workspace app with no `app.json`. Both native steps refuse it:

```text
$ pnpm qnx project prepare-native --project apps/dashboard --platform qnx --dry-run
rnqnx: No Expo config found. Add app.json or app.config.js/ts to the project root.
$ pnpm qnx project build-native --project apps/dashboard --platform qnx --dry-run
rnqnx: ENOENT: no such file or directory, lstat '.../apps/dashboard/build/qnx/native-app.json'
```

`docs/getting-started/qnx-cli.md` says `bundle-qnx` produces JavaScript for the existing
runner, but neither it nor the README says that an `init` app has no route to a native
build, and the `deploy-qnx` row in the same page deploys only the Screen probe
(`rnqnx_playground`). `project run` lists its steps (`config`, `bundle`,
`native-descriptor`, `native-build`, `deploy`, `launch`) in `--dry-run` and, per the S6
receipt, stops at `deploy`. A new developer therefore has no documented command that
takes the app they just created onto a device. That is the main gap behind items 3, 4
and 6. It is recorded here, not fixed: the native route is `project create`, covered by
`docs/getting-started/expo-project-create.md`, and no run of it was made.

## 7 to 9: package contents and ownership

Contents ([npm/pack-dry-run.txt](npm/pack-dry-run.txt), made by
[pack-check.mjs](npm/pack-check.mjs)): `npm pack --dry-run --json --ignore-scripts` on every
directory under `packages/` that has a `package.json`. Eleven are public and one is
private (`@rn-qnx/react-native-qnx-cli`). Three directories (`expo-qnx`, `qnx-devtools`,
`react-native-qnx-codegen`) have no `package.json` and are not packages. No packed path
matches `.env`, `.pem`, `.key`, `id_rsa`, `id_ed25519`, `qnxsdp`, `.tar`, `.zip`, `.tgz`,
`.qnx`, `.core`, `node_modules`, `.DS_Store` or `.log`. File counts run from 4 to 76.
Three shasums differ from the [2026-11-21 record](../s10-distribution-2026-11-21/pack-dry-run.txt)
(`expo-qnx-autolinking`, `expo-qnx-config-plugins`, `react-native-qnx-cli`) because those
packages changed on main after it was made.

The pattern list is mine; it is a screen, not a proof that no SDK content is present.
A reviewer should read the file lists before publication.

Ownership ([npm/npm-view.txt](npm/npm-view.txt)): `npm view` returned 404 for five of
the packages checked (`react-native-qnx`, `react-native-qnx-cli`, `metro-config-qnx`,
`qnx-nitro-runtime`, `expo-router-qnx`), so none of them is published. Whether the
`@rn-qnx` scope exists, and who owns it, cannot be read without an npm login: `npm
whoami` fails with `ENEEDAUTH`, `https://www.npmjs.com/org/rn-qnx` returned 403 and
`https://registry.npmjs.org/-/org/rn-qnx/user` returned 404. The scope is provisional
per `TODO.md`, and the permanent organization is an owner decision. The CLI package is
`private: true`, so even a published set would not give a developer a `npx` route to it.
Nothing was published and no publish command was run.

## Not done

- Any QNX native build, deploy or run of a created app, on the VM or the board.
- `project create` without `--dry-run`, `project add-qnx`, `project prepare-native` and
  `project build-native` for a project with an Expo config.
- A fresh cross-build in `qnx805-build` (H21). The container was only inspected.
- The release-board run, the independent licensed developer, the owner's npm decision
  and the H14 template packaging.
- A cold-network install time.

## Verify

```sh
cd docs/evidence/s11-developer-path-2026-11-21 && shasum -a 256 -c SHA256SUMS
```
