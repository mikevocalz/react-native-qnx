# Build and Deploy on an Ubuntu 24.04 QNX Host

This document describes how to set up an x86-64 Ubuntu 24.04 workstation for `react-native-qnx` development, build the Milestone 0 native executable, and deploy it to a QNX target.

## Host requirements

- x86-64 Ubuntu 24.04 LTS
- QNX SDP 8.0.5 installed through QNX Software Center 2.0.4 or newer
- QNX license activated
- Network access to the QNX target device

## Install freely redistributable host tools

```bash
cd react-native-qnx
./scripts/bootstrap-host.sh
```

This installs CMake, Ninja, Node.js 24 LTS, pnpm, Python, and other build dependencies. It does **not** install the QNX SDK, which must be licensed and installed separately.

## Install and license QNX SDP 8.0.5

1. Download and install QNX Software Center.
2. Install QNX SDP 8.0.5 and the target BSP packages for your board.
3. Activate your license.
4. Source the SDP environment script every session:

```bash
source /opt/qnx/800/host_linux/x86_64/bin/qnxsdp-env.sh
```

Adjust the path to match your installation location.

## Verify the QNX environment

```bash
./scripts/verify-qnx-license.sh
./scripts/doctor.sh
```

The doctor checks:

- Host OS and architecture
- `QNX_HOST` and `QNX_TARGET`
- `qcc` / `q++` availability
- QNX Screen and EGL headers/libraries
- CMake, Ninja, Node.js, pnpm, Python versions
- Project structure

Resolve every reported error before continuing.

## Configure the build

For an AArch64 QNX target in Debug configuration:

```bash
./scripts/configure-qnx.sh aarch64 Debug
```

For x86-64 QNX target:

```bash
./scripts/configure-qnx.sh x86_64 Debug
```

This creates a build directory such as `build/qnx-aarch64-debug` and runs CMake with the QNX toolchain file.

## Build

```bash
./scripts/build-qnx.sh aarch64 Debug
```

The native executable is written to `build/qnx-aarch64-debug/bin/rnqnx_playground`.

## Deploy to the QNX target

Set the target device IP and optional user:

```bash
export QNX_TARGET_IP=192.168.1.100
export QNX_TARGET_USER=root
```

Then deploy:

```bash
./scripts/deploy-qnx.sh aarch64 rnqnx_playground
```

## Run on the QNX target

```bash
./scripts/run-qnx.sh aarch64 rnqnx_playground
```

You should see a window open on the target display and alternate between blue and black frames.

## View target logs

```bash
./scripts/log-qnx.sh
```

## Remote development from macOS

The primary build host must run Ubuntu with the QNX toolchain. macOS can be used as a development client:

1. Install VS Code with the Remote - SSH extension.
2. SSH to the Ubuntu build host.
3. Open the `react-native-qnx` directory in VS Code Remote.
4. Run the doctor, configure, and build commands in the remote terminal.

Do not attempt to run the QNX compiler or CMake toolchain directly on macOS; QNX SDP 8 host tools are not supported there.

## Troubleshooting

### QNX_HOST or QNX_TARGET not set

Source the SDP environment script:

```bash
source /opt/qnx/800/host_linux/x86_64/bin/qnxsdp-env.sh
```

### qcc not found

Ensure `${QNX_HOST}/usr/bin` is on `PATH`. The SDP environment script normally does this.

### QNX Screen headers missing

Install the graphics BSP package for your target through QNX Software Center. The package usually contains the Screen headers and libraries.

### CMake cannot find the QNX toolchain

Make sure you invoked `configure-qnx.sh` rather than running `cmake` directly, or pass the toolchain file explicitly:

```bash
cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/qnx-8.0.cmake -S . -B build/qnx-aarch64-debug
```

## Next steps

After the native executable runs on QNX, proceed to Milestone 1 (Hermes and React runtime) by following `docs/handoff/NEXT_ACTIONS.md`.
