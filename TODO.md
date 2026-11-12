# Ordered work queue: v1 gates

V1 has eleven gates. [v1-readiness.md](docs/releases/v1-readiness.md) holds the
acceptance text and status, generated from
[v1-readiness.json](docs/releases/v1-readiness.json). This file only orders the
work and names an owner role for each gate. When the two disagree, the readiness
record wins.

Gates 1 to 7 are on `main`. Gates 8 to 11 (`app-identity`, `window-chrome`,
`expo`, `dependencies`) arrive with
[PR #115](https://github.com/mikevocalz/react-native-qnx/pull/115); until it
merges, `pnpm validate:docs` reports seven gates.

Owner roles come from the [contributor roster](docs/marathon/ROSTER.md). Roles are
responsibilities; each PR names its own DRI and an
independent reviewer.

## Gates

All eleven are `pending` as of 2026-11-11. "Device" means the gate cannot pass
without a run on the named board; host work can still land first.

| #   | Gate id          | Title                              | Owner role                                                 | Needs a device | Next step                                                                                                                       |
| --- | ---------------- | ---------------------------------- | ---------------------------------------------------------- | -------------- | ------------------------------------------------------------------------------------------------------------------------------- |
| 1   | `distribution`   | Open-source runtime packaging      | Release / dependency engineer                              | No             | Build Qt from the public source archive and record module/QPA notices, hashes and the library/plugin closure                    |
| 2   | `ci`             | Candidate CI                       | Security / QA engineer                                     | No             | Define the required checks ([PR #116](https://github.com/mikevocalz/react-native-qnx/pull/116)), then run them on the candidate |
| 3   | `cold-boot`      | Repeatable graphics cold boot      | QNX regression owner                                       | Yes            | 20 consecutive supervised cold boots on the named release board and image, with logs                                            |
| 4   | `device-parity`  | Device component and input results | QNX regression owner, Qt Quick / Fabric engineer           | Yes            | A disposition for every component on stock Pi 4 QSTI; keyboard and touch proof                                                  |
| 5   | `performance`    | QNX performance measurements       | Security / QA engineer                                     | Yes            | Startup-to-first-frame, presented-frame intervals and RSS for Hello World, the 1000-row list and drawer drag                    |
| 6   | `developer-path` | Clean-host app workflow            | Documentation / DX engineer, Metro / Expo tools engineer   | Yes            | A new licensed developer creates, builds, deploys and runs an app from the docs alone                                           |
| 7   | `documentation`  | Versioned v1 documentation         | Documentation / DX engineer                                | No             | Review the [v1 docs set](docs/v1/README.md) and publish it at a versioned location                                              |
| 8   | `app-identity`   | App identity: icon and name        | Metro / Expo tools engineer, Qt Quick / Fabric engineer    | Yes            | Icon and short-name pipeline from Expo config, with measured tile-label fit                                                     |
| 9   | `window-chrome`  | Window chrome and windows API      | Qt Quick / Fabric engineer, Nitro 1.x migration engineer   | Yes            | QML title bar outside the Fabric root, a minimize ADR for QNX Screen, and the Nitro v1 windows API                              |
| 10  | `expo`           | Expo readiness                     | Metro / Expo tools engineer, Expo Modules API 2.0 engineer | Yes            | Prebuild from `expo-qnx-config-plugins`, a prebuilt runner, and Expo Router on device                                           |
| 11  | `dependencies`   | Packages updated                   | Release / dependency engineer                              | No             | Land every Bump row in the dependency baseline that PR #115 adds, and attach `pnpm outdated -r`                                 |

Gates 1, 2, 7 and 11 can pass on hosts and hosted runners. The other seven need
the release board; for `device-parity` that is stock Pi 4 QSTI, kept separate
from the custom Pi 400 image.

## Order

1. `ci` and `dependencies`. Every later receipt has to cite a candidate that
   passed the required checks on the updated graph.
2. `distribution`. The release candidate needs a public-source Qt build before
   any device receipt can describe the shipped runtime. The evaluation Qt used by
   the Pi 400 receipts expires on 2026-11-17.
3. `cold-boot`, then `device-parity` and `performance` on the same board and
   image.
4. `app-identity`, `window-chrome` and `expo`. Host work can start now; their
   device receipts follow step 3.
5. `developer-path`, run by someone who did not write the docs.
6. `documentation` last, so the published set describes the candidate that
   passed the other gates.

## Carried over from the G0 queue

These items predate the gate list and are still open. They feed the gates in
brackets.

- [ ] Run the C++20 probe binary on a stock Pi 4. The recorded run is on the
      Pi 400 ([receipt](docs/evidence/pi400-hermes-2026-11-06.txt)).
      [`device-parity`]
- [ ] Record the QSTI image package/build and its SDP compatibility.
      [`device-parity`]
- [ ] Measure a rebuilt hello-world round trip below 30 seconds.
      [`developer-path`]
- [ ] Repeat the smoke path on the licensed QEMU QSTI environment. [`ci`]
- [ ] Supply the permanent npm organization that replaces provisional
      `@rn-qnx`. [`developer-path`]
- [ ] Define package manifests and release units before enabling
      release-please or per-package changelogs. [`distribution`]

Dropped from the old queue because they are done: the named SSH user and key
setup and the deploy/log scripts ([build and deploy](docs/getting-started/build-and-deploy.md)),
CODEOWNERS (`.github/CODEOWNERS`), and the Nitro version conflict (ADR-0008).
The Qt licensing questions are closed by
[ADR-0013](docs/adr/0013-open-source-qt.md). The full G0 list is in this file's
git history and in [the dated handoff history](docs/handoff/HISTORY_THROUGH_2026_11_07.md).
