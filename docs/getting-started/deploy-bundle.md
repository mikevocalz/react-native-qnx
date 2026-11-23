# Deploy a bundle to a QNX target

`pnpm qnx deploy-bundle` compiles an app's JavaScript bundle to Hermes bytecode and
pushes it to a QNX target over ssh. It replaces the manual `hermesc` and `scp`
steps recorded in the
[2026-11-21 developer-path receipt](../evidence/s11-developer-path-2026-11-21/README.md).

**Where this has run.** The command is exercised on the host by
`tests/js/qnx-templates.test.mjs`, with `hermesc`, `ssh` and `scp` stubbed to run
locally. It has not run against a QNX VM, a Pi 400 or the release board. Until it
does, treat it as host-tested only.

## Before you start

- A bundle from `pnpm qnx bundle-qnx --app <name>` in `build/<name>.bundle.js`.
  [Create and bundle a QNX app](qnx-cli.md) shows how.
- The runner executable and its Qt/Hermes closure already on the target. This command
  deploys the app's bytecode only. It does not build or install the runner. The
  [Pi 400 runbook](pi400-hello-world.md) covers the runner.
- ssh access to the target with the same variables the other scripts use:
  `QNX_TARGET_IP`, `QNX_TARGET_USER`, `QNX_TARGET_DIR`, `QNX_SSH_KEY`
  ([build and deploy](build-and-deploy.md)). Root is not required.
- `hermes-compiler` from `pnpm install --frozen-lockfile`. The command picks the
  `hermesc` for your host (macOS or Linux). Set `RNQNX_HOST_HERMESC` or pass
  `--hermesc <path>` to use another one. Its bytecode version must match the Hermes
  built into the runner.

## Run it

```bash
pnpm qnx bundle-qnx --app dashboard
pnpm qnx deploy-bundle --app dashboard
```

The second command:

1. runs `hermesc -O -emit-binary -out build/dashboard.hbc build/dashboard.bundle.js`
   and prints the size and SHA-256 of the bytecode;
2. copies it to `${QNX_TARGET_DIR}/bundles/dashboard.hbc` through a temporary name on
   the target;
3. compares the SHA-256 on the target with the local one and refuses to install on a
   mismatch. A target without `sha256sum`, `sha256` or `openssl` falls back to a size
   comparison and prints a warning;
4. renames the temporary file over the final one.

Add `--run` to start the runner with the new bundle afterwards. `--runner <name>`
names the executable in `QNX_TARGET_DIR` (default `rnqnx_playground_qnx`):

```bash
pnpm qnx deploy-bundle --app dashboard --run --runner org.rnqnx.showcase
```

This runs `./<runner> --bundle ${QNX_TARGET_DIR}/bundles/dashboard.hbc` over ssh.
A runner that needs a display, Screen or environment setup first still needs that
setup on the target. The deploy command does not start Screen or set Qt variables.

`--remote-dir <path>` (or `QNX_BUNDLE_DIR`) changes the bundle directory. `--dry-run`
prints the `hermesc` and push steps without running either.

## What this does not do

- It does not build the native runner, deploy Qt, or change boot configuration.
- It does not make an `init` app a native app. See "Why the created app cannot go
  further" in the developer-path receipt. The app must register the surface name the
  runner expects (`RNQnxPlayground` for the stock playground runner).
- It does not prove the bytecode loads: that needs the runner on a QNX target.

## Recover from an error

- `hermes-compiler is not installed`: run `pnpm install --frozen-lockfile`.
- `... not found. Run pnpm qnx bundle-qnx --app <name> first`: bundle first.
- `hermesc exited with code ...`: read the compiler error above it.
- `target hash ... differs from local`: the transfer was corrupted. The temporary
  file is removed and the previous bundle stays in place. Run the command again.
- `deploy-bundle exited with code ...`: check that `QNX_TARGET_IP` and
  `QNX_TARGET_USER` reach the target over ssh and that `QNX_BUNDLE_DIR` is writable.
