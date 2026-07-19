# CURRENT_STATE.md

## Milestone

Milestone 0: Compatibility and Toolchain

## Completed

- Repository structure created.
- Handoff documentation scaffolded and populated.
- `.project-state.json` created.
- Dependency compatibility matrix created in `docs/getting-started/compatibility-matrix.md`.
- Initial ADRs created (QNX Screen over Qt, Fabric-only, Skia renderer, Nitro Modules, Hermes-only, Expo dev builds, self-hosted build infra, version pinning).
- QNX CMake toolchain file created at `cmake/toolchains/qnx-8.0.cmake`.
- Host doctor command created at `scripts/doctor.sh`.
- Minimal native QNX Screen application created (`native/application/main.cpp`, `native/screen/QnxScreenSurface.{h,cpp}`).
- Build/deploy scripts created (`scripts/bootstrap-host.sh`, `scripts/verify-qnx-license.sh`, `scripts/configure-qnx.sh`, `scripts/build-qnx.sh`, `scripts/deploy-qnx.sh`, `scripts/run-qnx.sh`, `scripts/log-qnx.sh`).
- Build-and-deploy documentation created at `docs/getting-started/build-and-deploy.md`.
- Host doctor executed; non-QNX macOS host correctly reported as unsupported with actionable diagnostics.
- CMake configure attempted; toolchain correctly fails with exact missing `QNX_HOST`/`QNX_TARGET` message.

## Partially completed

- CMake configure logic validated for missing-environment path only.
- No actual QNX compilation has run because the host has no QNX SDP installed.

## Not started

- CI foundation.
- Native executable build on a real QNX target.
- Hermes cross-compilation investigation.
- React Native package manifests and Metro configuration.

## Active branch

`main`

## Latest known-good commit

None (git repository not yet initialized).

## Last successfully executed command

`bash scripts/doctor.sh` (completed with expected errors; see `artifacts/logs/2026-07-19-qnx-doctor.txt`).

## Last successful build target

None.

## Last failing command

`bash scripts/configure-qnx.sh aarch64 Debug`

## Current failure

```text
CMake Error at cmake/toolchains/qnx-8.0.cmake:28 (message):
  Environment variable QNX_HOST is not set.

  Install QNX SDP 8.0.5 and source the SDP environment script, e.g.:

    source /opt/qnx/800/host_linux/x86_64/bin/qnxsdp-env.sh

  Then re-run CMake.
```

This is expected on a non-QNX host.

## Applications currently running

None.

## Components currently rendering

None.

## Native modules currently working

None.

## Architectures tested

None.

## Repository buildable

No. The QNX toolchain is not installed on this host, so the native executable cannot be built. The toolchain file and scripts are in place and will produce a clear configure error until QNX SDP is installed.

## Notes

The next agent should install QNX SDP 8.0.5 on an x86-64 Ubuntu 24.04 host, source the environment, and re-run `./scripts/doctor.sh` followed by `./scripts/configure-qnx.sh aarch64 Debug` and `./scripts/build-qnx.sh aarch64 Debug`.
