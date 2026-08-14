# ENVIRONMENT.md

## Verified authoring environment

Verified on 2026-08-13:

- Operating system: macOS 26.6.1 (`Darwin 25.6.0`)
- Architecture: arm64
- Apple Clang: 21.0.0
- CMake: 4.4.2
- Ninja: 1.13.2
- Node.js: 26.7.0
- Repository pnpm: 11.21.0, selected by `packageManager`
- Global pnpm launcher: 10.32.1; it delegates this workspace to pnpm 11.21.0
- npm: 11.19.0
- Python: 3.13.3
- Git: 2.55.0

The repository requires Node.js 24.3.0 or newer, pnpm 11.21.0 exactly, CMake 3.28
or newer, and Ninja 1.11 or newer. `./scripts/bootstrap-macos.sh` checks and
installs the freely redistributable workspace prerequisites.

## Missing QNX prerequisites

- Licensed QNX SDP 8.0.2 installation
- `QNX_HOST` and `QNX_TARGET`
- `qcc` and `q++`
- Installed QNX `screen/screen.h` and `libscreen`
- EGL/GLES development files from the target BSP
- x86-64 Ubuntu 24.04 QNX build host
- QNX target, Screen service, display, and deployment address

The current machine can run host validation only. It cannot establish QNX
header, ABI, compiler, linker, BSP, driver, compositor, or runtime compatibility.

## Expected licensed build-host environment

- x86-64 Ubuntu 24.04 LTS
- QNX SDP 8.0.2 installed through the licensed QNX channel
- Target BSP/graphics development packages
- `QNX_HOST` and `QNX_TARGET` exported by the official environment script
- `QNX_TARGET_IP` for deploy/run; `QNX_TARGET_USER` is optional and defaults to
  `root`

No QNX header, library, license material, or credential may be copied into this
repository.

## Application/tooling availability

Milestone 0 has no runnable React Native app, Metro server, iOS simulator,
Android emulator, or QNX device workflow. The only runnable native host artifact
is the isolated C++ test executable. Device UI validation begins only after a
real platform runtime/app exists.
