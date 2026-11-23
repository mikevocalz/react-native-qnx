# Clean-host checklist for a first-time licensed developer

Use this on a machine that has never built React Native QNX, by someone who did not
write these docs. It lists what to have, what to run, and what to record. It is a
checklist for the `developer-path` gate. Filling it in does not pass the gate.

Mark each result with where it ran: **host**, **QNX VM**, **Pi 400** or **release
board**. Do not copy a result from one place to another. A VM or TCG run is not
board evidence.

## 1. Licenses and accounts (before the session)

- [ ] A QNX Software Development Platform 8.0 license for your own account, accepted
      under your own name. Do not reuse someone else's license or a shared download.
- [ ] A QNX SDP install on a Linux host (cross-build) or access to a Linux build host
      that has one. A macOS laptop alone cannot build the QNX native runner.
- [ ] Qt for QNX built or installed from the public sources under the Qt license
      terms ([toolchain guide](../toolchain.md)).
- [ ] A GitHub account that can clone the repository.
- [ ] Write down: host OS and version, CPU architecture, free disk, and whether any
      tool below was already installed. A "clean" host that already had Qt or a warm
      pnpm store must say so.

## 2. Host prerequisites

- [ ] Node 26 or newer: `node --version`
- [ ] pnpm 12.21.0: `pnpm --version`. If it prints another version, follow
      [the pnpm install note](../../README.md#quick-start-host-preview).
- [ ] CMake and Ninja: `cmake --version`, `ninja --version`
- [ ] Qt 6.11.1 or newer for the host renderer ([toolchain guide](../toolchain.md))
- [ ] `ssh` and `scp`, plus `openssl` for the bundle hash
- [ ] Several GB of free disk under the checkout; the first Hermes build compiles Hermes, folly and Boost

## 3. Create (host)

```bash
git clone https://github.com/mikevocalz/react-native-qnx.git
cd react-native-qnx
pnpm install --frozen-lockfile
pnpm qnx templates pack --out build/templates
pnpm qnx init dashboard --template qnx
pnpm install --no-frozen-lockfile
```

- [ ] `templates pack` prints a version and `contentSha256` for each template.
- [ ] `init` prints `Created apps/dashboard.` and the template version and hash.
- [ ] Record the commit you cloned (`git rev-parse HEAD`) and the three outputs above.

## 4. Build (host)

```bash
pnpm qnx bundle-qnx --app dashboard --out build/dashboard.bundle.js
cmake -S . -B build/playground -G Ninja -DCMAKE_BUILD_TYPE=Debug -DRNQNX_BUILD_HERMES=ON
cmake --build build/playground --target rnqnx_playground_host
./build/playground/bin/rnqnx_playground_host --bundle build/dashboard.bundle.js \
  -platform offscreen --screenshot build/dashboard.png
```

- [ ] The host renderer writes `build/dashboard.png` and exits 0.
- [ ] Record each step's wall time and exit code. Note where the instructions were
      unclear or wrong. That note is the main result of this run.

## 5. Native build (QNX build host)

- [ ] Source your SDP environment, then `pnpm qnx doctor`. Every failed check prints a
      `Fix:` line. Record which checks failed on a fresh machine.
- [ ] Build the runner: `pnpm qnx configure-qnx` and `pnpm qnx build-qnx` for
      `rnqnx_playground_qnx`, following the [Pi 400 runbook](pi400-hello-world.md).
- [ ] Record the runner's SHA-256 and the SDP and Qt versions used.

## 6. Deploy and run (QNX VM, then the board)

```bash
export QNX_TARGET_IP=<target-address> QNX_TARGET_USER=<user>
pnpm qnx deploy-bundle --app dashboard --run --runner <runner-name>
```

- [ ] `deploy-bundle` prints the bytecode SHA-256 and `Target sha256 matches.`
- [ ] The app appears on the target display. Save a screenshot and the app log.
- [ ] Repeat on the release board. A VM result does not count for the board.

## 7. What to hand back

- [ ] This checklist with every box marked done, skipped or failed, and a label for
      where it ran.
- [ ] The logs and timings from sections 3 to 6, with SHA-256 for each artifact.
- [ ] The names of the people involved. The gate needs a licensed developer who did
      not write the docs, and that person's name belongs in the record.
- [ ] Anything you had to guess. Each guess is a documentation defect.

Known gaps at the time of writing, none of them closed by this checklist: a created
`init` app has no route to a native build of its own, the permanent npm
organization is undecided, and H21 (a fresh Linux/amd64 licensed cross-build with a
docs receipt) has not been run.
