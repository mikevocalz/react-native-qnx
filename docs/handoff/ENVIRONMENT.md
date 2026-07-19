# ENVIRONMENT.md

## Verified development environment

The current workspace was verified on 2026-07-19. It is **not** the supported build host; it is a macOS arm64 client used for development only.

### Current workspace

- **Host operating system:** macOS (Darwin)
- **Host architecture:** arm64
- **CMake:** 4.3.3 (newer than required 3.28)
- **Node.js:** v26.0.0 (newer than required 24.x)
- **pnpm:** 10.28.0 (newer than required 9.x)
- **Python:** 3.13.3

### Missing on current workspace

- QNX SDP 8.0.5 (not installed)
- `QNX_HOST` environment variable
- `QNX_TARGET` environment variable
- `qcc` / `q++` toolchain
- QNX Screen headers and libraries
- Ninja build tool

### Expected build host environment

- **Host operating system:** x86-64 Ubuntu 24.04 LTS
- **Host architecture:** x86-64
- **QNX SDP version:** 8.0.5
- **QNX Software Center version:** 2.0.4 or newer
- **QNX compiler:** `qcc` / `q++` from the installed SDP
- **CMake:** 3.28 or newer
- **Ninja:** 1.11 or newer
- **Node.js:** 24.x LTS
- **pnpm:** 9.x or newer
- **Target CPU architectures:** x86-64, AArch64

### Required environment variables

- `QNX_HOST` – path to the host toolchain directory
- `QNX_TARGET` – path to the target sysroot
- `QNX_TARGET_IP` – target device IP or hostname (for deploy/run scripts)
- `QNX_TARGET_USER` – target SSH user (defaults to `root` if unset)

### Notes

- The primary build host is **not** macOS; macOS can be used only for remote development (SSH/VS Code Remote).
- Do not write QNX license content, keys, or credentials into this file.
- The current workspace cannot build QNX binaries; the toolchain file intentionally fails with a clear message until QNX SDP is installed.
