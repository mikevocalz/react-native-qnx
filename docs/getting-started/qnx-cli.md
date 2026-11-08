# Create and bundle a QNX app

The CLI runs from a React Native QNX source checkout with Node 26+ and pnpm
12.8.1. `init` creates a workspace app. `bundle-qnx` produces JavaScript for
the existing native runner. Native Qt/Hermes builds, licenses and runtime
library deployment remain separate prerequisites.

## Create an app

From the repository root:

```bash
pnpm install --frozen-lockfile
pnpm qnx init dashboard
pnpm install --no-frozen-lockfile
pnpm qnx bundle-qnx --app dashboard --out build/dashboard.bundle.js
```

`init` writes `apps/dashboard` with exact React/RN versions from the root
manifest, the workspace Metro resolver and the existing runner's surface name,
`RNQnxPlayground`. Choose a lowercase app name with letters, digits or hyphens.
It refuses an existing destination and does not run install scripts or change
the lockfile. The second install adds the new workspace importer and links.
Review and commit the app and lockfile together.

The template has no component state. Use Zustand when adding state. Leave
native handles in refs. Bundle output must be under the checkout's `build/`
directory; a successful bundle does not establish device compatibility.

## Render your bundle on the host

Install host Qt ≥6.11.1 and the prerequisites in [the toolchain guide](../toolchain.md).
The existing host renderer builds Hermes from source:

```bash
cmake -S . -B build/playground -G Ninja -DCMAKE_BUILD_TYPE=Debug -DRNQNX_BUILD_HERMES=ON
cmake --build build/playground --target rnqnx_playground_host
./build/playground/bin/rnqnx_playground_host --bundle build/dashboard.bundle.js
```

For an offscreen capture, append `-platform offscreen --screenshot build/dashboard.png`.
The runner's existing default window dimensions remain unchanged. This host
run is separate from QNX execution.

## Use the QNX scripts

These commands retain the existing shell scripts and their environment contract:

| Command                                                | Existing script            | Scope                                 |
| ------------------------------------------------------ | -------------------------- | ------------------------------------- |
| `pnpm qnx doctor`                                      | `scripts/doctor.sh`        | Local QNX toolchain/dependency checks |
| `pnpm qnx configure-qnx --arch aarch64 --config Debug` | `scripts/configure-qnx.sh` | Configure the existing preset         |
| `pnpm qnx build-qnx --arch aarch64 --config Debug`     | `scripts/build-qnx.sh`     | Build the configured preset           |
| `pnpm qnx deploy-qnx --target rnqnx_playground`        | `scripts/deploy-qnx.sh`    | Atomically deploy a built executable  |
| `pnpm qnx run-qnx --target rnqnx_playground`           | `scripts/run-qnx.sh`       | Run a deployed executable             |
| `pnpm qnx log-qnx`                                     | `scripts/log-qnx.sh`       | Stream slog2 output                   |

**`rnqnx_playground` is the Screen probe.** The full React Native executable is
`rnqnx_playground_qnx`, with the separate build/dependency closure described in
[Pi 400 runtime bring-up](pi400-hello-world.md). The generic scripts deploy a
single executable; they do not provision Qt, copy its plugins, deploy a bundle,
or configure graphics startup. Use that runbook for the full runtime.

Source your licensed SDP environment before QNX builds. The scripts retain
`QNX_TARGET_IP`, `QNX_TARGET_USER`, `QNX_TARGET_DIR` and `QNX_SSH_KEY` configuration
from [the build/deploy guide](build-and-deploy.md). No SSH root requirement is
introduced. App creation and bundling do not connect to a target.

Add `--dry-run` to an executable command to inspect the exact executable,
argument array and working directory without executing it:

```bash
pnpm qnx deploy-qnx --arch aarch64 --config Release --target rnqnx_playground --dry-run
```

A child process failure stays a nonzero CLI result. The CLI forwards SIGINT and
SIGTERM to its direct child; the existing script owns any remote process.
`log-qnx` also reports SSH failures instead of returning a false success.

## Verify a captured Pi Screen probe

After collecting evidence with the Pi installation workflow, verify its files and write a SHA-256 manifest:

```bash
pnpm qnx evidence verify
```

By default, this reads the lexicographically last directory under `artifacts/pi/`.
Select a specific capture to avoid mixing results from multiple boards:

```bash
pnpm qnx evidence verify --dir artifacts/pi/2026-11-07-pi400
```

The command verifies the existing Screen-probe logs and preflight files, then writes
`manifest.json` **inside the selected evidence directory**. It rejects missing
files, invalid target/preflight markers and inconsistent run counts. Use
`--dry-run` to inspect the exact command without reading or modifying the
capture. Run it only on a directory you intend to update.

**Scope:** this is the existing `rnqnx_playground` Screen-probe verifier, not
a test of the React Native/Fabric runtime, physical keyboard/touch, twenty
cold boots or release readiness. Keep Pi 400 and stock Pi 4 QSTI receipts
separate, and review the [v1 gates](../releases/v1-readiness.md) before
promoting a result.

## Recover from an error

- `already exists`: choose a different app name; existing files are preserved.
- Missing Metro or a workspace dependency: run the documented install from the
  repository root. Newly created apps need the second install above.
- `does not accept`: check `pnpm qnx --help`; unrelated options are rejected.
- Missing binary or configured build: run configure/build for the matching
  architecture and Debug/Release preset before deploying.
- QNX/Qt provisioning or graphics failure: follow the named board runbook and
  preserve recovery boot. The CLI does not rewrite boot configuration.
