# Five-surface design preparation

Base: `f87c742a423e5c9fda61dfa837b629a7a2dd5307`. Gates:
`window-chrome`, `app-identity`, `expo`, all pending. This PR changes design
documents and appends to the existing handoff. It changes no QML, native code,
application state, dependencies, window dimensions or Linux/Wayland behavior.

## Delivered documents

- [References](references.md): three Mobbin screens for each of five surfaces,
  with canonical links, borrowed details and reference limitations.
- [Research plan](research-plan.md): interview guide and five-task first-time
  developer script. No interviews or usability sessions were conducted.
- [System audit](system-audit.md): current source tokens, type, spacing, target
  sizes and proposed names/corrections.
- [Direction](direction.md): decisions and rejected alternatives per surface.
- [Strings](strings.md): selected English text and static budgets, plus
  measured-overflow rejection and defensive runtime elision rules.
- [Accessibility audit](accessibility-audit.md): source-based findings,
  severities and untested runtime checks. No WCAG conformance claim.
- [Critique](critique.md): per-screen review against the supplied roster.
- [Handoff](../app-design-handoff.md#qnx-native-runner-surfaces-2026-11-14):
  additive layout, tokens, states, edges, motion and ownership contract.

## Skills, source and review

[skills.json](skills.json) records read bodies, plugin versions, exact byte
lengths and SHA-256 values. The design sequence used Mobbin references, then
`user-research`, `design-system`, `frontend-design`, `ux-copy`,
`accessibility-review`, `design-critique` and `design-handoff`. The knowledge-work
design plugin is 1.2.0; frontend-design is 1.1.1. Documentation 1.2.0 and the
unversioned local no-ai-slop guidance were also read. Decisions from each are
recorded in the documents. Local cache presence is not an upstream provenance
attestation. The installed code-review skill is 0.1.0; CodeRabbit CLI 0.7.6 is
signed out, so no external CodeRabbit review was run.

Source inspection covers `WindowChrome.qml`, its model/header and host tests,
`QnxDevSupport.cpp`, label-fit/desktop-entry tooling and ADR-0016. Mobbin previews
were inspected by the design author; browser headers supply hierarchy examples,
not proof of native window-manager conventions. Actual simple-terminal Screen
source inspection and target cursor behavior remain H20/H28 prerequisites.

DRI: `explore_g`, acting as Qt Quick / Fabric and Documentation / DX engineer.
Independent reviewer: fresh-context `review_design`, acting as native UI and
accessibility reviewer. The reviewer recomputed all contrast ratios, verified
all skill file hashes/lengths and string budgets, checked source claims and
preserved window contracts, and reported no blocking finding. It did not reopen
the Mobbin previews; that inspection remains attributed to the author. No
external roster author or agency is an assigned collaborator.

## Validation and limits

[validation.txt](validation.txt) retains the actual source-color calculations,
source excerpts, formatting and document checks. Ten solid-color contrast
pairs, 29 static English string budgets and 15 canonical reference links were
checked. These are host/source checks, not rendered-pixel measurements or device
results. JSON parsing, local link targets, Prettier, `git diff --check` and
`node scripts/validate-docs.mjs` pass locally. Exact-head hosted results remain
separate.

The existing dark focus ring and inactive-title colors need implementation
changes. The preflight screen, windows API, runtime layout switching, native
eager compatibility check and JS/GUI isolation remain unimplemented. Research,
keyboard/screen-reader/reduced-motion behavior, target label geometry and the
requested device acceptance are not completed by this PR. No host native,
QNX cross-build, QEMU, Pi 400 or stock QSTI run was performed here.

Rollback is reverting these documents and the additive handoff section. There
is no binary or target rollback. This PR distributes no SDK, evaluation runtime,
premium asset or Mobbin screenshot. Implementation must preserve the current
window contract and complete the separate A/B/runtime prerequisites before
claiming the proposed native behavior.
