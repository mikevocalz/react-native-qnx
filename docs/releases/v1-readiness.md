# Prepare the QNX v1 milestone

V1 is a release milestone for the RN 0.88 line. It does not imply a stable
Nitro ABI: the exact v1 alpha remains pinned. Package versions stay at their
existing values until package ownership, release units and publication are
reviewed. This preparation PR does not tag or publish a release.

<!-- generated:gates:start -->

| Gate                               | Status  | Acceptance evidence required                                                                                                                                                                                                                              |
| ---------------------------------- | ------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Open-source runtime packaging      | pending | Qualify the eligible public Qt source build, exact module/QPA notices and hashes, shared-library/plugin closure and applicable source/library replacement materials. Keep evaluation and QNX SDK payloads separate. No paid Qt subscription prerequisite. |
| Candidate CI                       | pending | Successful required checks for the release candidate on authorized runners; local checks remain separately labeled.                                                                                                                                       |
| Repeatable graphics cold boot      | pending | 20 consecutive supervised cold boots on the named release board/image, with recovery and logs. Application relaunch trials do not count as cold boots.                                                                                                    |
| Device component and input results | pending | Every component has a device disposition and tested limits on stock Pi 4 QSTI; prove keyboard/touch and the selected interaction scope. Pi 400 results stay separate.                                                                                     |
| QNX performance measurements       | pending | Publish startup-to-first-frame, presented-frame intervals and RSS for Hello World, a 1000-row list and drawer drag, including revision, sample sizes and capture limitations.                                                                             |
| Clean-host app workflow            | pending | A new licensed developer can create, build, deploy and run an app from documented prerequisites. Verify actual package ownership and contents before any npm publication.                                                                                 |
| Versioned v1 documentation         | pending | Published, versioned getting-started, API/support limits, evidence gallery, ADRs and release notes with reviewed local links.                                                                                                                             |

<!-- generated:gates:end -->

The table is generated from [v1-readiness.json](v1-readiness.json).
[Required checks](required-checks.md) lists the workflows the Candidate CI
gate needs, with hosted, QNX cross-build and device results kept apart.
`pnpm validate:docs` checks that documentation matches its sources.
`pnpm validate:v1` additionally exits nonzero unless every required gate is
passed with a repository evidence path. A reviewer must inspect the cited
revision and actual result before changing a gate; a file's existence cannot
prove the claim inside it.

The first slice adds a local CLI/template, dependency/support generation,
release checks, project license and contributor policies. It preserves the
native runtime, Qt subset, feature flags, window dimensions and recovery boot.
Device results remain historical prerequisites until the affected candidate
is tested. The [current handoff](../handoff/CURRENT_STATE.md) names those results.
The [preparation receipt](../evidence/v1-polish-host-2026-11-07.md) records the
CLI bundle/render check, native host results, failed build attempt and review.

## Use the selected open-source Qt path

The repository owner clarified the direction on 2026-11-07:
[eligible open-source Qt](../adr/0013-open-source-qt.md), with no paid Qt
subscription prerequisite. Marathon's Linux/Wayland target uses that path.
Keep QNX's separate toolchain and runtime packaging requirements explicit.

Qualify the exact public-source build and its module/QPA notices, hashes,
libraries, plugins and applicable LGPL/GPL accompanying materials. The older
Pi 400 receipts used private evaluation artifacts; their expiry does not expire
open-source Qt or stop development. Release artifacts need their own source
and build record. See [packaging](../packaging.md).

## Next implementation slices

Prioritize public-source packaging and candidate CI, then graphics cold boot on the
release image. Add measured device conformance and performance before promoting
support or publication claims. IME, multi-touch, Reanimated and general QNX
Nitrogen generation retain explicit gap records. Forge remains an optional
licensed consumer integration.
