# Versioned offline QNX payloads

`scripts/deploy-qnx.sh` intentionally stages and rolls back only one executable.
It does **not** deploy an application dependency closure. Workspace
`build-packages.mjs` runs npm builds, not QNX application packaging. The offline
tool below fills that separate gap without changing deployment or contacting a
target.

```sh
node scripts/offline-package.mjs create package-input.json staging-input release-1.2.3
node scripts/offline-package.mjs verify release-1.2.3
```

Output is a portable versioned directory containing `manifest.json` and its
payload. The output directory must not exist and must be outside the source
root. The tool copies bytes, not symlinks, and never executes an input binary or
`ldd`. Existing releases are not replaced. The manifest is published last. Caught publication failures remove only the
exclusively created output directory; preexisting releases remain untouched. A
process crash/power loss may leave an incomplete directory without a manifest,
which must not be distributed. Always verify the package before use.

Example input (add the actual runtime libraries, plugins, assets and their
license material before a dynamically linked program will pass):

```json
{
  "schema": "rnqnx-offline-input/v1",
  "version": "1.2.3",
  "arch": "aarch64",
  "files": [
    {
      "source": "app",
      "path": "bin/app",
      "role": "executable",
      "component": "consumer",
      "requires": ["js/app.hbc", "assets/logo.png"]
    },
    {
      "source": "app.hbc",
      "path": "js/app.hbc",
      "role": "bundle",
      "component": "consumer",
      "requires": []
    },
    {
      "source": "logo.png",
      "path": "assets/logo.png",
      "role": "asset",
      "component": "consumer",
      "requires": []
    },
    {
      "source": "LICENSE",
      "path": "licenses/consumer.txt",
      "role": "license",
      "component": "consumer",
      "requires": []
    }
  ]
}
```

Roles are `executable`, `bundle`, `asset`, `library`, `plugin`, and `license`.
Exactly one executable and one `.js` or `.hbc` bundle are required. `requires`
contains paths inside the package, not source paths. Use it to declare runtime
resources that are not ELF link dependencies, including Qt plugin discovery
configuration, dynamically opened providers, QML imports, assets and fonts.
All payload components need a nonempty license-material file with the same
`component` value. This is an inventory requirement, not legal interpretation or
permission to redistribute proprietary QNX/Qt/vendor files.

## Fail-closed verification

- Relative portable paths only; reject traversal, absolute paths, backslashes,
  duplicate paths, symlinks, devices and other nonregular inputs.
- Record SHA-256, byte length, role, component, references and normalized mode
  for every file. Verify payload hashes/modes and reject unlisted files.
- Parse actual ELF64 little-endian headers for the selected aarch64/x86_64
  machine. Resolve `DT_NEEDED` and `PT_INTERP` loader names against uniquely named
  packaged libraries/plugins; check each library/plugin transitively. Missing
  or ambiguous libraries fail. There are no silently assumed system libraries.
- Verify every explicitly declared resource reference and component license
  inventory before publishing.

Qt installations commonly contain symlinks. Prepare an authorized regular-file
staging copy with the exact loader/SONAME filenames rather than supplying links.
The tool does not infer which license materials you may redistribute.

## Evidence and remaining boundary

Host tests exercise synthetic ELF structure (not executable target evidence),
actual Linux Node ELF dependency inspection without execution, missing closure,
missing license inventory, tampering, unlisted data, path traversal, symlinks,
wrong architectures and refusing existing output. Run:

```sh
node --test tests/js/offline-package.test.mjs
pnpm typecheck
```

Passing means integrity and **declared/ELF** closure, not complete discovery of
all dynamically loaded resources. It does not prove ABI compatibility, Hermes
bytecode-version compatibility, correct loader search paths, plugin initialization,
QNX launch, signing/authenticity or license compliance. SHA-256 detects changed
payload bytes relative to a trusted manifest; an attacker can replace an unsigned
manifest and payload together. Use trusted transport/storage and an independently
authenticated release manifest for production distribution. Build from a stable,
trusted local staging tree; concurrent hostile filesystem mutation is out of scope.

The tool deliberately does not integrate this directory into the binary-only
remote deployment script. Whole-release atomic activation, rollback and target
launch validation remain separate work requiring authorized target evidence.

Guidance applied: repository `qnx-platform-validation` requires host/target
claims to remain separate. No native ABI or Nitro API changes were made; no
premium Forge files are required or bundled by this tool.
