# Hardware-free release work, 2026-11-22

Baseline: `ae0040d34343e5efc95017e655efd594fda6cc4a`, followed by the tested
PR #218 continuation at `20a631fd0001fb02e75b9b66014371ea5c9b3da5`.
The owner requested all work possible without physical hardware. The release
board and licensed hosted runners are unavailable. No gate is waived here.

## Completed software work

| Area               | Implemented and checked                                                                                                                                                                                                                                                                  |
| ------------------ | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Qt build ownership | Disk guard starts a separate process group, refuses low space before launch, terminates owned descendants, handles launch-time and repeated interruption, and preserves unrelated processes. Four process tests pass on macOS and Linux                                                  |
| Runtime packaging  | A provisional manifest generator verifies actual source/recipe hashes, applied patch variant, successful stage sequence, staged/build plugin equality, target ELF architecture and a fresh runtime closure. Twelve fixture groups pass; no actual runtime is qualified by these fixtures |
| Loader closure     | Install-prefix directory boundaries and normalized paths replace a raw string-prefix check. Four groups exercise 27 real ELF cases, including traversal and prefix-collision rejection                                                                                                   |
| Expo example       | Public Router tabs and a per-root Zustand store replace the empty example. Actual production Metro bundling passes                                                                                                                                                                       |
| Expo CLI           | An exact SDK autolinker host-identity patch permits QNX Metro setup while unsupported native linking remains rejected. Actual Expo-owned development bundling, watching, source/bundle restoration, reload and cancellation cleanup pass                                                 |
| Consumer creation  | Generated apps retain all three reviewed source/security patches, including the Expo host fix. Unknown or duplicate patch inventories are refused                                                                                                                                        |
| Documentation      | Isolated committed-source snapshots build at frozen candidate paths, record every file hash, reject overwritten outputs, and regenerate source links for the selected revision. Hosted CI now builds and retains a snapshot                                                              |
| Native window API  | The experimental implementation, local test scope and remaining linked/target checks are recorded in `window-api-contract-2026-11-22.md` and the window software evidence receipt. The feature remains default OFF                                                                       |

The root locked install, dependency contracts and peer check pass. The registry
security audit reports zero advisories across 646 dependencies. The complete
JavaScript suite, linked native ON/OFF graphs and exact-head hosted results must
be checked on the final continuation; local component checks do not replace them.

## Actual frozen documentation build

The committed documentation continuation at
`85b24feaaa26837441b4e8082a1799998fa54606` passed isolated `npm ci`, generated-data
validation/regeneration, Next build and content checking. All **1,504 exported
file hashes** were independently read back and verified. Its frozen base path is
`/react-native-qnx/v/candidate-85b24feaaa26` and receipt SHA-256 is
`9c402a0f7b197bc3ac98f9b1c7f8bd5ac3f5944c096154819ce7a6a87ef4d837`.
The output is local, not a publication. The later integrated native continuation
needs its own hosted documentation build, which the workflow now performs.

## Remaining release boundaries

The readiness record still has one passed gate and ten pending. The software
receipts above establish narrower prerequisites:

- Distribution needs the final pristine Qt build, actual runtime/plugin
  acceptance, source-offer owner review, publication and independent reproduction.
- Candidate CI needs all required workflows on one candidate, including the
  authorized QNX cross-build and VM lanes. The repository currently has no
  registered self-hosted runners.
- Cold boot needs 20 supervised physical power cycles. Component/input parity
  and QNX performance require the named release board and actual measurements.
- Clean developer, identity, window and Expo gates retain their specified
  target acceptance. Published development runners and versioned documentation
  also need actual reviewed release assets; local tarballs are not publication.

An ordinary fresh recipe workspace was prepared in the existing licensed build
container. Its compiler successfully linked a temporary target probe without
changing licensing or container identity. A full Qt build is not started while
free internal space is insufficient to preserve the build guard's 4 GiB floor.
The external workspace mount needs separate authorization because automatic
approval review rejected privileged shared-container namespace changes.

## Scope and rollback

No physical device, boot configuration, established window dimensions or
orientation was changed. No SDK or system libraries are added to release units.
The draft source offer remains draft. No merge or release publication is made
by these host checks. Revert individual software commits to roll back; preserve
historical receipts and any subsequently published immutable documentation.
