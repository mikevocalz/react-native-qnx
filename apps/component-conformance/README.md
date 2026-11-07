# QNX non-premium conformance fixture

This application exercises the **existing Fabric/Hermes/Qt Quick React Native
path**. It is a source-checkout fixture, not a standalone npm package and not
an installable QNX application. It uses synthetic data and no Nitro Forge
components, binaries or entitlement code.

## Build the QNX JavaScript bundle

From the repository root, with Node 26+, pnpm 12.8.1 and the repository's
frozen dependencies installed:

```bash
pnpm install --frozen-lockfile
pnpm qnx bundle-qnx --app component-conformance --out build/component-conformance.bundle.js
```

This creates JavaScript only. To render on a supported development host, follow
the [Qt host prerequisites](../../docs/toolchain.md) and run:

```bash
cmake -S . -B build/playground -G Ninja -DCMAKE_BUILD_TYPE=Debug -DRNQNX_BUILD_HERMES=ON
cmake --build build/playground --target rnqnx_playground_host
./build/playground/bin/rnqnx_playground_host --bundle build/component-conformance.bundle.js
```

The entry registers `RNQnxPlayground`, matching the existing React Native
playground host. For QNX, follow the separate
[Pi 400 runtime bring-up](../../docs/getting-started/pi400-hello-world.md)
or [stock Pi QSTI guide](../../docs/getting-started/raspberry-pi-today.md).
The generic `deploy-qnx` script does not provision the full runtime.

## What to exercise

| ID | Manual action | Acceptance to record |
| --- | --- | --- |
| LIST-001 | Scroll 1,000 rows; select Document 0500; switch pages and return | Correct row selection, stable list, no phantom press |
| INPUT-001 | Edit the field, toggle the switch, press twice and revisit | Correct controlled text, switch and press count; record focus behavior |
| LAYOUT-001 | Resize wide/narrow; return to selected document | Layout changes without losing the selected row |

The **Limits** page lists five later scenarios that remain blocked: interrupted
gesture drawer, multilingual IME, non-premium Nitro document binding,
permissioned native service and lifecycle restoration. Do not turn a blocked
scenario into a green result because the page renders.

## Evidence to attach for each run

Record the target OS, board and image, host versus physical device, revision,
bundle SHA-256, command and exit code, actions actually performed, screenshots,
logs, observed result and known gaps. Use `passed`, `failed`, `not-run` or
`blocked` per scenario. Keep stock Pi 4 QSTI and custom Pi 400 separate.
Screenshots of a loaded app do not prove physical HID, IME, accessibility
reader output, FPS or complete native module support.

The [component support table](../../docs/component-support.md) and
[v1 release gates](../../docs/releases/v1-readiness.md) remain the acceptance
authorities. A successful Metro build is a **host bundle result only**, not a
QNX device qualification. The fixture doesn't change the runtime, feature
flags, graphics startup, or the premium-component boundary.
