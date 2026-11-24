# Publish a runner release

Host procedure. Nothing here runs on the Pi 400, and nothing here has been run
against GitHub: the script is tested with a stand-in `gh` that records its
arguments.

Creating a runner release is the owner's decision (ADR-0018). Packing is
automatic and safe. Creating the release is gated twice so an agent or a stray
command cannot do it.

## 1. Pack

```bash
SOURCE_DATE_EPOCH=<seconds> rnqnx runner pack --stage <dir> --out <out> --id <id> \
  --target qnx-aarch64-pi400 --hermesc <hermesc> --hermes <v> --qt <v> --sdp <v> \
  --builtins <json> --target-libs <file>
```

`<out>` then holds `<id>.tar.gz`, `rnqnx-runner-manifest.json` and
`rnqnx-runtime.json`. The same stage and inputs give the same bytes. Details are
in [packaging](../packaging.md#runner-runtime-descriptor).

## 2. Dry run (the default)

```bash
node scripts/publish-runner-release.mjs --dir <out> --tag <tag>
```

The script checks the manifest, checks that the archive matches the entry's
`sha256` and `size`, applies the SDP content policy that `runner fetch` applies,
then prints the SHA-256 of every asset and the exact command it would run:

```text
gh release create <tag> --repo mikevocalz/react-native-qnx --draft --title 'Runner <id>' --notes '...' <out>/<id>.tar.gz <out>/rnqnx-runner-manifest.json <out>/rnqnx-runtime.json
```

It creates nothing and always exits 0 when the checks pass.

## 3. Create the draft (owner only)

```bash
RNQNX_RELEASE_OWNER_APPROVED=<tag> node scripts/publish-runner-release.mjs \
  --dir <out> --tag <tag> --confirm
```

Both are required. `--confirm` alone exits 1 with a message. The variable must
equal the tag, so approval for one tag does not carry over to another. A tampered
archive, a manifest that fails validation or a QNX archive that holds SDP files
stops the script before `gh` runs.

The release is a draft. The script does not push tags, publish the draft, run the
attestation workflow or publish to npm.

## 4. After the draft exists

Follow the remaining [ADR-0018](../adr/0018-runner-hosting-and-sdp-boundary.md)
steps: run the "Runner release attestation" workflow with the tag and archive
name, publish the draft, then set the default manifest URL in `runner.mjs`.

## Tests

`tests/js/runner-pack-cli.test.mjs` runs `rnqnx runner pack` on a synthetic stage,
compares bytes across two runs, and covers the dry run and each refusal.
