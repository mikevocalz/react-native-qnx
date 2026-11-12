# Clean-host simulation, 2026-11-12, base `b51b49c`

**This is a simulation on a used Mac, not a machine that has never seen the
repository.** The Mac already had Homebrew, Node, CMake, Ninja, Qt 6.12.0 and the
QNX SDP 8.0.5 installation used for the Pi 400 work. Isolation was limited to:

- a fresh `git clone` of `origin/main` at `b51b49cd` into an empty directory;
- a new, empty `HOME` (so no pnpm store, npm cache, shell profile, SSH config
  or tool state carried over);
- `env -i` with `PATH=$TEMP_HOME/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin`,
  no `QNX_*` variables and no target variables.

Host: macOS 27 (Darwin 27.0.0) on arm64, Node 26.13.0, Homebrew pnpm 10.32.1,
CMake and Ninja from Homebrew, Qt 6.12.0 from Homebrew. No device was used.

The walk followed `README.md` "Start from source", then "Check your checkout",
then `docs/getting-started/qnx-cli.md` and `build-and-deploy.md`. Fixes are on
branch `v1/gate6-developer-path` (PR #123).

## Steps without the SDP

| #   | Step (as documented)                           | Result                  | Doc or tool defect                                                                                                                                                                                                                                                                                               |
| --- | ---------------------------------------------- | ----------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 1   | `git clone …; cd react-native-qnx`             | ok                      | none                                                                                                                                                                                                                                                                                                             |
| 2   | `pnpm install --frozen-lockfile`               | **failed** in 2 s       | D1. Homebrew pnpm 10.32.1 reads `packageManager: pnpm@12.8.1`, downloads 12.8.1 into `~/Library/pnpm/.tools` and stops with `Failed to switch pnpm to v12.8.1 … ENOEXEC`. The README says "use pnpm 12.8.1" but not how to install it.                                                                           |
| 2a  | (undocumented) obtain pnpm 12.8.1              | workaround              | D2. Node 26 has no `corepack` binary (`command not found`). See "Limits of this run" for how 12.8.1 was obtained here.                                                                                                                                                                                           |
| 2b  | `pnpm install --frozen-lockfile` (12.8.1)      | ok, 466 packages, 9.1 s | none                                                                                                                                                                                                                                                                                                             |
| 3   | `pnpm validate:dependencies`                   | ok                      | none                                                                                                                                                                                                                                                                                                             |
| 4   | `pnpm qnx --help`                              | ok                      | D3. Footer named `rnqnx project …` and `rnqnx marathon …`; `rnqnx` is not on `PATH` in a source checkout.                                                                                                                                                                                                        |
| 5   | `pnpm qnx init dashboard`                      | ok                      | none                                                                                                                                                                                                                                                                                                             |
| 6   | `pnpm install --no-frozen-lockfile`            | ok, 4 s                 | none                                                                                                                                                                                                                                                                                                             |
| 7   | `pnpm qnx bundle-qnx --app dashboard --out …`  | ok, 8 s                 | none                                                                                                                                                                                                                                                                                                             |
| 8   | `pnpm typecheck`                               | ok                      | none                                                                                                                                                                                                                                                                                                             |
| 9   | `pnpm test:js`                                 | **exit 1** after 21 s   | D4. The runner imports files in order and stops at the first failure, `qnx-consumer-native-config`. Run one file per process: 46 pass, 5 fail (`qnx-consumer-native-config`, `qnx-device-tools`, `qnx-external-project`, `qnx-native-consumer-build`, `qnx-project-create`). The README did not warn about this. |
| 10  | `pnpm validate:docs`                           | ok                      | none                                                                                                                                                                                                                                                                                                             |
| 11  | `pnpm validate:macos`                          | not run                 | Skipped: host disk was at 5–7 GiB free with a parallel Qt build running.                                                                                                                                                                                                                                         |
| 12  | Host render (`qnx-cli.md`, Hermes from source) | not run                 | Same reason. The guide does not state the disk or time cost of building Hermes; not measured here.                                                                                                                                                                                                               |
| 13  | `pnpm qnx doctor`                              | exit 1, 8 errors        | D5. One root cause (SDP not sourced) produced 8 errors (`QNX_HOST`, `QNX_TARGET`, two "directory does not exist: " with an empty path, `qcc`, `q++`, Screen headers, Screen library). No error named a command to run.                                                                                           |

## Steps with the SDP sourced

`source /Users/…/qnx-pi400-8.0.5/qnx800/qnxsdp-env.sh` on macOS sets
`QNX_HOST=…/host/win64/x86_64`. The SDP directory contains `host/linux` and
`host/win64` only.

| #   | Step                                                   | Result           | Defect                                                                                                                                                                                                                                                                              |
| --- | ------------------------------------------------------ | ---------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 14  | `pnpm qnx doctor`                                      | exit 1, 3 errors | D6. `qcc not found in PATH. Ensure QNX_HOST/usr/bin is on PATH.` is wrong advice on macOS: no runnable `qcc` exists for this OS. D7. `QNX Screen library missing` was a false negative: `target/qnx/aarch64le/usr/lib/libscreen.so` is a symlink and the check used `find -type f`. |
| 15  | `pnpm qnx configure-qnx --arch aarch64 --config Debug` | exit 1           | Expected: `Could not find CMAKE_C_COMPILER using the following names: qcc, qcc.exe`. The CLI printed no next step (D8). `build-and-deploy.md` does say macOS is a client for a Linux build host, but the README did not, so a first-timer only learns it here (D9).                 |
| 16  | `build-qnx`                                            | not reached      | Needs an x86-64 Linux host with the SDP. Not available in this session.                                                                                                                                                                                                             |

## Other documentation defects

- D10. `build-and-deploy.md` listed stale pins: Expo 58.0.0, React Native
  0.88.0-rc.3, Nitro/Nitrogen 0.37.1. The lockfile resolves Expo 58.0.22, React
  Native 0.88.1 and Nitro `1.0.0-alpha.nightly.105.g69d365fc9b18`.
- D11. `build-and-deploy.md` ran `./scripts/doctor.sh` while `qnx-cli.md` and
  the README use `pnpm qnx doctor`. Same script, two spellings.
- D12. CLI errors without a next step: `bundle-qnx --app nope` printed a raw
  `ENOENT … access '…/apps/nope/index.js'`; `pnpm qnx --dry-run` printed
  `Unknown command: undefined`; `pnpm qnx init` printed `Unexpected arguments for init`.

## Fixes on the branch

| Defect       | Fix                                                                                                                                                            |
| ------------ | -------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| D1, D2       | README: install pnpm 12.8.1 explicitly, with the exact switch error; doctor's pnpm fix line says Node 26 has no Corepack.                                      |
| D3           | Help footer lists `pnpm qnx project …` and `pnpm qnx marathon …`.                                                                                              |
| D4           | README lists the five macOS failures at `b51b49c` and the per-file command.                                                                                    |
| D5           | Doctor reports one SDP error and marks dependent checks `SKIPPED`; every `ERROR`/`WARNING` has a `Fix:` line; the summary ends with `Start with: <first fix>`. |
| D6           | Doctor distinguishes "no runnable `qcc` for this OS", "`qcc` exists in `${QNX_HOST}/usr/bin` but is not on PATH" and "`qcc` missing from a Linux SDP".         |
| D7           | `libscreen.so` symlinks count as present.                                                                                                                      |
| D8, D12      | CLI errors name the next command; failing configure/build/deploy/run/log children add one next-step line.                                                      |
| D9           | README states that QNX cross-builds need an x86-64 Linux host and why.                                                                                         |
| D10, D11     | Stale pins replaced with a link to the generated README table; `build-and-deploy.md` uses `pnpm qnx doctor` and describes the new output.                      |
| (new checks) | Qt 6.11.1+ host tools (warning); target "SKIPPED: no target configured" instead of two warnings; half-configured target warns for the missing variable.        |

After the fixes, the same clean environment printed for step 13:
`Environment has 1 error(s) and 2 warning(s).` followed by
`Start with: source <qnx-installation>/qnxsdp-env.sh in this shell, then rerun pnpm qnx doctor`,
and for step 14 two errors (`qcc`, `q++`) whose fix is to run configure/build on
x86-64 Ubuntu 22.04. The Screen library check passed.

## Limits of this run

- **pnpm install path not executed.** `npm install -g pnpm@12.8.1` and the
  official standalone installer were both blocked by this session's tool
  permissions. The run used the pnpm 12.8.1 native binary that pnpm 10's own
  `packageManager` switch had already downloaded into the temporary HOME before
  failing. The README command is pnpm's documented npm route and remains
  unverified on a clean machine.
- The used Mac already had CMake, Ninja, Python 3 and Qt. Their absence was
  covered by `tests/js/qnx-environment-doctor.test.mjs` (stub tools on a
  sandboxed `PATH`), not by uninstalling them.
- No QNX build, deploy or run happened. Steps 16 onward need an x86-64 Linux
  host with the licensed SDP and then the stock Pi 4 QSTI board.
- `validate:macos` and the host renderer were not run (disk).
