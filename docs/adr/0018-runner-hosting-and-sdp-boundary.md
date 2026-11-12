# ADR-0018: Runner hosting, signing and the QNX SDP boundary

## Status

Accepted, 2026-11-12. The owner delegated this decision. No runner has been
built or published yet. QNX's redistribution terms for SDP files have not been
checked; this ADR avoids needing them.

## Context

`rnqnx runner fetch` downloads a prebuilt development runner listed in an
`rnqnx-runner-manifest/v1` manifest and checks its size and SHA-256
([packaging](../packaging.md#runner-manifest-format)). Four owner items were
open: the publication host, how entries are signed or attested, the manifest
URL, and whether a QNX runner archive may carry QNX SDP runtime files.

On the Pi 400, a runner process loads three groups of shared objects (closure
manifest, `device_loaded_objects`):

1. Project-built code: the runner executable, `libhermesvmlean.so`, and the
   public-source Qt libraries and plugins under LGPL-3.0.
2. QNX system libraries from the board image: `libc.so.6`, `libc++.so.2`,
   `libm.so.3`, `libgcc_s.so.1`, `libscreen.so.1`, `libEGL.so.1`,
   `libGLESv2.so.1`, `libslog2.so.1`, `libsocket.so.4`, `libz.so.2`,
   `libzstd.so.1.5.6`, `libeventfd.so.1`, `libfsnotify.so.1`.
3. The Mesa/libdrm overlay under `/data/pi400-graphics-compat`, taken from QNX
   Software Center packages.

Groups 2 and 3 come from QNX. Their redistribution terms are set by QNX's
licence, which this project has not reviewed and cannot settle for itself.
No code in this repository puts them in an archive today, but nothing stopped a
future runner archive from doing so.

The v1 client refused every redirect. GitHub Release asset URLs answer with a
302 to `release-assets.githubusercontent.com` (or
`objects.githubusercontent.com`), so a manifest on GitHub Releases could not be
fetched.

## Decision

1. **Hosting.** Runner archives and the manifest are assets of GitHub Releases
   on `mikevocalz/react-native-qnx`. Each runner is a release; the manifest is
   an asset named `rnqnx-runner-manifest.json`. Once the first runner is
   published, the default manifest URL becomes
   `https://github.com/mikevocalz/react-native-qnx/releases/latest/download/rnqnx-runner-manifest.json`.
   Until then `fetch` keeps requiring `--manifest` or `RNQNX_RUNNER_MANIFEST`.
2. **Redirects.** The client follows a redirect only when the current URL is
   an https `github.com/<owner>/<repo>/releases/download/...` or
   `.../releases/latest/download/...` path and the next URL is either another
   such path on `github.com` or an https host under `githubusercontent.com`,
   at most five hops. All other redirects are still refused.
3. **Integrity and signing.** Three layers:
   - The manifest's per-archive `sha256` and `size`, which `fetch` already
     verifies before keeping a download.
   - A `SHA256SUMS` asset covering the archive and the manifest.
   - GitHub artifact attestations (`actions/attest-build-provenance`) for the
     archive and the manifest, made by `.github/workflows/runner-release.yml`.
     A developer checks one with
     `gh attestation verify <file> --repo mikevocalz/react-native-qnx`.
     `fetch` does not call `gh`; the check is a documented manual step.
     The attestation records the workflow run and commit that published the
     files. QNX runners are built on a machine with a licensed SDP, outside
     GitHub-hosted runners, so the attestation does not prove which machine
     compiled the archive. The release notes for each runner name the source
     commit, lockfile hash and Qt runtime hash it was built from.
4. **No QNX SDP files in any runner archive.** A QNX runner archive contains
   only project-built binaries, the public-source Qt libraries and plugins, and
   the Qt/LGPL notices, corresponding-source pointers and attribution files
   that [packaging](../packaging.md#material-to-ship-with-a-runtime-built-this-way)
   requires. QNX system libraries, the Mesa/libdrm overlay, sysroot files and
   anything else from the SDP or a QNX target image are left out. The
   developer's own licensed SDP and target image provide them.
   Redistributing SDP files stays out of scope until QNX's terms have been
   checked by the owner; this ADR is not a legal opinion and makes no claim
   about what those terms allow.
5. **Enforcement in code.**
   - Every QNX manifest entry must list `targetLibraries`: the QNX libraries
     the runner loads from the board.
   - After the hash check, `fetch` lists the tar.gz without extracting it and
     refuses a QNX runner whose archive contains a file named like a QNX SDP
     or target-image library (`libc`, `libc++`, `libscreen`, `libEGL`,
     `libGLESv2`, `libslog2`, `libsocket`, `libz`, `libzstd`, `libdrm`,
     `*-mesa.so`, `*_dri.so` and others in `QNX_SDP_FILE_PATTERNS`) or any
     name in its own `targetLibraries`. Absolute paths and `..` are refused for
     every target. A refused download is deleted.
   - `fetch --target-libs <file>` compares `targetLibraries` with a listing
     taken on the board (`ls -1 /usr/lib /lib /proc/boot`) and fails with the
     missing names. Without the flag, the output says
     `targetLibrariesChecked: false`. `fetch` still never contacts a device.
   - `scripts/verify-runner-archive.mjs` runs the same content check in the
     release workflow before anything is attested.

## Consequences

- A developer needs a QNX target image that contains the listed libraries.
  The Pi 400 board used for the receipts had the group 2 libraries in
  `/usr/lib` and `/proc/boot`. The Mesa/libdrm overlay in group 3 lives under
  `/data/pi400-graphics-compat`, outside the image; a Pi 400 developer installs it
  from QNX Software Center under their own account, as the bring-up did. The
  runner release notes must say so.
- The "Needs the SDP: No" column for the prebuilt runner means no SDP on the
  development host. A licensed QNX target image is still required.
- The manifest is trusted over TLS to github.com. A compromised repository
  could publish a bad manifest and matching archive; the attestation check
  catches files not published by this repository's workflow, and only when the
  developer runs it.
- `targetLibraries` is a new required field for QNX entries in the v1 schema.
  No v1 manifest has been published, so no published file breaks.

## Alternatives considered

- **A separate CDN or object store.** Rejected: GitHub Releases is free for
  this public repository, sits next to the source the LGPL corresponding-source
  offer points to, and supports attestations without another signing key.
- **Signing with a project key (minisign, cosign with a stored key).**
  Rejected for now: it adds key custody and rotation work for one maintainer.
  Keyless attestations tie the files to this repository's workflow identity.
- **Following all https redirects.** Rejected: the manifest is the root of
  trust, and an open redirect anywhere on the path would let another host
  supply it.
- **Shipping the QNX libraries for convenience.** Rejected until QNX's terms
  are checked (item 4).

## Related files

- `packages/react-native-qnx-cli/src/runner.mjs`
- `tests/js/qnx-runner-manifest.test.mjs`
- `scripts/verify-runner-archive.mjs`
- `.github/workflows/runner-release.yml`
- `docs/packaging.md`

## Notes

Release steps for a runner, once one is built:

1. Build the runner and stage it with the Qt runtime from an ADR-0017 (`-O2`)
   build and the licence material; leave out every QNX file.
2. Write the manifest entry with `sha256`, `size` and `targetLibraries`.
3. Create a draft release, upload the archive and `rnqnx-runner-manifest.json`.
4. Run the "Runner release attestation" workflow with the tag and archive
   name. It checks the archive, attests it and the manifest, and uploads
   `SHA256SUMS`.
5. Publish the release, then set the default manifest URL in `runner.mjs`.

An archive built on a self-hosted runner could be attested in the same job
that compiles it. That moves the attestation closer to the build and can
replace step 4 when such a runner exists.
