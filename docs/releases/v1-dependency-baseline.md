# v1 dependency baseline

The `dependencies` gate in [v1-readiness.json](v1-readiness.json) passes when
every **Bump** row below has landed, every **Hold** row has its reason in the
[compatibility matrix](../getting-started/compatibility-matrix.md), and the
`pnpm outdated -r` output for the candidate is attached with each remaining
item bumped or held with a reason. Checked against npm and download.qt.io on
2026-11-11.

| Dependency                                       | Pinned at baseline      | Latest checked                            | Action   | Reason                                                                                                                             |
| ------------------------------------------------ | ----------------------- | ----------------------------------------- | -------- | ---------------------------------------------------------------------------------------------------------------------------------- |
| Qt (device)                                      | 6.12.0 evaluation       | 6.12.0                                    | Rebuild  | Gate `distribution`: build 6.12.0 from the public source archive; check for 6.12.1 before the release candidate                    |
| Qt (host floor)                                  | 6.11.1                  | 6.11.3 / 6.12.0                           | Decide   | Owner decision: align host tests to 6.12.0, or raise the floor to 6.11.3 and record the mismatch                                   |
| react-native                                     | 0.88.1                  | 0.88.1 (0.89.0-rc.1 on `next`)            | Hold     | v1 is the 0.88 line                                                                                                                |
| react                                            | 19.3.0                  | 19.3.1                                    | Bump     |                                                                                                                                    |
| expo                                             | 58.0.22                 | 58.0.24                                   | Bump     | Lands with expo-modules-core                                                                                                       |
| expo-modules-core                                | 58.0.29                 | 58.1.0                                    | Bump     | Required by expo 58.0.24; re-run `validate:qnx-expo-modules` and the autolinking checks                                            |
| metro                                            | 0.87.1                  | 0.88.0                                    | Hold     | react-native 0.88.1 requires `^0.87.1`                                                                                             |
| hermes-compiler                                  | 260318099.0.5           | 260318099.0.7                             | Hold     | react-native 0.88.1 pins `.0.5` exactly; `.0.7` belongs to 0.89                                                                    |
| react-native-nitro-modules, nitro core, nitrogen | 1.0.0-alpha.nightly.105 | 1.0.0-alpha.nightly.136 (`alpha-nightly`) | Bump     | Audit the diff against [NITRO_V1_API_POLICY.md](../platform-modernization/NITRO_V1_API_POLICY.md); stable 0.37.2 is not the target |
| typescript                                       | 7.0.2                   | 7.0.2 (7.1.0-beta)                        | Hold     | No stable release newer than the pin                                                                                               |
| pnpm                                             | 12.8.1                  | 12.18.0                                   | Bump     | `packageManager` and engines; lockfile regenerated in its own PR                                                                   |
| Node                                             | >=26.0.0                | 26.13.1 LTS                               | Bump     | Pin CI to the current 26 LTS                                                                                                       |
| QNX SDP                                          | 8.0.5                   | 8.0.5                                     | Re-check | Confirm against the SDP 8.0 and QNX Everywhere release notes before the release candidate                                          |
