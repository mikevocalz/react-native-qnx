# Packaging and Licensing Boundary

This document records constraints, not legal advice. No QNX, Qt, or npm package
has been produced by the current working tree.

## Project code

Repository-authored code is MIT-licensed. Third-party source and generated
artifacts retain their own notices and obligations; the project license does not
replace them.

## QNX

The QNX Everywhere introduction offers a free, non-commercial QNX SDP 8.0
license through a signed-in myQNX account. Commercial development or shipping
requires a separate license determination with QNX.

QNX SDP tools, sysroots, headers, libraries, target images, BSP payloads,
licenses, and account data are not committed or redistributed here. Public CI
cannot claim a QNX build unless it runs on an authorized private/self-hosted
runner and records only sanitized evidence.

Source: [QNX Everywhere introduction and license acquisition](https://www.qnx.com/developers/docs/qnxeverywhere/introduction.html).

## Qt module posture

ADR-0011 records module terms. The following stay outside the selected
LGPL-compatible core; adding them requires a compatible GPL product decision
or an independently chosen commercial route:

- `Qt Virtual Keyboard` — GPLv3-only open-source; required for on-screen
  keyboard on touchscreen targets.
- `Qt Quick 3D`, `Qt Graphs`, `Qt Lottie Animation`, `Qt Quick Timeline`,
  `Qt Quick 3D Physics`, `Qt Canvas Painter`, `Qt Qml Compiler`, `Qt HTTP Server`,
  `Qt MQTT`, `Qt OPC UA` — all GPLv3-only in the open-source edition.

Check every selected module against the pinned source notices; do not infer
terms for uninspected modules.

## Open-source Qt packaging

The owner selected the open-source Qt route in
[ADR-0013](adr/0013-open-source-qt.md). A paid Qt subscription is not a platform
prerequisite. Marathon uses the Linux/Wayland target; QNX uses its own toolchain
and runtime. Keep those target and artifact records separate.

Proceed with eligible public Qt sources and record the exact source channel,
tag/hash, module/QPA license files and built library/plugin closure. Include
the notices, corresponding source and library replacement/relinking materials
required by the selected terms. The existing LGPL-compatible module subset
stays in place; optional GPL-only modules need an explicit compatible product
choice. A commercial Qt subscription is one optional route, not the default
action or an approval stop for open-source development.

Qt's [general licensing page](https://doc.qt.io/qt-6/licensing.html) documents
the open-source option; [the LGPL terms](https://doc.qt.io/qt-6/lgpl.html) describe
the associated conditions. The [QNX commercial product page](https://doc.qt.io/qt-6/qnx.html)
does not replace the per-file/source audit for the selected open-source build.

## Pi 400 evaluation authorization (2026-11-06)

The owner requested the free ten-day Qt evaluation. The signed-in commercial
Qt portal was inspected and showed **Qt for Device Creation Enterprise,
Evaluation, Active**, expiring **17 November 2026**. This records a temporary
evaluation basis for the private Pi 400 bring-up, not a production or
redistribution authorization for those artifacts.
[ADR-0013](adr/0013-open-source-qt.md) selects the separate open-source Qt path.

The licensed account Downloads page supplied Qt **6.12.0 LTS**, full-framework
UNIX source archive `qt-everywhere-src-6.12.0.tar.xz`, SHA-256
`98ff4f44bac6ec3e1e62ee2a4316ae0e3d15badb015d753cc0268a20db52f165`.
Its top-level `.tag` is `ef25002ed292b132d1ed3246bfcee1d24a9cd966`.
The archive also matches the portal-provided `md5sums-6.12.0` value
`ac677cc64158740f257fd37a5b4c5ffb`. These are integrity checks, not an
independently verified vendor signature. Account identifiers, license numbers, credentials, and licensed
source are excluded from the repository.

Only qtbase (Core, Gui, Network), qtdeclarative (Qml, Quick), and qtshadertools
(matching host shader tools) are selected for this experiment. Their exact
source notices, QNX support, built dependencies, and deployment files must be
checked before the target runtime is accepted. Other optional modules remain
excluded. The evaluation does not accept this ADR's broader module matrix or
authorize release packaging. The expiry applies to these evaluation artifacts; it does not expire
independently obtained open-source Qt.

Sources: [Qt account licenses](https://account.qt.io/s/),
[Device Creation tiers](https://www.qt.io/development/qt-framework/target-devices),
[Qt for QNX](https://doc.qt.io/qt-6/qnx.html).

### Evaluation deployment subset

ADR-0011's restricted evaluation decision also records QmlMeta, Qml Models,
Qml WorkerScript and Quick Shapes, required by the existing Qt Quick runner.
The source headers were checked against the acquired 6.12.0 archive. Only
selected build targets and their actual runtime closure will be deployed;
Quick Controls, Quick Layouts, Widgets and other optional modules remain
outside this application deployment. Binary size, target build time and
rendered-window proof remain pending.

## npm packages

`@rn-qnx` is provisional. Do not publish until the owner supplies the real npm
organization, package ownership, provenance/authentication policy, and
CODEOWNERS identities. Independent release-please units and per-package
changelogs begin only after real package manifests exist.

## Release evidence

A release claiming QNX support requires, at minimum:

1. frozen dependency/source identities;
2. a licensed qcc cross-build on the supported host;
3. execution of the phase acceptance binary/app on the named QNX target;
4. cited installed/pinned native seams;
5. generated Nitrogen output when a HybridObject spec changes;
6. updated support/gap tables and an honest list of unimplemented capabilities.

### Observed OpenGL runtime dependency

The 6.12.0 QNX build also produces Qt OpenGL from qtbase, required by the
selected Quick rendering path. It is included in this restricted private
evaluation runtime. `qtbase/src/opengl/qopenglshaderprogram.cpp` offers
`LicenseRef-Qt-Commercial` alongside LGPLv3/GPL alternatives. The built QNX QPA
plugin is `plugins/platforms/libqqnx.so`; QML plugin filenames and the shared
library closure were inspected from the built artifacts, rather than assumed.

## Expo run modes

An Expo app reaches a QNX board or a desktop host in one of three ways. All
three use the same `native-app.json` from `rnqnx project prepare-native`.

| Mode                        | What runs                                                                   | Needs the SDP | Status on this branch                                                      |
| --------------------------- | --------------------------------------------------------------------------- | ------------- | -------------------------------------------------------------------------- |
| Prebuilt development runner | A published native host for one exact SDP + Qt + React Native set           | No            | `rnqnx runner fetch` downloads and verifies it; no runner is published yet |
| Custom development build    | A native host built from this checkout with the app's native modules linked | Yes (QNX)     | Not a separate command yet; `rnqnx project build-native` is the build path |
| Standalone package          | The app executable plus its staged bundle (`.hbc` on QNX)                   | Yes (QNX)     | `rnqnx project build-native`; device launch not run                        |

### Runner manifest format

`rnqnx runner fetch` reads a JSON manifest with schema
`rnqnx-runner-manifest/v1`:

```json
{
  "schema": "rnqnx-runner-manifest/v1",
  "publishedAt": "2026-11-12T00:00:00Z",
  "runners": [
    {
      "id": "qnx-pi400-dev-0.1.0",
      "target": "qnx-aarch64-pi400",
      "kind": "development",
      "versions": {
        "reactNativeQnx": "0.1.0",
        "reactNative": "0.88.1",
        "hermes": "<version>",
        "qt": "6.12.0",
        "sdp": "8.0.5"
      },
      "url": "runners/qnx-pi400-dev-0.1.0.tar.gz",
      "sha256": "<64 lowercase hex>",
      "size": 123456789,
      "format": "tar.gz"
    }
  ]
}
```

- `target` is one of `qnx-aarch64-pi400`, `macos-arm64`, `macos-x86_64`,
  `linux-x86_64`, `linux-aarch64`. Host runners set `versions.sdp` to `null`;
  QNX runners must name the SDP, and `fetch` for a QNX target requires
  `--sdp <version>`.
- The client picks the single `development` entry whose target and all five
  versions match exactly. A near miss fails and lists the published entries
  for that target, then points at `rnqnx project build-native`.
- `url` may be relative to the manifest. Manifests and archives must use
  `https`; plain `http` is accepted only for loopback test servers. Redirects
  and URLs with credentials are refused.
- The archive is streamed to a temporary file, checked against `size` and
  `sha256`, and only then renamed into
  `$XDG_CACHE_HOME/rnqnx/runners/<sha256>/` (default `~/.cache`). A mismatch
  deletes the download. A cached archive is re-hashed before reuse.
- Requests ask for `accept-encoding: identity`; a compressed response is
  refused, so `content-length` is the archive size. The manifest is read with
  a 1 MiB byte counter. A download that receives no data for 30 seconds is
  abandoned.
- `fetch` does not unpack, install or launch the runner and does not contact
  a device.

Limitation: the client refuses every redirect (any 3xx). The publication host
must serve the manifest and archives at their final URLs, so CDN or
release-asset URLs that redirect to signed storage URLs cannot be listed
directly.

`tests/js/qnx-runner-manifest.test.mjs` covers the format and client against a
local fixture server with synthetic archives.

### Owner TODOs before runners are published

- Publication host and the manifest URL to set as the default for
  `RNQNX_RUNNER_MANIFEST`. Until then no default exists and `fetch` says so.
- Real archives. The QNX runner comes from the public-source Qt 6.12.0 build
  (A1); its notices and accompanying materials must ship inside the archive
  under the open-source Qt packaging rules above.
- Publication basis: who signs off each entry, which source SHA and lockfile
  hash it was built from, and whether the manifest itself is signed. The v1
  client trusts the manifest over https and verifies archives by hash only.
- Redistribution terms for QNX SDP runtime files inside a QNX runner archive.
