# Phase implementation integration candidate

This review branch combines the implemented slices, not completion of the seven
phases. React Native QNX remains QNX-first. Main and the original implementation
branches remain unchanged. No package publication or physical-device result is
implied by this integration.

## Exact implementation sources

- Phase1, PR89: `eaca3fdb964b8cd82172aa202748fadce8be4d6a`.
- Phase2, PR90: `c2b617f3819d85262f8efb86ca142244b56ee641`.
- Phase3, PR91: `4dbb7c3deca9e9d687af5cddadac23735c7575a7`.
- Phase4, PR92: `4f478fcbb7aa486dd2b64d60d874440f2345a99e`.
- Phase5, PR93: `e30bfdfa64aa4db39c043c0c88dbcfc8f3e75d50`.
- Phase6, PR94: `bb984174c97e2461f460e922bbfa552365e752ef`.
- Phase7, PR95: `a491d0e344df6c5abcc37612c4e2047f51752eb8`.

The integration commit retains Phase7 as its first parent, which already contains
Phases1,5,6. It retains Phases2,3,4 as additional parents. All inherit the prior
PR80 baseline `43f0692aabea57e0f965b892858811349db8ec5a`. The selected trees preserve
the exact implementation blobs. Shared SDK validation already contains the same
integration-base trigger in each overlapping branch. CLI commands are retained
from Phase6 rather than replaced with the older Phase1 entry.

## Combined behavior test

`tests/js/qnx-integrated-consumer.test.mjs` creates one source directory outside
the platform checkout. It links the actual installed workspace dependency graph,
uses public React Native imports, and executes the real doctor and bundler CLI
for qnx and marathon. The exact entry source stays unchanged between builds. Real
Expo discovery finds a local module with explicit declarations for both targets.
That module is an INTERFACE fixture: discovery is not native registration.

The registered JS suite runs the tooling portion. The full Qt/Hermes host workflow
also supplies an actual renderer and evidence directory to the same test, which
renders both bundles, checks platform log markers and PNG output, and retains
source/bundle hashes and logs. Those are Linux offscreen results, not Marathon
compositor or physical QNX qualification. The source still uses RNQnxPlayground
registration; per-app native identity remains unfinished.

Dependencies are linked from the provisioned CI graph, not independently installed
published packages. This test does not establish standalone distribution, broad
Expo module parity, Expo Router, native gestures, permissions, debugger attachment
or physical lifecycle behavior. The isolated lifecycle observer remains default
OFF and is not silently installed into the application runtime.

## Validation and review

Existing SDK validation, conformance/performance capture and fresh-source workflows
remain intact. The full host matrix now also runs for the integration base, with
input contracts OFF and ON. Lifecycle sanitizer, public-import native-host,
Marathon-process, diagnostics and release-dossier workflows retain separate results.
Every result belongs to its exact candidate; prior branch passes are prerequisites,
not inherited acceptance.

The release dossier snapshots the unchanged seven pending gates. Integrity checks
cannot authorize release. No changes to runtime dependency pins, lockfile, recovery
boot, existing Qt/Fabric renderer or premium-product boundary occur here.

Implementation/integration author: assistant in this session. Independent
RN/Qt/Expo/tooling and release reviewers: unassigned. Applied repository native and
validation boundaries, phase rosters and source-read Callstack CI/library/docs
guidance. No new Nitro binding or native API is introduced by combining these
slices; actual v1 API-first requirements remain in force. No external skill install,
subagent execution or CodeRabbit approval is claimed.

Rollback by abandoning this review candidate leaves main and each original branch
intact. If incorporated later, revert focused integration changes with their paired
runtime/JS contracts; do not merge both this candidate and constituent PRs without
reconciling ancestry and review evidence.
