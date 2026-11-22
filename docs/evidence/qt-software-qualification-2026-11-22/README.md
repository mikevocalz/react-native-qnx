# Hardware-free qualification receipts

These receipts record actual software checks and local artifact assembly. They do not close a release gate, publish a runtime or website, or establish physical-device qualification. The runtime remains provisional with `releaseEligible: false`; source-offer approval/contact, publication and independent reproduction remain pending.

| Evidence                 | Exact source                               | Actual result                                                                                                                             |
| ------------------------ | ------------------------------------------ | ----------------------------------------------------------------------------------------------------------------------------------------- |
| Qt compilation           | `61d0f25786039aef16a31cb7ca94bc6c03c7b6d1` | Clean-tree preflight and seven ordered stages passed; total stage time 3,902 seconds.                                                     |
| Corrected notice staging | `1b3b8ce56e63b80cf8a8c7021b5969c1d6e51189` | Fresh closure passed all seven checks; RPATH check required zero changes. All 26 ELF hashes match the original build.                     |
| Frozen documentation     | `acb5c849b927a252c7ce7471cf2cf98e13f3444f` | Untouched workflow artifact verified and locally combined with 1,503 unchanged live files; 1,505 candidate files verified. No deployment. |

The [Qt result](result.json) separates compile and packaging provenance: only `stage-runtime.py` differs between those recipes. No Qt rebuild is claimed. Corrected staging preserves all 13 audited source-notice files verbatim and retains all 146 previous upstream license texts. The [notice coverage](../source-notice-coverage-2026-11-22/result.json) records the original Khronos-year and John Layt discrepancies and their byte-preserving correction. Upstream license classifications and the draft source offer remain unchanged.

The [Pages receipt](../pages-preservation-acb5-2026-11-22/result.json) identifies the downloaded artifact, live deployment and immutable candidate. Durable snapshot retention and reviewed deployment remain outstanding. Earlier candidate receipts remain historical.

The stage table measures `/build/final`; its free-space figures are actual `df`/`statvfs` values reported by the `/work` virtiofs mount, not direct host-disk measurements. These Qt checks do not establish application/VM acceptance or supervised physical cold-boot results.

The [inventory](public-evidence-inventory.json) hashes every copied evidence file and records formatting or summary changes. This set contains text metadata, hashes and short logs only. Runtime binaries, source archives, core dumps, SDK payloads, identity files and license configuration remain excluded; underlying release units remain external and unpublished. Original qualification artifacts are unchanged.
