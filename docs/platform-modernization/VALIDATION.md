# Program validation record

## Preserved predecessor PR47 readiness record

The following record describes PR47 before merge. Its unpublished-workstream state and commands are historical; the follow-up record below tracks the expanded series separately.

Readiness review on 2026-11-07. This PR changes program documentation, source/ownership records and repository skills. It changes no runtime, dependency, workflow, target image or boot configuration.

## Candidate and scope

Original program head `fab13fc69a7338df2e267971823897e9f54e174e`; premium-boundary follow-up `6edc6568fd889ee2d16dbb69b2600d8bc24bc331`. Readiness integrates main `cfcf2b4e7ec934171876773fd465271eac83de9f`, where PRs42/44/45/46 are merged. Final integration also preserves main `95c3fce` (premium policy and manual-only compatibility workflow); the AGENTS conflict was resolved by retaining the program routing and the complete current premium policy. Their dated receipts remain evidence for their own implementation revisions, not a native validation pass for this documentation change.

Program00 is published as [PR47](https://github.com/mikevocalz/react-native-qnx/pull/47). Workstreams01–13 are proposed and unpublished; their catalog paths and branches are proposed destinations, not existing delivery documents. Null URLs/commits deliberately record that state. No companion PR or full specification publication is claimed.

## Checks performed and corrections

- Read the two repository skills, program, roster, catalog, source lock and existing architecture/ownership rules. Independent review found false prepared/publication claims, a stale PR46 row and premium Forge implied as core conformance work. Corrected those boundaries. The current input row records host14/14 ON,13/13 OFF and synthetic Qt input on physical QNX, with hardware/target-JS/Wayland limits retained.
- Initial raw-file SHA256 verification failed for all five entries in `skills.lock.json`. Retrieved the exact pinned repository/commit/path bytes through curl with TLS verification and corrected all five hashes. A fresh comparison against the corrected lock passes5/5. This does not retroactively validate the previously recorded hashes or prove installation, source-guidance execution, native compatibility or device support.
- Initial system-Python skill validation failed because PyYAML was absent. Used an isolated temporary virtual environment with PyYAML6.0.3; the actual skill-creator `scripts/quick_validate.py` then passed both skills. No global Python or project dependency was installed.
- Corrected two broken relative links to repository skills. Checked local Markdown links, JSON parsing, catalog IDs and dependency cycles, published versus proposed path semantics, and repository skill/agent YAML fields. Unpublished paths are checked as proposed records, not counted as existing files. Program12 core conformance does not require optional commercial workstream10.
- Prettier and `git diff --check` passed for the final documentation/skill changes. Existing native source, app window dimensions/orientation, dependency pins and boot configuration are unchanged.

Commands are reproducible from the repository root (substitute the actual installed skill-creator path and a Python environment containing PyYAML):

```sh
python /path/to/skill-creator/scripts/quick_validate.py .agents/skills/qnx-native-api
python /path/to/skill-creator/scripts/quick_validate.py .agents/skills/qnx-platform-validation
pnpm exec prettier --check AGENTS.md .agents/skills docs/platform-modernization
git diff --check
```

For source verification, fetch each `skills.lock.json` entry using its immutable repository/commit/path URL and compare SHA256 of raw bytes, not a rendered page or API envelope. The listed SHA is source provenance, not a runtime package version.

## Independent review and behavior

Readiness DRI `/root`; independent reviewer `/root/renderer_build_audit` reviewed program consistency, both workflows, publication/optional-product boundaries and source provenance. Human implementation owners remain unassigned; no vendor or maintainer endorsement is implied. The earlier draft's pending claims about other drafting/evaluation agents are not counted as completed evidence here. Skill frontmatter validation establishes structure; source and independent workflow review establish only the inspected guidance, not native execution.

## Not run and rollback

No native compilation, ASan/UBSan, QNX cross-build, Qt/QNX/Marathon execution, physical-device test or performance measurement was run for this documentation/skill PR. No runtime acceptance criterion is marked complete. Hosted CI is subject to the existing account billing/spending-limit refusal; local checks do not imply green hosted jobs.

Revert these additive program/skill changes to remove their guidance. Future implementation remains separately scoped, reviewed and validated, with the existing renderer, recovery boot, licensing and permission boundaries preserved.

## Follow-up expansion record

The expanded program is prepared against main `48e37f7550b4244f8a7936c01af75419e1fec416`, preserving PR47 and the owner's optional premium-product policy. It adds the Nitro v1 API contract, a 19-category legacy-source map, two new QNX source/probe workstreams, and detailed companion specifications. Publication URLs are recorded only from successful GitHub responses.

An independent fresh-context agent exercised both repository skills against an unsafe native temperature API proposal. It correctly rejected synchronous MsgSend hidden by a JS Promise, a receive-only timeout for SEND/REPLY, and shutdown joining a worker that still awaited JS. It also required real v1 generation and separate host/target evidence. This was a document/skill behavior review; no native code executed.

Independent design review corrected initial-observation ordering, shared-buffer disposal/pinning, admitted-callback cleanup semantics, and completion-queue drain wording. The official-source audit distinguishes inspected sections from discovered indexes and pending manuals. Root review preserves current PR46 proof: host 14/14 ON, 13/13 OFF and deterministic Qt event injection on physical QNX; physical USB/touch and target JS-handler delivery remain separate gates.

Follow-up checks passed: 24 Markdown documents, 13 local relative links, all 16 unique workstream IDs with an acyclic implementation dependency graph, all 19 archive categories matched to the pinned inventory, and five source-byte SHA256 comparisons. Both project skills passed the actual skill-creator quick_validate.py again. An independent source reviewer approved Plans 14 and 15 after the message-queue/pipe coverage gap was corrected. Remote publication verification will be recorded after publication. Prettier and native checks from the predecessor record were not rerun or claimed for this follow-up. No native compilation, sanitizer run, cross-build, device execution or benchmark was performed for this follow-up. Earlier receipts do not become a pass for these new specifications.
