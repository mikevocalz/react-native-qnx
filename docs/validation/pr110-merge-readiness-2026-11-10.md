# PR 110: integration validation handoff

The initial 2026-11-10 review covered implementation commit
`083c4bdb887a844e533313fed9b967ff94f4c4d3`, originally based on PR #104 at
`3b414f185ec9b6c23eb7fbfdbc0afc9bfed18b94`. The published follow-up
`2657620b0e0184bb72e3fc7d3fc42bac5fe3fb95` incorporates #104 documentation
commit `c5c54a09817c54b24d9c2132267d6fe93905da2d` and the integrator's
host-renderer CI changes. The subsequent merge below adds PR #106 and tests
the combined public API. Each published candidate needs its own hosted checks;
no future commit identity or full native result is inferred here.

## Combined public API: PR #106 into PR #110

The integration is a merge with first parent
`2657620b0e0184bb72e3fc7d3fc42bac5fe3fb95` and second parent
`5e3bb88655d311a7be5f905b71af06abca9145b6`, the published PR #106 head.
It preserves both histories and does not incorporate PRs #105, #107 or #109.
PR #106 remains independently reviewable; PR #110 now carries that dependency.

The public probe retains both `I18nManager` layout-direction validation and
the Image API decode probe. The Image wrapper and its Promise/callback/error
assertions are retained from #106. The Switch combines #106's nested style
array composition with #110's controlled `Commands.setValue` reconciliation.

The initially merged public-entry test failed with
`Unexpected Switch dependency react-native/Libraries/Utilities/useMergeRefs`.
Its old React double did not model the hooks or merged ref used by #110.
The style, tint, event and ref assertions now run in the existing
`qnx-controlled-switch.test.mjs` retained-hook harness, alongside accepted,
rejected, deferred, rapid, disabled and cleanup scenarios. The styled case
checks exact event identity/order, controlled rejection and ref cleanup;
frozen nested styles retain caller precedence across reconciliation. Image,
public-export/resolver and `I18nManager` assertions stay in the public-entry
test. No failing assertion was waived.

Executed on Linux x86-64 with Node 26.13.0 and pnpm 12.8.1, against the merged
working tree and the unchanged frozen dependency graph:

| Command                                                                 | Result on the combined candidate                                                                                                                                                                                  |
| ----------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `node tests/js/qnx-public-entry.test.mjs`                               | Passed: public exports/resolver, `I18nManager` export and Image loader Promise/callback/error contracts. Host VM/native-loader doubles.                                                                           |
| `node tests/js/qnx-controlled-switch.test.mjs`                          | Passed: style precedence, props/tints, event identity/order, disabled handling, controlled reconciliation and ref cleanup through retained hook/native doubles.                                                   |
| `node tests/js/qnx-public-entry-metro.test.mjs`                         | Passed: real Metro with separate temporary reset caches; QNX 684,840 bytes and Marathon 684,881 bytes. The test removes its temporary bundles; these are bundle checks, not executed device artifacts.            |
| `pnpm typecheck`                                                        | Passed.                                                                                                                                                                                                           |
| Documentation sync/validation, changed-file formatting and merge checks | Passed: `pnpm docs:sync`, `pnpm validate:docs`, changed-file Prettier, local links and `git diff --check`. Prior handoff lines and distinct evidence records are preserved; all seven gate records are unchanged. |
| Full native ON/OFF and public-import host CI                            | Pending for the published merge commit; no fresh native test count is assigned to this merge.                                                                                                                     |

The owner smoke report predates this combined tree and does not identify its
artifacts, flags or scenarios. It cannot qualify the added #106 APIs or their
integration with the controlled Switch. No new QNX cross-build, QNX VM,
physical QNX, actual Marathon/Wayland or Fast Refresh session ran here.
All seven v1 gates remain pending. The original-author native counts and
earlier individual-branch host results below keep their original scope.

Merge #104 and #106 before their dependent #110, inspect the final base/diff,
and require the final candidate's own hosted results. The documentation DRI
also owns this bounded JS/test conflict resolution; the parent integrator
provides independent review and publication.

## Scope and retained behavior of the first follow-up

PR #110 adds opt-in Metro asset staging, Modal keyboard focus, host development
runtime, frame tracing, Qt runtime staging and candidate-bound release
receipts, plus controlled Switch reconciliation and an uninstalled Pi 400
startup script. The Fabric/Yoga/Hermes/Qt Quick architecture is retained.
Native feature options remain OFF by default; Switch reconciliation needs the
matching native implementation even in the default build.

The CI follow-up makes full host-renderer checks run for stacked PRs and
enables input lifecycle, text-input contract, accessibility bridge, desktop
window, Modal focus, development runtime, WebSockets and frame tracing in the
ON matrix entry. The inventory validator checks the required tests for the
enabled features. This expands validation coverage; it does not change the
runtime defaults or establish that the new native job has already passed.

The documentation follow-up updates the six handoff records, this receipt and
the Modal/Switch and development-runtime guides. It preserves the #104 owner
report and adds `evidenceBaseline.pr110` to the readiness source record;
`.project-state.json` is regenerated from that source. All seven release
gates remain pending.

## Original implementation evidence

The [PR #110 body](https://github.com/mikevocalz/react-native-qnx/pull/110),
read on 2026-11-10 at head `083c4bd`, reports the following results. They are
the original author's evidence, not independent reruns on the updated tree.

| Original lane              | Reported result and boundary                                                                                                                                                                                                                                                                                                            |
| -------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| JavaScript/source          | `pnpm test:js` passed 56 files, including real Metro asset staging, CMake tamper fixtures and 23 Pi startup host fixtures. Typecheck, formatting and dependency/docs/architecture/manifest checks passed.                                                                                                                               |
| Linux native, features ON  | 23/23 CTests passed with Modal focus, development runtime, frame trace and WebSockets ON; clang 14, Qt 6.11.1, Hermes and ASan/UBSan.                                                                                                                                                                                                   |
| Linux native, features OFF | 17/17 CTests passed with the default feature configuration on the same host.                                                                                                                                                                                                                                                            |
| QNX cross-build            | Incremental AArch64 build with licensed SDP 8.0 and evaluation Qt 6.12, flags OFF, onto the `3b414f1` tree. The author records abbreviated ELF `60ff3966…` and HBC `d0980214…` hashes; those prefixes are not a complete artifact identity.                                                                                             |
| Pi 400 launch              | Initial launch failed because the Qt Quick Controls closure was missing. Manual app-local Controls/Templates/Basic library and QML staging allowed a fullscreen UID 1000 launch with `RNQNX_SURFACE_READY width=1920 height=1080`. This is reported launch evidence; it does not validate the automated Qt stager or every interaction. |
| Release gate check         | `validate:v1` failed with seven pending gates, as expected.                                                                                                                                                                                                                                                                             |

The native counts excluded `rnqnx_g0_cxx20_probe` and
`qnx_screen_surface_host_test`, which the author could not compile with
clang 14/libstdc++ 11. The author also reported intermittent ASan SEGVs unless
the host used `vm.mmap_rnd_bits=28`, including an unchanged test. Preserve
those limitations when comparing future runs; excluded tests are not passes.
This documentation work changes no kernel setting.

The original report did not run a real state-preserving Fast Refresh edit,
cold boot, stock Pi 4 qualification or performance measurement. Its QNX
build had the optional native features OFF and cannot qualify their ON paths.
Manual deployment repair is separate from automated stager acceptance.

## Checks recorded in the first follow-up

The integration workspace is Linux x86-64 with Node 26.13.0 and pnpm 12.8.1.
The frozen lock SHA-256 remains
`b854ebe2e50a42b2e8e07124df065592d99d43b1d1a3a984b66d3835ad86c3e4`.
React Native remains 0.88.1, Hermes `hermes-v260318099.0.5` and Nitro
core/adapter/generator `1.0.0-alpha.nightly.105.g69d365fc9b18`.
No new native executable, HBC, target sysroot, display mode or device artifact
is produced by these focused checks.

| Command or lane                                                                                | Current result                                                                                                                                   |
| ---------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------ |
| `node tests/js/host-renderer-contract.test.mjs`                                                | Passed, executed by the integrator against the CI follow-up. Validates source/workflow/test-inventory contracts; no native renderer execution.   |
| `node tests/js/qnx-controlled-switch.test.mjs`                                                 | Passed, executed by the integrator. Exercises the controlled wrapper through React/native doubles; no Fabric command-delivery claim.             |
| `pnpm typecheck`                                                                               | Passed, executed by the integrator.                                                                                                              |
| `pnpm docs:sync`, `pnpm validate:docs`, changed-document Prettier check and `git diff --check` | Passed on the follow-up documentation tree. Local Markdown links exist; the seven gate records and inherited #104 handoff content are unchanged. |
| Full native ON/OFF CI on the final combined commit                                             | Pending. The original 23/23 and 17/17 counts are not assigned to this updated candidate.                                                         |
| Fresh full JS suite, QNX cross-build, QNX VM, physical QNX and actual Marathon/Wayland session | Not run by this documentation follow-up.                                                                                                         |

The [owner's QNX report](../evidence/gallery-owner-report-2026-11-10.md)
retains their statement that PR #110 ran smoothly. It does not identify the
installed executable, bundle or assets hashes, board/image, selected flags or
individual actions. Repository ancestry establishes that the inspected #110
contains #104; it does not bind the owner's installed artifacts to either
exact commit. Do not convert the report into feature-specific pass counts,
physical HID/IME/accessibility acceptance or release qualification.

## Skills, ownership and follow-through

| Source read                                                                    | Source revision/path                                                                       | Decision applied                                                                                                        |
| ------------------------------------------------------------------------------ | ------------------------------------------------------------------------------------------ | ----------------------------------------------------------------------------------------------------------------------- |
| Repository QNX Native API, QNX Platform Validation and QNX Expo Phase Delivery | `.agents/skills/` at implementation revision `083c4bd`; relevant Phase 3 and Phase 6 plans | Preserve the shared native architecture and distinguish original author, current host, cross-build and target evidence. |
| Margelo `api-design` → `build-nitro-modules` → `cpp`                           | `1e9e17be6f41c838db472d6a4b943fc26355a3be`, `skills/<name>/SKILL.md`                       | Keep the existing native API/ownership contract; this follow-up creates no new native API or generated binding.         |
| Callstack `writing-user-docs`                                                  | `59e9d17446a248b21d86f078fcd0bd2d36db1ca4`, `skills/writing-user-docs/SKILL.md`            | Replace stale untested claims with the exact available evidence and actionable remaining validation.                    |

These sources were read from repository/pinned upstream checkouts; no personal
skill installation is claimed. Documentation DRI: the assigned PR review
agent. CI implementation and independent documentation reviewer: the parent
integrator. Human device-qualification and release acceptance remain
unassigned. Reading a skill or reviewing a report is separate from rerunning
its native or device tests.

Publish the combined candidate and inspect both full-host matrix jobs at its
exact head, including the newly enabled native test registrations and results.
Retain #104 before #110 in the dependency order, then check the final base and
diff. Run an actual host Fast Refresh edit/reload/reconnect session separately.
Attach exact artifact identities and named interactions to future QNX evidence;
keep optional-feature, ordinary gallery and release acceptance distinct.

No licensing permission, performance result or distribution authorization is
added. Evaluation Qt launch evidence remains historical; public-source Qt
qualification follows ADR-0013. The documentation can be reverted without
runtime effects. Reverting the whole integration must keep the Switch JS and
native command implementations matched; feature flags retain their existing
OFF rollback paths.
