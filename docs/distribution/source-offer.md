# Written source offer for the Qt runtime

**Status: draft for the owner's review.** It states what the repository can
supply today. It is not legal advice, and nobody has checked it against the
license texts with counsel. A distribution that includes the Qt runtime must
not ship this file as final until the owner has done that and filled in the
contact line below.

The react-native-qnx Qt runtime contains Qt 6.12.0 libraries and plugins built
from the public source archive. Qt is licensed here under
LGPL-3.0-only ([ADR-0013](../adr/0013-open-source-qt.md)). Several bundled
third-party components carry their own licenses; each is named with its
copyright line in the runtime's `licenses/THIRD_PARTY_ATTRIBUTIONS.txt`.

## What you can get

Anyone who received a copy of the runtime can get the Corresponding Source for
it, at no charge beyond the cost of the medium and shipping if you ask for a
physical copy. It consists of:

| Part                     | Where                                                                                                                                                                      | Identified by                                                                                                      |
| ------------------------ | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------ |
| Qt 6.12.0 source archive | `https://download.qt.io/official_releases/qt/6.12/6.12.0/single/qt-everywhere-src-6.12.0.tar.xz`                                                                           | SHA-256 `98ff4f44bac6ec3e1e62ee2a4316ae0e3d15badb015d753cc0268a20db52f165`, MD5 `ac677cc64158740f257fd37a5b4c5ffb` |
| Three qtbase diffs       | `https://download.qt.io/official_releases/qt/6.12/` (`CVE-2026-116337-qtbase-6.12-0001.diff`, `CVE-2026-116337-qtbase-6.12-0002.diff`, `CVE-2026-105924-qtbase-6.12.diff`) | SHA-256 values in `scripts/qt-public/apply-cve-patches.sh`; also in `SOURCE-MANIFEST.json`                         |
| react-native-qnx patches | `scripts/qt-public/patches/`                                                                                                                                               | Git commit recorded in `SOURCE-MANIFEST.json`                                                                      |
| Build recipe             | `scripts/qt-public/` in `https://github.com/mikevocalz/react-native-qnx`                                                                                                   | Git commit recorded in `SOURCE-MANIFEST.json`                                                                      |

`scripts/qt-public/make-source-bundle.sh` packs the diffs, the patches, the
recipe and `SOURCE-MANIFEST.json` into one tarball. With `WITH_ARCHIVE=1` it
also packs the Qt archive, so that the offer does not depend on the Qt mirror
staying up. Each release lists the SHA-256 of its bundle.

The exact patch set of a given runtime is the one named in that release's
package manifest (`docs/releases/release-units.json` names the manifest file).
At the time of writing, `apply-qnx-patches.sh` applies two react-native-qnx
patches (`qtbase-qnx-offscreen-surface.patch`, `qtbase-qnx-resize-swap.patch`)
and does not apply `qtbase-qnx-software-grab.patch`; see
[the software-grab proposal](../evidence/distribution-2026-11-21/proposals/software-grab-decision.md).
A fourth qtbase diff, `CVE-2026-105943-qtbase-6.12.diff`, is not applied
because it changes only a Windows SSPI code path that this build does not
compile; the reasoning is in the
[distribution receipt](../evidence/distribution-2026-11-21/README.md).

## Replacing the Qt libraries

Qt is linked as shared libraries (`libQt6*.so.6`), so you can rebuild Qt from
the source above and run an application against your build. The build steps
are in [`scripts/qt-public/README.md`](../../scripts/qt-public/README.md). On
QNX 8.0.5 the loader reads `LD_LIBRARY_PATH` before a `DT_RUNPATH`, so putting
your library directory first on `LD_LIBRARY_PATH`, or copying your files over
the ones in the runtime directory, selects your build. A build-host `DT_RPATH`
would defeat that, which is why the build writes a `DT_RUNPATH` and the result is
checked with `verify-closure.py` ([packaging](../packaging.md)). This works on the
QNX VM for the gallery app after a rewrite
([receipt](../evidence/distribution-2026-11-21/README.md)) and for the playground
app with a runtime built that way
([receipt](../evidence/distribution-2026-11-22/README.md)). For a device that
only accepts signed software, LGPL-3.0 section 4 and GPL-3.0 section 6 also
require the installation information; the owner has to decide how a release
supplies it.

## How to ask

Open an issue titled "Qt source request" at
`https://github.com/mikevocalz/react-native-qnx/issues`, or write to the
contact below. Say which release you received (its package manifest hash).

Contact: **to be filled in by the owner before any distribution.**

## How long the offer stands

For at least three years from the date you received the runtime, and for as
long as the project distributes that release or supports it, whichever is
longer. This follows GPL-3.0 section 6(b), which LGPL-3.0 incorporates. The
owner should confirm the wording.

## Not part of the offer

The QNX SDP (compiler, sysroot, headers and system libraries such as
`libscreen` and `libEGL`) is licensed separately by QNX and is not in the
runtime or in this bundle ([ADR-0018](../adr/0018-runner-hosting-and-sdp-boundary.md)).

## FreeType credit

Portions of this software are copyright © 2025 The FreeType Project
(https://freetype.org). All rights reserved.
