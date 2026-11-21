# Runner tiers: stock runner, custom development build, standalone app

An app reaches a QNX board in one of three ways. They differ in who compiles
the native host, so they differ in what you need installed and in how often
you wait for a compile. Pick the first tier that covers your app.

| Tier                     | The native host is                                     | Needs the QNX SDP | Native modules you can use                  | Status in this repository                                                   |
| ------------------------ | ------------------------------------------------------ | ----------------- | ------------------------------------------- | --------------------------------------------------------------------------- |
| Stock runner             | A prebuilt host from a runner archive                  | No                | Only the ones the runner already links      | `runner pack`, `fetch`, `check` and `analyze` exist; no runner is published |
| Custom development build | Built from your project, with your native modules      | Yes               | Any module that declares a QNX native build | `rnqnx project build-native`                                                |
| Standalone app           | The same build, with the compiled `.hbc` bundle inside | Yes               | Same as the custom build                    | `rnqnx project build-native`; device launch not run on this branch          |

A stock runner is built for one React Native version, one Hermes bytecode
version and one Nitro ABI. Your bundle has to match all three. A custom
development build and a standalone app are compiled from your own project, so
they match by construction.

## Choose a tier

Run `analyze` in your project. It lists the native modules the app brings and
says whether the stock runner links them.

```bash
pnpm qnx analyze --project apps/component-gallery
```

On the checkout's `apps/component-gallery` it prints the Expo modules
`@rn-qnx/expo-constants-qnx 0.1.0` and `@rn-qnx/expo-image-qnx 0.1.0`, the
Fabric components `react-native-safe-area-context 5.11.1` and
`react-native-screens 4.30.1`, and `Stock runner: undetermined`: four native
modules and no runner description to compare against. (Host output, 2026-11-20.)

Give it the `rnqnx-runtime.json` from the runner you plan to use and it answers
with a reason:

```bash
pnpm qnx analyze --project apps/component-gallery --runtime <rnqnx-runtime.json>
```

- `sufficient`: the runner links every one of the app's native modules at the
  same version. Use tier 1.
- `not sufficient`: the output names each module the runner lacks, or links at
  another version. Use tier 2.
- `undetermined`: the app has native modules and no runner description was
  given.

An app with no Nitro modules, Expo modules or Fabric components beyond React
Native itself is `sufficient` on modules. Versions are checked at launch.

## Tier 1: stock runner

1. `rnqnx runner fetch --target <target> --manifest <url>` downloads the
   archive for your exact versions and verifies size and SHA-256. See
   [the manifest format](../packaging.md#runner-manifest-format).
2. Unpack the archive. Its top level holds `rnqnx-runtime.json`.
3. Bundle the app, then launch with the runner named:

   ```bash
   rnqnx project run --project . --runtime <unpacked>/rnqnx-runtime.json
   ```

   Before anything is built or copied, the launch step compares the runner and
   the bundle. A mismatch stops the run and names both sides:

   ```text
   This runner cannot load this app (runner macos-arm64-dev-test).
     React Native     runner 0.88.1  app 0.87.0
     Hermes bytecode  runner 99      app 98
   Use a runner built for this app, or build your own: rnqnx project build-native --help.
   ```

`rnqnx runner check --runtime <file> --project . --bundle <file>` runs the same
comparison without the rest of `project run`.

What the check covers and what it does not:

- It reads the bytecode version from the bundle's `.hbc` header. A JavaScript
  source bundle has no bytecode version; the output lists it as not checked.
- The Nitro ABI is the exact version of the installed `nitro-core` (or
  `react-native-nitro-modules`). Nitro 1.x is an alpha, so any version
  difference counts as a mismatch. An app with neither is listed as not
  checked.
- It runs on the development host. The native runner does not yet show a
  message on screen when a bundle does not match.
- Copying to the board and launching are not implemented in `project run`; it
  stops at `deploy` with `needs device`.

## Tier 2: custom development build

Use it when `analyze` says the stock runner does not link a module, or when you
change native code. Install the QNX SDP, then follow
[Build a native consumer app](expo-native-build.md). The build links the
modules your `package.json` dependencies declare under `react-native-qnx`
([ADR-0019](../adr/0019-nitro-package-metadata.md)). For Fast Refresh, build
with `RNQNX_ENABLE_DEV_SUPPORT=ON` and start Metro with `rnqnx project start`.

## Tier 3: standalone app

The same build as tier 2, with the compiled `.hbc` bundle staged inside and no
Metro connection. Use it for the app you hand to someone else. See
[Build a native consumer app](expo-native-build.md).
