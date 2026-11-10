# Gallery follow-up: owner report, 2026-11-10

The owner reported testing their QNX build for PR #110 and said that everything
had run smoothly so far. This is useful owner-reported runtime feedback. The
report did not identify the executable or bundle hashes, board/image, enabled
features, launch command, or individual actions exercised. No separate device
session, capture, timing measurement, or rebuilt native executable was produced
by this follow-up.

## Source relationship

The repository inspection found PR #104 at
`3b414f185ec9b6c23eb7fbfdbc0afc9bfed18b94` and PR #110 at
`083c4bdb887a844e533313fed9b967ff94f4c4d3`. Git ancestry shows that #110 contains
that exact #104 head and adds one commit. Those are the inspected repository
revisions; the owner's report did not establish that the installed binary was
built from either exact revision. PR #102 is a separate branch, with common
ancestor `0c91be9bbb4e7cd6dabccf8cdfe441c7fa57c629`, and is not implicitly
accepted by this report.

The [2026-11-09 host receipt](gallery-desktop-host-2026-11-09.md) remains the
record for the measured Linux gallery tests. Its native binaries and bundle
hashes are not relabeled as the owner's QNX artifacts.

## What changes in this follow-up

- Retain the owner's positive report alongside the earlier host and failed-input
  history. Per-component physical support remains unverified where a named
  scenario and artifact receipt are absent.
- Correct the Modal host-support note: the implemented Qt bridge dispatches
  `onRequestClose` on Escape, uses viewport dimensions, and blocks background
  pointer/wheel input. `onShow` and `onDismiss` remain unwired. This documentation
  correction does not add or change native behavior.
- Preserve PR #104's existing commits and the #104 → #110 ancestry. Merge order
  remains #104 before the dependent #110, with the latter's target and diff
  checked after any base update.

## Validation and acceptance

This documentation follow-up uses Linux x86-64, Node 26.13.0 and pnpm 12.8.1.
The frozen offline dependency installation passed. The component-conformance
JavaScript test passed, checking fixture registration/state retention/public
entry/assets without mounting the native renderer. Documentation validation,
formatting, and the host-renderer inventory test are recorded in the handoff.
No QNX cross-build, QNX VM, physical interaction, or Marathon/Wayland test ran
for this follow-up. No release gate or device-support row is promoted.

For target acceptance, attach the installed ELF/HBC/assets hashes and selected
launch flags to the actions actually exercised. Separate ordinary gallery
interaction from optional desktop menus, resize/close behavior, modal focus,
and any untested control. The owner report is retained even while those details
remain unspecified; it is not replaced with invented pass counts.

## Sources, ownership, and rollback

Repository `qnx-platform-validation` and `qnx-expo-phase-delivery` were read at
the inspected #104 revision. They keep owner feedback separate from observed
artifacts and preserve the shared Fabric/Yoga/Hermes/Qt renderer. Callstack
`writing-user-docs` was read at
`59e9d17446a248b21d86f078fcd0bd2d36db1ca4` for the fixture-guide link and its
acceptance wording. No new native API, Nitro generation, dependency upgrade,
license change, or performance claim is introduced.

Automated documentation/review owner: `audit_base_stack`; independent merge
review: the session integrator. Human QNX/Qt evidence acceptance remains
unassigned. Reverting this follow-up removes the report and host-support
correction without changing runtime behavior.

## Follow-up source checks

The frozen lockfile SHA-256 is `b854ebe2e50a42b2e8e07124df065592d99d43b1d1a3a984b66d3835ad86c3e4`.
Documentation, dependency, architecture, and manifest validation and
`pnpm exec tsc --noEmit` all passed with exit 0. Generated records were updated
with `pnpm docs:sync`; release gates remain pending. `git diff --check` passed.
