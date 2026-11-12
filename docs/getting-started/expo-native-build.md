# Build a consumer Expo app with its native Qt/Fabric/Hermes host

This is a **source-checkout** workflow. It does not yet support independent registry installation, device deployment or managed Marathon packaging.

After creating a project with `project create`, install its pinned packages, bundle for each target, and run `project prepare-native`. The generated `native-app.json` binds the app identity to an exact bundle hash. `build-native` rechecks that hash before compiling.

From the React Native QNX checkout:

```bash
pnpm qnx project build-native --project /absolute/path/to/app --host-preview
pnpm qnx project build-native --project /absolute/path/to/app --platform marathon
```

The first command is an **explicit QNX-on-Linux development preview**, never a physical QNX build. The second builds a separate Linux-native Marathon client. Both reuse the existing Fabric/Hermes/Qt Quick renderer. Separate CMake build directories are created under the consumer's `build/<platform>/` folder so apps do not share native build outputs.

For actual QNX cross-compilation, prepare an HBC bundle using the Hermes compiler **matching the runtime**, configure the licensed QNX SDP environment (including `QNX_HOST` and `QNX_TARGET`), and omit `--host-preview`. The command selects the repository's `qnx-aarch64-debug` preset and must fail when the SDK or HBC is missing. Compilation is not device installation, and the required QNX board/Qt provenance remains a separate release gate.

`--dry-run` prints the selected preset, flags and output paths without spawning CMake or creating files. The command refuses a stale descriptor, missing bundle, identity mismatch, cross-target confusion and unsupported previews. Native code, config files and installed packages are trusted local inputs; this is not a build sandbox.

The build command does not claim that native modules were registered, Expo Router/Worklets/RNGH work, debugger/Fast Refresh is available, or that the app was launched inside MarathonOS. The QNX v1 physical and release gates and independently reviewed Marathon managed-app lifecycle remain open. No Nitro Forge dependency is bundled.
