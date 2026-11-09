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

## Configured application icons and the QNX launcher

New projects include `expo.icon: "./assets/icon.png"`, a 1024-pixel React Native
logo that belongs to the consuming app. Replace it with your own icon. QNX uses
`expo.name`, `expo.extra.rnqnx.appId`, and that configured image, never a generic
system icon. An optional QNX-specific PNG override is
`expo.extra.rnqnx.qnx.icon`. Local square PNGs from 256 to 2048 pixels are
accepted; remote URLs, symlinks and paths outside the project are refused.
Dynamic `app.config.js/ts` evaluation is not implemented by these static-config tools.

`project prepare-native` and successful QNX `project build-native` automatically
prepare `build/qnx/launcher/{launcher.json,icon.png}` when an icon is configured.
The native descriptor and compiled application identity stay separate from
launcher metadata. Older projects without an icon still build; registration
requires adding one. You can prepare the launcher assets without a native build:

```bash
pnpm qnx project launcher prepare --project /path/to/app
```

After deploying the **complete runnable native app and bundle**, register it:

```bash
export QNX_TARGET_IP=verified-device.example
export QNX_TARGET_USER=qnxuser
export QNX_SSH_KEY=/absolute/path/to/dedicated-key
pnpm qnx project launcher install --project /path/to/app \
  --app-dir /data/home/qnxuser/apps/org.example.notes
```

The executable filename defaults to the configured app ID. Use `--executable app`
only if your deployment renamed it. This command registers an already-deployed
application; it does **not** deploy native binaries, Hermes, Qt, plugins or
licensed SDK files. The generic `deploy-qnx` command above remains an executable-only
probe deployment, not a complete native consumer installer.

Optional process-local target runtime settings belong under
`expo.extra.rnqnx.qnx.runtime`: `libraryPaths` (an array of absolute directories),
`qtPluginDirectory`, and `qtFontDirectory`. The deployed application directory is
first in its library path. The generated launch script enforces non-root
execution and `QT_QPA_PLATFORM=qnx`; no global environment or system library is
changed. These settings do not prove a complete runtime dependency closure.

Registration uses a user-owned overlay at
`$HOME/.local/share/rnqnx/launcher`, copying the installed stock QNX demo
launcher's desktop entries/configuration and appending generated entries. Original
OS files, artwork, headers and launch actions are preserved. Each app ID owns
exactly one generated entry: rerunning install updates its name/icon/action,
while different IDs coexist. An unowned or colliding OS entry is never adopted.
The target must already provide the stock `demolauncher`, `on`, `waitfor`,
`pidin`, SHA-256 via `sha256sum` or `openssl dgst`, and a functioning OS window manager. Alternate launchers and
their layouts need their own integration; this is not an iOS/Android shell emulator.

By default, registration starts a fresh user-owned launcher on the overlay and
checks its process identity/stability before stopping only matching user-owned launcher
instances. Other apps, Screen, DRM, HID and the window manager are not stopped.
Use `--no-refresh` to change registration only. `--dry-run` reads local config
without writing files or contacting a target.

```bash
pnpm qnx project launcher remove --project /path/to/app
```

Removal unregisters only that app's generated launcher assets; native executables
and user data are preserved. It does not uninstall a native application or stop
a running app. Reinstall restores the entry. Process readiness is not proof that
the physical monitor displays the icon; Screen context metadata may be inaccessible
to a separate unprivileged client. On device failure, the existing
launcher is left running if the replacement was not ready; registration may
already be written, so correct the cause and rerun. Device commands are bounded
locally, but losing SSH is not proof that remote file operations were cancelled.
A stale operation lock must be reviewed, not blindly removed during another install.

This integration never starts privileged services or changes boot configuration.
For persistence across boots the image owner must configure the OS launcher to
use the user overlay; current-session refresh is not a cold-boot qualification.
Fast Refresh, native input behavior and physical icon visibility require their
own runtime evidence and are not implied by metadata generation.

## Recover from an error

- `already exists`: choose a different app name; existing files are preserved.
- Missing Metro or a workspace dependency: run the documented install from the
  repository root. Newly created apps need the second install above.
- `does not accept`: check `pnpm qnx --help`; unrelated options are rejected.
- Missing binary or configured build: run configure/build for the matching
  architecture and Debug/Release preset before deploying.
- QNX/Qt provisioning or graphics failure: follow the named board runbook and
  preserve recovery boot. The CLI does not rewrite boot configuration.
