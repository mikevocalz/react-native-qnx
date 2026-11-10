# Bundle an external application

The CLI bundles and serves JavaScript from an external source directory. The CLI itself still comes from this checkout. [App creation](expo-project-create.md) and native build/deployment are separate commands; bundling or serving alone does not establish native support.

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

## Serve development bundles and source maps

From this checkout, start the external app's installed Metro server:

```bash
pnpm qnx project start --project /path/to/app
# Separate target configuration, same application source:
pnpm qnx project start --project /path/to/app --platform marathon --port 8082
```

The server binds only `127.0.0.1`, defaults to port 8081 and uses two Metro
workers. It stays in the foreground until Ctrl-C; unlike production bundling,
it has no two-minute lifetime limit. `--reset-cache` opts into clearing Metro's
transform cache. `--entry` selects a project-relative entry file. `--dry-run`
prints the plan without evaluating the config or starting the server.

The printed plan includes development bundle and source-map URLs with the
chosen platform, `dev=true` and `minify=false`. For the default entry these are
`http://127.0.0.1:8081/index.js.bundle?platform=qnx&dev=true&minify=false` and
`http://127.0.0.1:8081/index.js.map?platform=qnx&dev=true&minify=false`.
The initial `starting`/`ready: false` message is a launch plan, not a claim that
the port is listening. The child reports `listening` only after Metro binds successfully; startup and
bind/configuration errors exit nonzero.

JavaScript source changes are watched. Changes to the Metro configuration
require stopping and restarting this command. A TLS-enabled Metro config is
refused rather than silently downgrading it; use your own TLS launcher for that
configuration.

A syntax error is returned by Metro to the requesting client. Fix the source and
request the bundle again; the server does not need a restart. A port collision
fails with a nonzero exit: stop the existing server you own or choose another
unprivileged `--port`. SIGINT/SIGTERM remain nonzero cancellation results even
if Metro exits cleanly. The supervisor terminates its owned worker process
group; it does not terminate unrelated servers or native applications.

No bundle file is published to `build/`. Source maps and app source are served
only on loopback, not advertised on the LAN. Connecting a separate device
requires an independently configured secure transport; this command does not
create SSH tunnels or change networking settings. It does not install a native
development runtime, attach a debugger, reload a device or establish Fast
Refresh/state preservation. Those require the matching runtime implementation
and a separate target receipt.

With the opt-in public-entry config generated by `project create`, QNX and
Marathon development bundles use the RN 0.88 runtime setup for globals, DOM,
performance, promises, timers, error handling, networking and enabled observer
features. The legacy initializer and public `react-native/setup-env` import
resolve to the same cached module, preloaded before the app. Existing caller
preloads are retained; other platforms keep their original setup resolution.
Apple/Android React DevTools, LogBox and HMR startup are deliberately excluded
because this runtime has not implemented those native services. JavaScript
errors still use the existing ExceptionsManager path. Do not interpret this
bundle-initialization compatibility as debugger or state-preserving refresh
support.
