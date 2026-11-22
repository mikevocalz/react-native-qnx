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
| Consumer creation  | Generated apps retain all four reviewed source/security patches, including the Expo host and pinned Nitro release receiver fixes. Unknown or duplicate patch inventories are refused                                                                                                     |
| Documentation      | Isolated committed-source snapshots build at frozen candidate paths, record every file hash, reject overwritten outputs, and regenerate source links for the selected revision. Hosted CI now builds and retains a snapshot                                                              |
| Native window API  | The experimental implementation, local test scope and remaining linked/target checks are recorded in `window-api-contract-2026-11-22.md` and the window software evidence receipt. The feature remains default OFF                                                                       |

Reviewed functional changes from PR #219 are adapted: cached linker flags,
unique attempt logs and extracted-bundle application toolchains. Four host
groups cover effective flags, real `try_compile`, retained logs and failed
attempt creation. The existing pinned tooling workflow runs these regressions;
the parallel PR's unpinned workflow is not copied.

The root locked install, dependency contracts and peer check pass. The registry
security audit reports zero advisories across 646 dependencies. The complete
JavaScript suite, linked native ON/OFF graphs and exact-head hosted results must
be checked on the final continuation; local component checks do not replace them.

The complete ordered JavaScript suite exposed inherited React loader mocks in
the new inset fixture. The fixture now runs unchanged in a fresh Node worker.
The exact old ordering fails its negative control; the isolated ordering and
the complete suite pass. No assertion is skipped or weakened.

Additional compile-only checks produced QNX aarch64 objects for the new window
bindings, observer and host installer using exact Nitro alpha136/JSI headers.
The observer uses existing release-profile warning demotions for two dependency
diagnostics. No new suppression or source change was introduced. See the window
evidence receipt; linked runtime acceptance remains separate.

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

A pristine Qt software-plugin build is now running in an isolated task container
with fresh source/build/log directories on external storage. The official QNX
compiler successfully linked a target probe there after restoring the normal
configuration symlink layout. The SDK is read-only; the container retains its own
Docker-assigned identity. No host namespace mount, identity change or license
content change is used. The earlier privileged shared-container mount proposal
is no longer needed. Build completion, actual runtime staging and closure results
must be recorded before claiming this distribution prerequisite passed.

The final architecture validator now checks the implemented Expo example's
entry points instead of its removed placeholder. The root code-generation gate
also verifies the constrained window bindings. These changes preserve the native
generated output and close failures exposed by the integrated hosted run.

## Scope and rollback

No physical device, boot configuration, established window dimensions or
orientation was changed. No SDK or system libraries are added to release units.
The draft source offer remains draft. No merge or release publication is made
by these host checks. Revert individual software commits to roll back; preserve
historical receipts and any subsequently published immutable documentation.

## Actual runtime and release receiver follow-up

The pristine software-plugin Qt build at recipe `61d0f257` completed all seven
stages. Actual SPDX audits, check-only RUNPATH verification and fresh closure
checks pass. The follow-up packaging recipe at `1b3b8ce5` preserves all thirteen
compiler-read source files flagged by the audit, retaining embedded copyright
notices alongside unchanged upstream metadata. All 26 ELF hashes remain
identical; the corrected package remains provisional and unpublished.

The QNX VM also exposed a release-only Nitro alpha136 receiver crash after
`dispose()`. A narrowly reviewed patch keeps existing receiver checks active in
release builds without changing the version or ABI. The original unchanged VM
probe crashed before the patch and passes in Default and Hidden modes after
it. An isolated real-Hermes release fixture reproduces the original crash and
verifies invalid-receiver rejection. See the
[Nitro receiver receipt](../evidence/nitro-release-receivers-2026-11-22/README.md)
and [standalone consumer receipt](../evidence/nitro-release-consumer-2026-11-22/README.md).
These are scoped software/VM checks; final-head CI and named-board acceptance
remain separate. The compiled Qt recipe and corrected packaging recipe retain
their own immutable source identities.
