# Bundle an external application

This first Phase1 implementation removes the source-directory restriction for JavaScript bundling. The CLI itself still comes from this checkout; standalone installation, app creation, native project generation and deployment remain unfinished.

Use the repository's Node26/pnpm prerequisites on Linux or macOS. The external app must have a package.json name, a local index.js entry (or --entry), installed compatible Metro/React Native dependencies and a target Metro config. QNX uses metro.config.js; Marathon uses metro.marathon.config.js with the existing corresponding platform factory. Configs and dependencies are trusted executable developer code.

From this platform checkout:

```bash
pnpm qnx project inspect --project /path/to/app
pnpm qnx project bundle --project /path/to/app --dry-run
pnpm qnx project bundle --project /path/to/app
pnpm qnx project bundle --project /path/to/app --platform marathon
```

The default target is QNX. Bundles go to build/qnx/app.bundle.js and build/marathon/app.bundle.js inside the external project. The destination must not exist; choose a new --out below build/ for another capture. Source/config/output paths reject traversal and symlinks. Work is staged privately and published exclusively only after Metro succeeds; a failed or interrupted build does not become a successful final bundle. Empty output, missing config/dependencies and child failure remain errors. Finite bundling has a two-minute watchdog and bounded termination of its owned process group.

inspect and --dry-run read metadata but do not execute Metro or its config, create directories, connect to devices or publish output. They cannot prove that a config implements the requested platform. The app may still require the existing runtime surface identity until the separate native app-identity work lands. This command does not turn unsupported public imports, Expo modules or Router into working native APIs.

The integration test provisions the existing CI dependency graph into a separate temporary application source directory and builds real QNX and Marathon bundles. It is not a clean-machine/package-publication proof. Unit tests use an explicit synthetic Metro executable to exercise safety/failure behavior. No new QNX or Marathon device result is implied.

QNX-first naming, the original workspace commands, runtime pins, lockfile, recovery boot and optional premium-product boundary stay unchanged. Path checks reject existing unsafe entries but are not a sandbox against concurrent hostile filesystem changes. No code is uploaded by the CLI.
