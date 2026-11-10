# Offline Qt runtime candidates

This opt-in tool validates supplied local inputs and creates a **new** local
candidate directory. It does not obtain or build Qt, execute binaries, deploy,
modify system libraries, authorize distribution, or change the seven release
gates. Existing executable-only deployment remains unchanged.

Use a separately identified eligible public-source build, not the historical
private evaluation prefix. See [ADR-0013](adr/0013-open-source-qt.md),
[the module boundary](adr/0011-qt-module-licensing.md), and
[packaging](packaging.md). Source URLs and receipts are assertions supplied by
the builder; local hashes establish integrity, not authenticity or legal
certification. Review all selected module, QPA, transitive and font terms.

## Invocation

Run as an unprivileged user with repository Node, after preparing the inputs:

```sh
node scripts/stage-qt-runtime.mjs \
  --manifest /work/receipts/candidate.json \
  --qt-prefix /work/public-qt-qnx \
  --app /work/app/rnqnx_playground_qnx \
  --bundle /work/app/matched.hbc \
  --output /work/candidates/new-candidate
```

The output's parent must already exist. Output must not exist and must be
outside the Qt prefix and manifest/material root. All five options are
required exactly once. There are no network, target, shell, root, download,
launch, release, or force/overwrite options. Input directories must remain
immutable during validation/copying; this is not a sandbox for adversarial
concurrent filesystem mutation. Failed validation creates no output. Copy
failure removes only the directory exclusively created by that invocation.

The standalone functions are `validateCandidate({manifest, qtPrefix, app,
bundle})` and `stageQtRuntime({manifest, qtPrefix, app, bundle, output})`.
They reject with an Error naming the missing/invalid input. Neither returns
a ready-to-release package. For example, catch a missing dependency, correct
the builder's inventory and retry with a new output name. Do not fix a missing
dependency by copying a whole SDK/prefix or adding Qt to target-system inputs.

## Input contract (schemaVersion 1)

The manifest is JSON. Fields below are required; unknown fields on defined
records are rejected. Hashes are lowercase SHA-256. Identifiers use letters,
digits, `.`, `_`, `-`; paths use confined POSIX relative paths with no empty,
`.` or `..` segments, backslashes, URI schemes or absolute paths.

| Field                | Required content                                                                                                                                           |
| -------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `schemaVersion`      | `1`                                                                                                                                                        |
| `profile`            | `baseline` or `desktop`, explicit                                                                                                                          |
| `repository`         | `{commit, sourceMaterial}`; exact 40-hex source commit and local source-archive material                                                                   |
| `app`                | `{sha256, bundleSha256, hermesSha256, buildMaterial}`; supplied ELF/HBC hashes, exact Hermes source hash and app-build receipt material                    |
| `abi`                | `{os:"qnx", class:64, endian:"little", machine:183, osabi, abiVersion, flags}`; exact ELF header expectations                                              |
| `qt`                 | Fields in the table below                                                                                                                                  |
| `materials`          | Array of `{id, path, sha256, role}`, paths relative to the manifest's directory; all are copied                                                            |
| `components`         | Array of `{id, version, channel:"public-source", sourceMaterial, buildMaterial, licenseMaterials:[ids], noticeMaterial, terms}`                            |
| `files`              | Explicit selected Qt-prefix inventory, described below; no recursive prefix copy                                                                           |
| `qmlModules`         | Array of `{uri, path, dynamicImports:[uris]}`; `path` must be `qml/` plus URI dots changed to `/`                                                          |
| `qmlRoots`           | Explicit root URI array, including inline imports and profile requirements                                                                                 |
| `inlineSources`      | Nonempty material ID array, role `inline-source`; include the app/factory C++ sources containing QML strings                                               |
| `dynamicPlugins`     | Nonempty prefix-relative ELF plugin paths; include QPA and every enabled dynamically loaded image/platform/etc. plugin                                     |
| `systemDependencies` | Array of `{name, sha256, material}`; separate target-provided DSO basename or exact interpreter absolute path, expected target hash, role `system` receipt |
| `replacement`        | `{mechanism:"shared-library", instructionsMaterial, installationMaterial}`                                                                                 |

| `qt` field                                       | Meaning                                                                                                                                                                                            |
| ------------------------------------------------ | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `channel`                                        | Exactly `public-source`; evaluation/commercial-channel receipts are rejected                                                                                                                       |
| `version`, `hostVersion`, `targetVersion`        | Exact stable `6.x.y`, all equal; no dependency version change is implied                                                                                                                           |
| `sourceUrl`                                      | Eligible HTTPS public Qt archive/snapshot URL from `download.qt.io/official_releases/qt/`, `code.qt.io/cgit/qt/`, or `github.com/qt/`; no credentials/query/fragment or evaluation/commercial path |
| `sourceSha256`, `sourceMaterial`                 | Complete local source archive/snapshot hash and matching role `source` material; not a hash of a URL, version label or selected source file                                                        |
| `modificationsMaterial`                          | Role `modifications`; complete patches or an explicit no-modifications record                                                                                                                      |
| `recipeMaterial`                                 | Role `build-recipe`; exact reproducible configure/build/install commands and source revision                                                                                                       |
| `configurationMaterial`                          | Role `configuration`; exact module/features/build flags and host/target configuration                                                                                                              |
| `toolchainMaterial`, `compiler`, `sysrootSha256` | Role `toolchain` receipt, exact compiler identity, hash of the complete sysroot identity inventory; do not include licensed SDK payloads                                                           |
| `qpaPlugin`                                      | Selected `plugins/platforms/…` ELF path also present in `dynamicPlugins`                                                                                                                           |

Material roles are `source`, `modifications`, `build-recipe`, `configuration`,
`toolchain`, `license`, `notice`, `replacement`, `installation`, `app-build`,
`component-build`, `inline-source`, `system`. All references must have the expected role and
matching actual local file hash. Material symlinks and symlinked ancestors
are rejected. The corresponding source for every component is a local material,
not merely an offer URL. Group component records only when the source, notices,
selected terms and build identity really match; otherwise separate them.

Selected terms currently accepted are `LGPL-3.0-only`, `LGPL-3.0-or-later`,
`MIT`, `BSD-2-Clause`, `BSD-3-Clause`, `Apache-2.0`, `Zlib`, `FTL`, `OFL-1.1`,
`Unicode-3.0`. This narrow vocabulary is not an eligibility decision. Do not
relabel a GPL-only, premium, SDK or evaluation component to pass it. An actual
additional license needs review and an explicit contract extension.

### App-build receipt

The hashed `app-build` material is JSON with these exact identity values
matching the manifest (notation below describes fields, not real hashes):

```text
qtChannel          = public-source
repositoryCommit   = repository.commit
appSha256          = app.sha256
bundleSha256       = app.bundleSha256
hermesSha256       = app.hermesSha256
qtSourceSha256     = qt.sourceSha256
qtVersion          = qt.version
compiler           = qt.compiler
sysrootSha256      = qt.sysrootSha256
abi                = abi
```

Each component's `buildMaterial` has role `component-build` and contains JSON
`{channel:"public-source", sourceSha256, version, qtVersion, compiler,
sysrootSha256, abi, files:[{path, sha256}]}`. Source hash must match that
component's local source material; version must match the component, and
Qt/compiler/sysroot/ABI identities must match the candidate. The files must
match exactly the selected files belonging to that component, including link
text hashes. Produce a selected-subset receipt from the full build inventory,
not a new provenance label applied to an evaluation prefix. Evaluation build
receipts are rejected even if the outer manifest says public-source.

The builder must produce these receipts from the same app/HBC/toolchain build.
The validator verifies identity agreement and actual supplied app/HBC hashes;
it cannot derive the Hermes compiler/source version from an opaque HBC or
prove the receipt truthful. A matching version string is not ABI proof.
ELF class/endian/machine/OSABI/ABI-version/flags are checked on every staged
ELF. Many QNX and Linux objects share System V OSABI; those headers alone do
not identify the OS, C++ ABI or build options. Compiler/sysroot/source receipts
and later cross-build/runtime qualification remain necessary.

### Runtime files and ELF closure

Each ordinary runtime file has `{path, kind, sha256, component}`. Kinds are
`elf`, `qml`, `qmldir`, `asset`. Paths are under `lib/`, `plugins/`, `qml/` or
`fonts/`. `lib/` is flat and contains only shared libraries/aliases. Static
archives, headers and build-system payloads are rejected. Assets cover actual
QML type information, shaders, fonts and related runtime data. ELF magic may
not be disguised as an asset. Component references supply source/terms/notices.

A library symlink uses `{path, kind:"symlink", target, sha256, component}`.
Only a basename target in the same `lib/` directory is accepted. Its hash is
SHA-256 of the UTF-8 link text. The target must be a listed regular ELF, not a
second link. No directory, plugin, QML or material symlinks are allowed.

The parser reads actual ELF64 little-endian program headers, dynamic tables,
DT_NEEDED, SONAME, RPATH/RUNPATH and interpreter. It never executes `ldd`, the
app, or a cross-tool. Every runtime ELF must be ET_DYN with no interpreter;
the app must dynamically need Qt Core and Quick. A physical SONAME filename
or confined alias is required; a SONAME string alone cannot satisfy loading.
Every listed ELF/plugin's dependencies are resolved recursively, including
QML implementation plugins, even if not linked by the app. Required names
must exist under the staged `lib/` or confined `$ORIGIN` search directories,
or be separately declared target-system dependencies. Ambiguous providers,
absolute/private-prefix/empty/escaping RPATHs, ELF filters/auditors and
missing loader declarations fail. Qt/Hermes cannot be target-system exceptions.

This is a deliberately narrow loader contract, not an implementation of all
QNX loader search semantics. Keep dependencies in flat `lib/` and use confined
origin-relative paths. Runtime environment paths are package-relative and
must later be made absolute by the consumer launcher without empty elements
or private prefix fallbacks. Target-system hashes are expectations, not
measurements from this offline run; no target file is read or copied.

### QML and plugin profiles

`baseline` requires `QtQuick` and `QtQuick.Shapes`, including the existing
inline Shapes use. It does not require Controls or Templates: the gallery
Switch uses Item/Rectangle/MouseArea. Baseline rejects desktop-only modules.
`desktop` also requires `QtQuick.Controls`, `QtQuick.Controls.Basic` and
`QtQuick.Templates`, with their actual recursive dependencies/plugins.
Neither profile permits `QtQuick.Layouts`; Yoga remains layout authority.

All declared modules need `qmldir` with a matching `module` URI. Validation
follows `depends`/`import`, selected `.qml`/`.js`/`.mjs` imports and the supplied
inline C++ source materials. It checks qmldir type files and implementation
`plugin` entries (including `optional plugin`, conservatively required),
recursively inventories selected module directories, and rejects unlisted
content or escaping/remote local imports. Modules must be reachable from
roots or explicit `dynamicImports`. Files embedded in resources must be
represented through source materials/import roots; compiled resources are not
decoded. Version availability, computed import/plugin URLs, QML engine style
selection and arbitrary QML/JS file I/O cannot be proved by this scanner.
The builder must enumerate them. Unsupported directory/plugin layouts must
fail or receive a reviewed contract extension, not a guessed plugin filename.

## Output and limits

The new tree contains `bin/app`, `app.hbc`, `lib/`, selected `plugins/`,
`qml/`, optional `fonts/`, confined `materials/`, `bin/qt.conf`,
`runtime-paths.json`, `candidate-input.json` and `candidate-receipt.json`.
No launcher or fullscreen arguments are changed. No input is executed.
The receipt records per-file hashes/modes/sizes, inspected dependency edges,
QML closure, component/material identities and system exceptions. It always
says `execution: "NOT RUN"` and `releaseAuthorized: false`. The receipt does
not hash itself; hash it externally when collecting candidate evidence.

For identical input bytes the generated JSON, contents, link text, permissions
and mtimes are fixed; there are no wall-clock timestamps or local absolute
source paths in the receipt. Inodes, filesystem ownership and archive formats
are not normalized. Limits are fixed in `LIMITS`: 2 MiB manifest/text/string
tables, 128 MiB parsed ELF, 2 GiB per copied file, 8 GiB total input bytes,
8192 entries per inventory, 24 directory/JSON levels, 100000 JSON values and
65536 traversed QML import edges. Inputs exceeding these bounds need a reviewed
contract change; callers cannot disable limits.

## Replacement and acceptance boundary

This slice selects the shared-library mechanism, not static Qt or a
relinkable-application-object route. Materials must contain the exact Qt and
transitive source, modifications, build recipe/configuration, applicable
license texts/notices, user instructions for rebuilding an interface-compatible
replacement and selecting it from this private runtime tree, and applicable
installation information. The tool verifies presence/hash/identity, not the
legal adequacy of prose or a successful replacement. Do not include SDK,
premium components, private evaluation archives or unrelated credentials in
materials. No blanket application-object requirement is imposed.

After all lanes are integrated, the parent/integrator should:

1. Run `node --test tests/js/qt-runtime-staging.test.mjs` as an unprivileged
   host user. The fixtures contain structural ELF bytes and synthetic text,
   not Qt, real HBC or QNX execution. Negative cases cover missing closure,
   provenance/material tampering, ABI/build pairing and path confinement.
2. Supply a real independently identified public-source build and inspect its
   per-component notices, QPA, transitive ELF/plugin/QML/font inventory and
   exact app/HBC receipts. No such build is claimed by this implementation.
3. Optionally wire a local staging command without changing default deployment,
   root manifests/lockfile ownership or default-OFF features. Reject unsupported
   layouts rather than weakening confinement. The parent owns shared wiring.
4. Later test the same app/HBC from the staged tree with no evaluation-prefix
   fallback, full screen preserved, target-system hashes verified, recursive
   loading failures exercised and the documented replacement demonstrated.
   Device/SDK/startup access and all final tests belong to the parent.

Implementation-stage tests, lint/type/syntax checks, staging invocations,
native builds, runtime, replacement and device acceptance are **NOT RUN**.
Nothing here qualifies hot reload, QNX support, release packaging or a gate.
Release/dependency engineer is the workstream role; independent QNX regression
and security/licensing reviewers are required. Human assignments are unassigned.
Rollback is to stop invoking the standalone command and remove only its own
local candidate directories; no shared deployment or system state was changed.
