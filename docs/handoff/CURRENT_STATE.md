# CURRENT_STATE.md

## Outcome

The macOS-host Milestone 0 foundation is complete and reproducible. The QNX
Milestone 0 exit gate is not complete because this workspace has no licensed QNX
SDP, installed `screen/screen.h`, Linux cross-compiler, or runtime target.

## Current baseline

- Authoring mode: mac-first; QNX remains the production target.
- Direct runtime/tooling line: React Native 0.86.2, React 19.2.3, Expo 57.0.12,
  Metro 0.84.4, Nitro Modules/Nitrogen 0.36.5, TypeScript 6.0.3, and pnpm
  11.21.0.
- Resolved Expo internals: `expo-modules-core` 57.0.10, `@expo/metro` 56.0.0,
  and `@expo/metro-config` 57.0.8.
- A scoped `xcode>uuid` override replaces vulnerable uuid 7.0.3 with 11.1.1;
  the installed CommonJS `uuid.v4()` seam is validated.
- Hermes identity from installed React Native source:
  `hermes-v250829098.0.16` / `hermes-compiler` 250829098.0.16.
- Yoga is the source snapshot bundled in React Native 0.86.2; no independent
  Yoga version is asserted.

These are exact, mutually compatible pins for the current Expo 57 / React Native
0.86 platform line. They are not a claim that incompatible next release lines
such as React Native 0.87, Metro 0.87, or TypeScript 7 can be substituted.

## Finished in the current working tree

- The workspace lockfile was regenerated with exact direct dependency pins.
- `validate:dependencies` verifies direct resolutions, peer baselines, the
  Hermes tag, resolved Expo internals, and named symbols in installed RN, Yoga,
  Nitro, and Expo source files.
- Every scaffold directory required by `validate:architecture` is tracked with a
  `.gitkeep` file so that validation survives a clean checkout.
- macOS/Linux host CMake presets build a strict-warning, ASan/UBSan test target.
- The production `QnxScreenSurface` lifecycle was corrected to use the typed
  window API, clean up partial initialization, encode ARGB fill attributes,
  preserve `errno`, and re-query the render-buffer list before each frame.
- Eight host cases exercise that concrete wrapper through a test-only subset of
  the documented QNX 8 Screen API, including all initialization failures and
  move ownership. This is host emulation, not an SDP compile or driver test.
- Validation, bootstrap, configure, build, deploy, architecture, manifest, and
  codegen-applicability scripts now fail explicitly instead of reporting
  placeholder success.
- The registry audit remains non-clean because no fixed `image-size` release is
  published for two Metro development-tool denial-of-service advisories. See
  `BLOCKERS.md`; Metro must not process untrusted assets.

## Installed-source boundaries

- RN Fabric entry points are grounded in installed `SchedulerToolbox`,
  `SchedulerDelegate`, `MountingCoordinator`, and `UIManagerBinding` sources.
- Nitro's installed generic C++ entry point is `margelo::nitro::install`, and its
  registry entry point is
  `HybridObjectRegistry::registerHybridObjectConstructor`.
- Nitrogen generation and Nitro platform specs currently support only iOS and
  Android; QNX generation/autolinking has not been designed.
- Expo Modules Core installs `globalThis.expo` only through its Apple/Android
  implementations; there is no installed QNX runtime installer.
- Expo Metro defaults omit `qnx`; no QNX Metro configuration exists yet.
- The installed RN package identifies the Hermes tag but contains no QNX Hermes
  build recipe or full Hermes source checkout.

No RN/Fabric/Nitro/Expo native QNX seam should be implemented until its design is
cited to these installed source files and symbols.

## Verification scope

See `TEST_STATUS.md` for command results. Host format/type/source/architecture
checks and the sanitizer-backed C++ suite pass. No QNX translation unit has been
compiled against an installed SDP, and no QNX binary has run.

## Repository state

The completed work is present as an uncommitted working-tree batch on `main`.
No commit was created by this session. Continue with `NEXT_ACTIONS.md` and
`QNX_VALIDATION_QUEUE.md`; do not mark Milestone 0 exited until QNX-RN-008 passes.
