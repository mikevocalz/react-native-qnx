# QNX SDP 8.0.5 — November 2, 2026 upgrade / bring-up queue

**Status:** documented public release evidence, not proof that these components have been installed or run on the user's machine. The main repository targets SDP 8.0.5 and QSTI 0.5.0 build 50 but has not captured a full Hermes + RN + Fabric + Qt Quick runtime running on physical QNX hardware.

## What is actually new and worth doing

| Item                             | Latest verified public source             | Action for react-native-qnx                                                                 |
| -------------------------------- | ----------------------------------------- | ------------------------------------------------------------------------------------------- |
| QNX SDP                          | **8.0.5** (June 25, 2026)                 | Keep baseline, update QSC installed packages, verify qcc/q++ and SDK headers                |
| QNX Everywhere QSTI Pi 4/5       | **0.5.0 build 50**, September 16 update   | Use matching Pi 4/5 image; do not mistake build 50 for a downgrade from old 0.4.x build 297 |
| RPi4 BSP                         | **build 670**, September 17               | Refresh when building a BSP/CTI, verify screen, input, console, networking                  |
| RPi5 BSP                         | **build 568**, September 17               | Refresh when building a BSP/CTI, confirm experimental maturity warning                      |
| QNX USB/HID                      | **build 19**, October 6                   | Apply and rebuild relevant USB/HID binaries when testing keyboard/touch/camera              |
| Connectivity char/serial/io-char | **builds 61/52/52**, September            | Install matched components per BSP notes                                                    |
| OpenSSH                          | **build 9**, September 14, OpenSSH 10.4p1 | Verify sshd-auth / sshd-session packaging before changing VM/Pi target images               |
| QNX Everywhere graphics samples  | **build 343**, September 16               | Use independent Screen tests for GPU/display diagnosis                                      |
| libarchive / BSD tar             | **build 91**, September                   | Optional operational/security update                                                        |

Authoritative release URLs and package IDs/internal versions are machine-readable in `release-matrix-2026-11-02.json`. QNX Software Center is the final authority for packages available under each license; online docs can lag or exclude entitled-only components.

## Source-of-truth requirements

1. Use **QNX SDP 8.0.5** on a documented supported **x86-64** host: Windows 11 / Ubuntu 22.04 or 24.04 / RHEL 9. Official SDP does not support macOS or ARM host installation.
2. Source the license-backed `qnxsdp-env.sh`; confirm target arch and `screen.h` symbol use; do not reuse old SDP 8.0.2 historical headers/toolchain as current proof.
3. Boot RPi4 or RPi5 QSTI 0.5.0 build50, independently verify Screen, supported GPU path, touch, keyboard, SSH and networking. QNX documents those specific Raspberry Pi models; the user’s Pi 400 lane is **experimental** until independently verified.
4. `pnpm pi:preflight` then `pnpm pi:install`; save 3 clean QNX native launch/exit cycles and `manifest.json` hashes plus stdout, slog2 and screenshots; review actual logs.
5. Run `pnpm qnx:runtime:validate` and full RN/Hermes/Fabric/Qt Quick graph with native dependencies; a Screen-only probe is **not** a React Native runtime demonstration.
6. Rerun RN 0.88, Expo 58, C++/JSI source contracts and Nitro/Expo migration gates after npm PRs land. Do not mark as tested when only CMake sources exist.
7. Run separate Marathon Linux/Wayland tests. The target is Linux, not QNX; do not reuse QNX binaries.

## Special hardware / graphics guidance

- RPi5 BSP is explicitly labeled **experimental software** by QNX. Do not use as certified or production hardened.
- The official QNX SDP 8.0.5 Screen release notes describe image codec and virtual display improvements. Update only tested compatible Screen, base/board support, graphics driver and Qt QPA versions together.
- Qt 6.12 LTS is publicly available, but react-native-qnx requires 6.11.1+ and Marathon's published shell line uses Qt 6.11. Never silently uplift QNX Qt runtime to 6.12 without toolchain cross-build and device pixels/input proof.
- Pi400 is a separate non-QSTI validated hardware experiment (PR #32). No assumption that a Pi4 image supports the built-in keyboard/Screen hardware configuration.
- OpenSSH 10.4p1 requires `sshd-auth` to be included in `mkqnximage` and older BSP images; follow QNX official instructions. Do not change SSH when the device cannot be recovered.

## Host commands to run on owned hardware

```bash
export QNX_SDP_ENV=/path/to/qnxsdp-env.sh
source "$QNX_SDP_ENV"
qcc -V
q++ -V
./scripts/doctor.sh
./scripts/verify-qnx-license.sh
./scripts/validate-qnx-805-cross.sh
pnpm pi:preflight
pnpm pi:install
pnpm qnx:runtime:validate
pnpm pi:evidence:verify
```

All tests are to be run **on a licensed QNX host and target**. These commands are an execution recipe, not evidence that they passed.

## Why no runtime C++/Screen changes in this PR?

The most valuable immediate work is a reproducible, publicly sourced release baseline and a strict proof checklist rather than guessing vendor binary contents. Applying the QSC updates happens in the developer's licensed QNX installation and physical device; a git commit cannot install vendor binaries. Runtime changes belong in follow-up PRs after device trace feedback.

## Milestone exit criteria

- [ ] QNX SDP and package identities recorded from actual QSC installation
- [ ] Pi4 build670 / Pi5 build568 BSP versions checked against each board
- [ ] QSTI 0.5.0 build50 boot and Screen surface screenshot logged
- [ ] USB/HID, keyboard and input tested
- [ ] Network, SSH and deploy rollback verified
- [ ] RN 0.88 / Hermes / Fabric / Qt Quick real screen first frame and touches
- [ ] Nitro 1 alpha and Expo Modules API 2.0 beta exercised separately without fake support claims
- [ ] CM5/Marathon Linux and QNX Pi evidence stored independently
