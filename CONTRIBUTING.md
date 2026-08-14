# Contributing

This repository implements an out-of-tree React Native platform for QNX. Read
[`docs/handoff/CURRENT_STATE.md`](docs/handoff/CURRENT_STATE.md) and the rest of
the handoff set before starting.

## Architecture locks

- React Native New Architecture only: Fabric plus JSI. Do not add the legacy
  bridge.
- Qt Quick is the view backend. QNX Screen is the window-system layer beneath
  Qt's QPA plugin and may also support a separately documented bare-Screen
  experiment; it is not the React Native component backend.
- Yoga is the only layout authority. Qt items are absolutely positioned leaves.
- Native modules use Nitro HybridObjects with manual registry integration. Do
  not introduce TurboModules as the QNX module architecture.
- Hermes is the JavaScript engine and must be built from the source tag selected
  by the pinned React Native release.

## Gate order

Technical work follows this order without skipping evidence:

1. G0-A: installed SDP identity, `qcc -V`, C++20 cross-build, and execution on
   the Pi.
2. G0-D: physical-device deploy/run/log loop with measured round-trip time.
3. G0-B: source-pinned Qt build and an on-glass Qt Quick proof.
4. G0-C: source-backed built-in-item strategy and Pi benchmark.
5. P1 onward in the order defined by the platform specification.

A locally prepared harness is not a passed gate. Host emulation, configuration
failure, and official documentation are useful evidence but cannot replace an
installed-header audit, a QNX cross-build, or target execution.

Work that does not depend on the QNX toolchain or the target device is not
gated by this order. Anything verifiable against an installed header on the
authoring host — React Native, Yoga, Nitro, Expo, Hermes, and a host Qt
installation — may proceed ahead of G0, provided its seams are cited the same
way and its portability claims stay honest.

## Source and API discipline

- Before calling a QNX, Qt, React Native, Hermes, Yoga, Nitro, or Expo native
  symbol, cite the installed header/source file and symbol or an exact pinned
  upstream source path in the Cited Seams ledger.
- Stop when the authoritative source is unavailable or disagrees with the
  proposed API. Never infer a native API shape from another platform.
- Run Nitrogen before implementing a HybridObject. Commit generated output with
  the specification that produced it.
- Unsupported behavior is an explicit typed error, documented no-op, or honest
  support-table gap. Never return invented success data.
- Do not commit QNX headers, libraries, images, license material, account data,
  device credentials, private keys, or proprietary Qt deliverables.

## Changes

- Use a Conventional Commit subject such as `feat(toolchain): add Pi probe` or
  `fix(screen): preserve errno`. Include a task identifier only when that task is
  defined in the repository.
- Keep a change focused. Include tests, generated Nitrogen output, cited native
  seams, support-table changes, and documentation with the implementation.
- Do not claim a real QNX build or runtime result without the recorded host,
  target, command, exit status, and observable evidence.

## Dependency and release discipline

- Use exact direct dependency pins and a frozen pnpm lockfile. A version bump
  requires registry verification, peer/source-contract validation, and a
  recorded decision when the platform specification pins a conflicting version.
- `tsc --noEmit` is a hard gate for every TypeScript package.
- Keep implementation, tests, generated artifacts, cited seams, support tables,
  and relevant docs in the same change.
- Do not publish, tag, or claim QNX support until the applicable real-device
  acceptance gate has passed. The `@rn-qnx` scope remains provisional until the
  owner supplies the publishable organization.

## Local validation

Install the exact package-manager version from `package.json`, then use the
frozen lockfile:

```bash
pnpm install --frozen-lockfile
pnpm validate:macos
git diff --check
```

Run the narrowest relevant checks while iterating, then run the complete locally
applicable suite before handoff.

QNX validation must use the exact commands and evidence requirements in
[`docs/handoff/NEXT_ACTIONS.md`](docs/handoff/NEXT_ACTIONS.md). Never promote a
QNX validation row from a host-only result. Host tests are not a substitute for
compiling against the installed SDP and running on the target.

## Handoff

Keep the handoff usable throughout the work, not only at the end. Before
yielding, update:

- `docs/handoff/CURRENT_STATE.md` with the actual outcome and dirty-tree scope;
- `docs/handoff/TEST_STATUS.md` with commands, host/target, exits, and evidence;
- `docs/handoff/BLOCKERS.md` with reproducible blockers and required authority;
- `docs/handoff/NEXT_ACTIONS.md` with the next executable step;
- `docs/handoff/SESSION_LOG.md` and `.project-state.json` with consistent state.

The next person must be able to continue without reconstructing decisions from a
conversation. Do not describe untracked work as committed or reproducible from
`HEAD`.

## Releases

Packages will use independent release-please releases once real package
manifests, owners, and the publishable npm organization exist. Until then, do
not publish, create release tags, or invent a CODEOWNERS identity.
