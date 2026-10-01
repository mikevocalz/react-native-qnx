# QNX Validation Queue

This queue separates host evidence, historical QNX evidence, current QNX
cross-compilation, and current physical-hardware proof. A previous successful
QNX run is never promoted across a React Native/Hermes or QNX SDP baseline
change without revalidation.

## Current target baseline

- QNX SDP 8.0.5
- QSTI 0.5.0 build 50
- Raspberry Pi 4 / Raspberry Pi 5
- AArch64le
- Stock target identity: `qnxuser@qnxpi.local`
- Current JS/tooling line: Expo 58.0.0, React Native 0.88.0-rc.3,
  React 19.3.0, Metro 0.87.1

## Allowed states

- Verified on current physical QNX hardware
- Verified on current virtual QNX target
- Cross-compiled on current QNX baseline
- Host-verified on current SDK line
- Historical QNX evidence; current revalidation required
- Implemented; current target validation pending
- Blocked
- Not started

## QNX-GFX-001 — Screen context/window/buffer lifecycle

**Status:** Historical QNX cross-compile/runtime evidence; current physical
validation pending.

**Production files:**

- `native/platform/qnx/screen/QnxScreenSurface.h`
- `native/platform/qnx/screen/QnxScreenSurface.cpp`

**Historical evidence:**

- QNX SDP 8.0.2 cross-built `qnx_platform` for AArch64.
- The same Screen wrapper ran successfully through the x86_64 QNX VM path.
- Host tests cover context/window/buffer creation, partial-failure cleanup,
  idempotent destruction, move ownership and buffer refresh behavior.

**Current source gate:** Before adapting Screen code, compare every used
function/property/constant with the installed QNX 8.0.5 `screen/screen.h`.
`scripts/audit-screen-symbols.sh` is the checked-in audit helper.

**Current target command path:**

```bash
pnpm pi:preflight
pnpm pi:install
```

**Advance to current physical verification only when:**

1. Context and application-window creation succeed on QSTI build 50.
2. Render buffers are created with the expected format/dimensions.
3. Normal shutdown destroys resources cleanly.
4. Repeated runs do not accumulate Screen resources.
5. Exact SDP/QSTI/board evidence is recorded.

## QNX-GFX-002 — ARGB fill, buffer acquisition and presentation

**Status:** Historical QNX VM pass; current physical display/GPU evidence
pending.

**Production files:**

- `native/platform/qnx/screen/QnxScreenSurface.cpp`
- `native/platform/qnx/application/main.cpp`

**Historical evidence:**

- QNX SDP 8.0.2 produced the AArch64 `rnqnx_playground` binary linked against
  `libscreen.so.1`.
- The x86_64 QNX VM run initialized a 1280x720 surface, rendered 600 frames and
  exited cleanly using the VM's software/dummy WFD configuration.
- Host tests check fill color encoding, buffer selection and post sequencing.

**Current physical evidence required:**

- Visible Screen output on Pi/QSTI.
- No post/fill errors.
- stdout and slog2 captured.
- Screenshot captured when the QNX screenshot utility is available.
- Repeated launch/exit behavior remains clean.

## QNX-RN-008 — Current end-to-end physical Pi validation

**Status:** Implemented; current target validation pending.

**Priority:** Highest.

**Current environment:**

- Licensed QNX SDP 8.0.5 host environment.
- Pi 4 or Pi 5 running QSTI 0.5.0 build 50.
- Screen/display available.
- SSH access, defaulting to `qnxuser@qnxpi.local`.

**Commands:**

```bash
export QNX_SDP_ENV=/path/to/qnxsdp-env.sh
pnpm install --frozen-lockfile
pnpm pi:preflight
pnpm pi:install
```

**Exit criteria:**

- Preflight confirms AArch64le + Screen.
- Current-line AArch64 build completes.
- Atomic deploy succeeds.
- `rnqnx_playground` initializes and exits cleanly.
- Evidence directory contains the run output and available target diagnostics.
- The exact board, QSTI build and SDP are recorded in `TEST_STATUS.md`.

## QNX-RN-009 — SDK-58 full React Native/Qt/Hermes target revalidation

**Status:** Not started on current QNX baseline.

**Host evidence available now:**

- SDK-58 dependency/source contracts pass.
- TypeScript and peers pass.
- Production Metro bundle for `--platform qnx` passes.
- Linux C++ host smoke tests pass.
- Dependency audit passes.

**Not yet proven:**

- Current RN 0.88 renderer cross-build for QNX.
- Current Qt Quick component backend on QNX.
- Current Hermes runtime on QNX.
- Full JSX → Metro → Hermes → React/Fabric → Qt Quick rendering on QNX.

**Advance only when:** the exact current dependency/source line is cross-built
with the licensed QNX toolchain and executed on a QNX target.

## QNX-HERMES-001 — Rebase historical Hermes QNX work

**Status:** Current source rebase required.

- Current bundled source: `hermes-v260318099.0.2`.
- Current compiler package: `hermes-compiler` 260318099.0.4.
- Historical patch: `patches/hermes-qnx-250829.patch` from the RN 0.86 /
  Hermes 250829 source line.

Every historical QNX change must be re-audited before reuse. Do not apply the
old patch wholesale to the current source.

## Queue discipline

Add or update an entry whenever a QNX-only source seam changes. Each entry must
name the upstream installed source/symbol authorizing the seam, identify whether
its evidence is host, historical QNX, current cross-build, virtual target, or
physical target, and list the exact command still required to advance state.
