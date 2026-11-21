---
name: react-native-qnx
description: Work in the react-native-qnx repository (React Native 0.88 on QNX with Fabric, Hermes, Yoga and Qt Quick). Use it to create and bundle a QNX app, run the host test suites, read component support status, change the docs site, or report what is device-verified. States what is and is not proven.
---

# react-native-qnx

Repository: https://github.com/mikevocalz/react-native-qnx (MIT, see `LICENSE`). Documentation: the `/docs` pages of this site. Site source: `docs-site/`.

The project is experimental and pre-v1. All 11 release gates in `docs/releases/v1-readiness.json` are pending, and nothing is published or tagged.

## Rules

- Claim only what a receipt shows. A device result means the one custom Raspberry Pi 400 image (BCM2711, QNX 8.0.5). No stock Pi 4 result exists.
- Component status words are `supported`, `partial`, `no-op`, `unsupported`, `scoped`, `failed` and `unverified`. Read them from `docs/component-support.json` or `/docs/reference/component-matrix`, not from memory.
- A host test or a hosted CI run says nothing about QNX. Never present it as a QNX or device result.
- Do not run QNX builds, deploy, or touch a board unless the user asked. No workflow in the repository cross-builds for QNX.
- Cite the installed header or pinned source before calling a native symbol. Unsupported behavior is a typed error, a documented no-op or a gap in the support table.
- Architecture is fixed: Fabric and JSI only, Hermes, Yoga as the layout authority, Qt Quick as the view backend, Nitro HybridObjects for native modules. See `CONTRIBUTING.md` and `/docs/explanation/architecture`.

## Setup

Needs Node 26 or newer and pnpm 12.21.0 exactly (`packageManager` in `package.json`). From a checkout:

```bash
npm install -g pnpm@12.21.0
pnpm install --frozen-lockfile
pnpm validate:dependencies
pnpm qnx --help
```

Details: `/docs/tutorials/quick-start` and `/docs/contributing`.

## Create and bundle an app

```bash
pnpm qnx init dashboard
pnpm install --no-frozen-lockfile
pnpm qnx bundle-qnx --app dashboard --out build/dashboard.bundle.js
```

`init` creates `apps/<name>`, requires a name matching `^[a-z][a-z0-9-]{0,62}$`, refuses an existing directory and does not accept `--dry-run`. The bundle output must be inside `build/`. The other executable commands (`bundle-qnx`, `doctor`, `configure-qnx`, `build-qnx`, `deploy-qnx`, `run-qnx`, `log-qnx`, `evidence verify`) accept `--dry-run` and print what they would run.

## Other commands

- `pnpm qnx doctor`: checks host tools and the QNX SDP environment. It exits 1 without an SDP, and each failure prints a `Fix:` line.
- `pnpm qnx project <inspect|create|add-qnx|bundle|doctor|prepare-native|build-native|desktop-entry|run|start|dev>`: build and run an app outside this checkout. See `pnpm qnx project --help` and `/docs/reference/cli`.
- `pnpm qnx runner fetch|list`: prebuilt development runner. No manifest is published, so pass `--manifest <url>` or set `RNQNX_RUNNER_MANIFEST`. See `/docs/how-to/prebuilt-runner`.
- `pnpm qnx:device status|logs|screenshot|port-forward`: board status over SSH.
- `pnpm doctor`: the `scripts/doctor.sh` host check.

## Test and validate

```bash
pnpm test:js            # tests/js/*.test.mjs and *.test.ts
pnpm test:cpp:host      # cmake preset macos-host-tests or linux-host-tests, build, ctest
pnpm typecheck
pnpm format:check
pnpm validate:docs      # generated tables against docs/component-support.json and v1-readiness.json
pnpm validate:macos     # the eight host stages in order; no QNX cross-compile
```

The checks that gate pull requests to `main` are `validate` (`sdk58-validation.yml`), the two `Qt 6.11 / RN 0.88 / Hermes (input lifecycle ON|OFF)` jobs (`host-renderer.yml`) and `image-cache` (`image-loader.yml`). See `docs/releases/required-checks.md`.

Commit subjects use Conventional Commits, for example `fix(screen): preserve errno`.

## What was verified on the Pi 400

Scoped runs on 2026-11-19, on a 1024x768 display: Reanimated timing and spring, Gesture Handler pan, tap and double-tap, SVG and expo-image cards, Escape closing a Modal and a drawer, and held-key repeat sent as synthetic Screen events. Receipts are in `docs/evidence/pr149-pi400-run1-2026-11-19`, `pr149-pi400-run2-2026-11-19` and `pr149-pi400-keyhold-2026-11-19`. Earlier scoped receipts cover `View`, `Text`, `TextInput` and `Pressable`, and a 1920x1080 Hello World (`docs/evidence/pi400-hello-world-2026-11-07.txt`).

## Not verified

Pinch, rotation and multi-pointer gestures. 1920x1080 output beyond the Hello World capture. Video and audio in the 2026-11-19 builds (`RNQNX_ENABLE_QT_VIDEO=OFF`; an earlier scoped receipt is `docs/evidence/media-2026-11-13.md`). A held key on a real USB keyboard. A stock Pi 4. All 11 release gates, including a QNX cross-build check. Pages: `/docs/reference/status` and `/docs/reference/known-gaps`.

## Decisions

Architecture decision records are in `docs/adr/`, indexed in `docs/adr/index.md` and at `/docs/explanation/adr-index`. Copy `docs/adr/adr-template.md` for a new one.

## Docs site

```bash
cd docs-site
npm ci
npm run data:check && npm run lint && npm run types:check && npm run build && npm run check:content
```

`npm run data` regenerates the component matrix, release gates and ADR index pages from `docs/`. Do not edit those three pages by hand. Pages are MDX in `docs-site/content/docs`, one Diataxis type each (tutorials, how-to, reference, explanation). Every page needs `title:` and `description:` front matter, and internal links look like `/docs/explanation/threading`.
