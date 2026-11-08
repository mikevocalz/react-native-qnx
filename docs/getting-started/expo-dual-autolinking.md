# Discover explicitly supported Expo native modules

This Phase2 slice extends the existing QNX autolinking adapter with an explicit Marathon target. It does not implement native modules, generate Nitro bindings or register them in Hermes. The original QNX exports and valid QNX CMake output remain compatible.

A package must list qnx or marathon in expo-module.config.json platforms and provide a matching configuration object with its target, modules and optional cmakeListsPath. Sharing Qt does not authorize falling back to a qnx declaration when marathon is missing. Apple-only and other-target declarations are excluded from the selected discovery result; this is not a full audit of undeclared required native dependencies.

From the repository root, with the pinned dependencies installed:

```bash
node packages/expo-qnx-autolinking/bin/expo-qnx-autolinking.js --project-root /path/to/app --platform marathon --json build/marathon-modules.json --cmake build/marathon-modules.cmake
```

The default platform is qnx. Use separate output files for each target. JSON records the selected platform; CMake uses RNQNX_EXPO_QNX_MODULE_TARGETS or RNQNX_EXPO_MARATHON_MODULE_TARGETS and separate build directories. Invalid targets, duplicate target/binary identities and CMake metacharacters fail before writing. Unknown/duplicate options and colliding JSON/CMake output paths are rejected.

This low-level generator preserves its existing behavior of overwriting the explicitly selected generated output files. Use only owned build-output paths; no atomic multi-file transaction or untrusted-source sandbox is claimed. Native package CMake/code is trusted executable dependency content. The higher-level expo:qnx prepare command remains QNX-only in this slice and retains its separate no-overwrite/check workflow.

The programmatic API adds findExpoModulesForPlatform, resolveExpoSearchResults and generateExpoPlatformCMake. Old QNX functions delegate with qnx explicitly. The tests use real installed Expo discovery and real CMake configuration with INTERFACE fixtures; they do not prove native cross-compilation, registration, ABI compatibility or device behavior. Nitro v1 and runtime/lockfile pins are unchanged. Forge is not a dependency.
