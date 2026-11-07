# Test the integrated developer workflow

The integration candidate combines PRs 74-79 without publishing packages or
changing the release gates. It is a source checkout, not a standalone SDK.
The exact input revisions are recorded in
[the integration record](../releases/dx-integration.json). Original PRs remain
separately reviewable. A passing result on an input PR is not a passing result
on this combined candidate.

## Create and prepare an app

Use Node 26+ and pnpm 12.8.1. From the repository root:

```bash
pnpm install --frozen-lockfile
pnpm qnx init dashboard
pnpm install --no-frozen-lockfile
pnpm qnx bundle-qnx --app dashboard --out build/dashboard.bundle.js
pnpm expo:qnx inspect --project apps/dashboard
pnpm expo:qnx prepare --project apps/dashboard
pnpm expo:qnx prepare --project apps/dashboard --check
```

The second install links the new workspace app. Review its manifest and lockfile
change together. Creating an existing app is refused; preparation does not
replace modified output. Expo discovery and generated CMake files do not prove
native module compilation, registration, Router or device compatibility.

Read [the CLI guide](qnx-cli.md),
[Expo preparation](../../packages/expo-qnx/README.md),
[device diagnostics](../../packages/qnx-devtools/README.md) and
[evidence qualification](release-evidence.md) for their separate prerequisites.
Device commands require an already provisioned target; the following test does
not connect to one.

## Exercise a new source checkout

On Linux with GNU timeout, Git, tar and the pinned Node/pnpm tools:

```bash
pnpm validate:clean-app
```

Commit tracked changes first. The script archives exactly HEAD into a new
temporary directory that has no node_modules, installs the frozen dependency
graph, creates an app, links its importer offline, validates the pins, bundles
it, runs real Expo preparation and check mode, and exercises the device and
evidence commands in dry-run mode only. It also verifies overwrite refusal and
that the unqualified v1 release command still fails for the expected reason.

The source checkout and its lockfile are unchanged. Temporary source and
node_modules are removed on handled exits. Logs, commit, generated manifest,
bundle, hashes and exit status remain in a new build/clean-app-evidence.*
directory. Package installation can access the registry; the offline second
install reuses the populated store. No device, SSH or boot operation occurs.

## Also render the generated app

After building the existing Linux host renderer:

```bash
pnpm validate:clean-app --renderer "$PWD/build/linux-host-renderer/bin/rnqnx_playground_host"
```

This executes the generated bundle through the existing Fabric/Hermes/Qt Quick
host, captures a PNG and preserves the renderer log and binary hash. Both
input-ON and input-OFF CI configurations run this step, along with the existing
conformance and performance workload captures. Every subprocess has a timeout;
a failed install, bundle, preparation or capture fails the job.

This is a fresh **source-directory** workflow on a provisioned Linux host. It
reuses installed host tools, the package store and, when selected, a prebuilt
host renderer. It is not a clean-machine native build, a licensed QNX cross-build,
physical deployment, presentation benchmark or release approval. Host rendering
cannot qualify physical keyboard/touch, cold boots or stock QSTI. Keep all of
those results separate under [the v1 gates](../releases/v1-readiness.md).
