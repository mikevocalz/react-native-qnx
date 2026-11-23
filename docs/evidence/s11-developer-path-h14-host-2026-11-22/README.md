# `developer-path` H14 and deploy-bundle: host run, 2026-11-22

Label: **host** (macOS 27 arm64, Node 26.13.0, pnpm 12.21.0). **The `developer-path`
gate stays `pending`.** `docs/releases/v1-readiness.json` is unchanged.

Ran `scripts/ci-host-path-receipt.sh`: `pnpm qnx templates pack`, `pnpm qnx init
ci-receipt --template qnx`, `pnpm install --no-frozen-lockfile`, `pnpm qnx bundle-qnx`,
then `pnpm qnx deploy-bundle` (host `hermesc`, then the push through a local stand-in
for ssh and scp). Results are in [receipt.json](receipt.json),
[templates-manifest.json](templates-manifest.json) and [deploy-bundle.log](deploy-bundle.log).

| Item                                                      | Result                                                                                            |
| --------------------------------------------------------- | ------------------------------------------------------------------------------------------------- |
| H14 template tarballs, `qnx` 1.0.0 and `qnx-router` 1.0.0 | Packed; `contentSha256` equals `template/templates.lock.json`; two packs gave equal `.tgz` hashes |
| `init` prints version and hash                            | Yes                                                                                               |
| Bytecode on the stand-in target equals the local `.hbc`   | Yes, SHA-256 `1b4de609...a82eb0` on both                                                          |

## Not run

- QNX VM, Pi 400 and release board runs of `deploy-bundle`. The push used a local copy, not a real ssh session.
- Any QNX native build and the H21 Linux/amd64 licensed cross-build.
- A first-time licensed developer run. This host already had Qt, CMake and a warm pnpm store.
- npm publication and the package-name decision.
