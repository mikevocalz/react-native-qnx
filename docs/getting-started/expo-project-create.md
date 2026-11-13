# Create a native Expo application for React Native QNX

This is a **source-distributed experimental** CLI: it requires the React Native QNX checkout and a trusted local npm binary to create local tarball packages. Nothing has been published to a registry. Existing app creation and native build/deployment remain separate steps.

With the platform checkout available:

    node packages/react-native-qnx-cli/src/bin.mjs project create \
      --project ../notes-qnx --app-id org.example.notes --name "Notes"

To create **one source tree** targeting QNX and MarathonOS:

    node packages/react-native-qnx-cli/src/bin.mjs project create \
      --project ../notes-both --app-id org.example.notes --name "Notes" --with-marathon

A QNX-only project is an expo-router starter: a left drawer (Home, Settings, About and an account header) around three tabs (Home, Explore, Controls), from `template/qnx-router`. The tabs are a bottom bar under 700 px and a left rail from 700 px up. With the host's native Qt drawer and tab bar it uses them, and otherwise it draws the same UI from React Native views. Add tabs under `app/(tabs)` (at most five; put the rest under a More tab) and give each `Tabs.Screen` `options.qnxIcon`. The starter packs `@rn-qnx/expo-router-qnx` as a local archive with the other platform packages; see [its README](../../packages/expo-router-qnx/README.md). A `--with-marathon` project keeps the earlier single-screen example. A project created this way was installed with `pnpm install` and bundled with `pnpm run bundle:qnx` in a container on 2026-11-13; it was not built into a native host or run on the Pi.

This creates app.json with a unique reverse-DNS app ID, a React component name, the starter source, QNX Metro config, optional Marathon Metro config, local npm archives in vendor/ and a SHA-256 inventory. Node 26/pnpm 12.18.0 are the app's required development versions. The tool does not install dependencies, modify an existing directory, build a native binary, flash a device or make release claims. Avoid checking generated vendor archives into a public release without independently reviewing each included license and source.

After reviewing the source and its local package contents, install with the documented pinned dependency graph. Then run project doctor, project bundle and project prepare-native from the earlier phase implementation. QNX is always the default. A Marathon target requires its separately compiled Linux/Wayland native host; the QNX executable and Marathon executable are never interchangeable.

The source package inventory deliberately excludes SDKs, QNX evaluation runtimes, Nitro Forge premium products and arbitrary sibling files. The generated starter is a **non-premium** fixture, not application packaging or generalized native-module support. It has no stack navigator, and the `qnxIcon` glyphs depend on the system font. Fast Refresh needs a development-client host build (see the development workflow guide).

This command refuses existing directories, unsupported identifiers, duplicate/unknown arguments, and invalid package source contents; an implementation failure removes only the newly created application directory. Like other source-checkout development tools it assumes a trusted local filesystem and trusted packages. The --dry-run option validates the path and identity without creating the destination.

Device and release evidence remain governed by the QNX v1 gates, and Marathon requires its own shell/package/lifecycle qualification.
