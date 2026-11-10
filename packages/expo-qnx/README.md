# Prepare Expo module linking for QNX

Use this source-checkout command to discover installed Expo modules that declare
`qnx` support and generate their CMake linking metadata. You need the repository's
Node 26 and pnpm 12.8.1 dependency graph installed with `pnpm install --frozen-lockfile`.
The selected project must have a named `package.json` and its dependencies installed.

This is **not** `expo prebuild`, a standalone app template, Expo Go, Expo Router
qualification, native module registration, or a QNX build/deployment command.
Android/Apple-only modules are not automatically ported. Nitro package discovery
is a separate contract. No native compiler, installation, SSH or boot operation runs.

## Inspect modules before generating files

From the repository root:

```bash
pnpm expo:qnx inspect --project apps/rn-playground
pnpm expo:qnx prepare --project apps/rn-playground --dry-run
```

Discovery uses the installed Expo autolinking implementation through the existing
QNX adapter. Package dependencies, configured search paths and local modules
follow that implementation. A module needs `platforms: ["qnx"]`, a `qnx.target`,
and an in-package `CMakeLists.txt`. Only trust packages you intend to build;
validating metadata does not audit their native source or CMake instructions.
A zero-module result means nothing declared discoverable QNX support, not that
all your Expo dependencies work on QNX.

## Generate and verify linking files

```bash
pnpm expo:qnx prepare --project apps/rn-playground --arch aarch64
pnpm expo:qnx prepare --project apps/rn-playground --arch aarch64 --check
```

The default output is `build/expo-qnx` **inside the selected project**. Use
`--out build/expo-qnx-next` to select another new directory. Output must remain
below that project's `build/`; existing symlink directories are rejected.

The directory contains `modules.json`, `modules.cmake`, `config.json`, and
`manifest.json`. The config snippet comes from the existing `withQnxProject`
helper; the command does not evaluate or rewrite `app.config.js`, `app.json`,
or any other application file. Review and apply that snippet separately.
The generated CMake file defines `RNQNX_EXPO_QNX_MODULE_TARGETS`; your reviewed
native build may include it and link those targets. Explicitly ported providers can
also declare `registrationFunction` for the opt-in [native runtime wiring](../../docs/validation/expo-autolinking-runtime-2026-11-10.md). Legacy discovery-only
metadata remains preparation-only and does not prove modules can run.

Repeated preparation of identical inputs verifies the existing bytes without
rewriting them. Changed, missing, extra or unsafe files cause an error instead
of an overwrite. `--check` never creates output. `--dry-run` performs discovery
and validation but writes nothing. New files use owner-only permissions where
the host filesystem supports them. The manifest is written last; incomplete
output cannot pass `--check`.

## Handle changes and errors

After updating a module or changing architecture, choose a new `--out` directory
and compare it with the previous output. There is no `--force` or automatic
cleanup of a pre-existing directory. Your hand edits and unrelated files remain
untouched. A failed write rolls back only the new output directory created by
that invocation; newly created parent directories may remain.

Duplicate package/target/binary-directory identities, unsafe CMake tokens,
escaping source paths, symlink output paths and missing CMake files fail before
publishing output. Project paths with CMake expansion characters such as `$`,
`;`, quotes or control characters are unsupported. Shell/path environment and
installed dependencies are trusted inputs. These checks are not an OS sandbox
against another process changing files during execution.

## Evidence and remaining work

The automated integration test uses **real installed Expo discovery** and real
host CMake configuration with a synthetic INTERFACE target. It does not compile
or load a native Expo module. The unit suite separately checks file ownership,
repeatability, hashes, invalid options and unsafe metadata.

Manifests retain `nativeBuildVerified: false` and `runtimeVerified: false`.
Hashes cover generated files, not all module source, authenticity or licensing.
Absolute module paths make output specific to the checkout; regenerate it when
moving the project. QNX compile/runtime and physical device results remain separate acceptance
work. The opt-in registration lane has a separate Linux native host contract
test, not evidence that arbitrary unported Expo modules work. See the
[release gates](../../docs/releases/v1-readiness.md).
