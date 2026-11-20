# Quick start host render, 2026-11-19

What ran: the host-render commands from the README quick start and
`docs/getting-started/qnx-cli.md`, on a fresh clone at `8d7deaa` plus the
braces-security fix. No board, no container.

Host: macOS 27.0 arm64, Homebrew Qt 6.12.0, CMake 4.4.5, Ninja 1.13.2,
AppleClang 21, Node 26, pnpm 12.18.0 through `npx`.

| Command                                                                                                                                | Result                                                                             |
| -------------------------------------------------------------------------------------------------------------------------------------- | ---------------------------------------------------------------------------------- |
| `pnpm qnx init dashboard`                                                                                                              | exit 0                                                                             |
| `pnpm qnx bundle-qnx --app dashboard --out build/dashboard.bundle.js`                                                                  | `Done writing bundle output`                                                       |
| `cmake -S . -B build/playground -G Ninja -DCMAKE_BUILD_TYPE=Debug -DRNQNX_BUILD_HERMES=ON`                                             | `Qt 6.12.0 found; building Qt Quick host targets`, `Build files have been written` |
| `cmake --build build/playground --target rnqnx_playground_host`                                                                        | 854 steps, exit 0, `Linking CXX executable bin/rnqnx_playground_host`              |
| `./build/playground/bin/rnqnx_playground_host --bundle build/dashboard.bundle.js -platform offscreen --screenshot build/dashboard.png` | `RNQNX_PLAYGROUND_SCREENSHOT build/dashboard.png 640x480`, exit 0                  |

The PNG is 11958 bytes and shows "dashboard on React Native QNX" on a dark
background. The first configure and build download and compile Hermes, folly and
Boost, and took the free disk from about 8.9 GiB to 3.2 GiB.

Not covered: the screenshot file was inspected by the agent that ran it and is
not committed. Nothing here ran on QNX.
