## Why / target

Describe the MarathonOS capability and its precise QNX regression surface.

## Change inventory / architecture boundary

Files, new public API, JS Metro platform name, Qt/CMake target flags, Marathon shell entry/permission requirements, alternatives rejected.

## Skill execution proof

| Exact skill | SHA/version | Scope | What changed | Artifact | Rejected finding and why |
| --- | --- | --- | --- | --- | --- |
| | | | | | |

## npm and native ABI

- [ ] npm stable tags and alpha/beta channels verified with timestamp
- [ ] Expo 58 tested RN/React and compiled Hermes source/provenance inspected
- [ ] Nitro and Nitrogen release matches verified; v1 alpha is not stable
- [ ] Expo Modules API v2 beta is not confused with package major v2
- [ ] Exact lockfile regenerated; frozen install and license/security audit pass

## Evidence — do not cross-label tests

| Test | Environment | Command | Result + log/artifact | Missing proof |
| --- | --- | --- | --- | --- |
| QNX Metro | host | | | |
| Marathon Metro | host | | | |
| Qt/Fabric/Hermes | host | | | |
| Wayland xdg-shell | Linux/QEMU | | | |
| Marathon app launch | QEMU/hardware | | | |
| QNX target | QNX SDP/VM/Pi | | | |
| Permission denied | Marathon | | | |
| Suspend/resume | Marathon | | | |

## Security, performance and limitations

List bwrap identity, trusted D-Bus permission checks, capabilities requested/denied, code signing, memory/frame metrics, hardware gaps and regressions.

## Upstream signoff, rollback and follow-up

Upstream Marathon maintainer decision, clean revert, next PR, and whether this remains draft. Never declare official/out-of-box support without upstream proof.
