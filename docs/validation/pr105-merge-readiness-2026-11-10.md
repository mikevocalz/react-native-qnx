# PR 105: Nitro contract-validation review

Reviewed 2026-11-10 against implementation commit
`c09a5b19800c2d7d79268c3466b618aa20430ba2`, based on
`0c91be9bbb4e7cd6dabccf8cdfe441c7fa57c629`.
This follow-up records the validation and continuation state required by
[CONTRIBUTING](../../CONTRIBUTING.md#handoff). It changes no generator,
TypeScript contract, generated binding, dependency, native source or release gate.

## Behavior and source

The existing change rejects class/member syntax that the constrained platform
generator cannot emit faithfully: unsupported visibility and static/optional
members, generic or invalid heritage, executable bodies/initializers, and
unsupported parameter forms. Explicit public members and the platform-info
getters/add contract remain accepted. It does not add general QNX Nitrogen
support. The two canonical generated C++ bindings remain byte-for-byte unchanged.

Local host: Linux x86-64, Node 26.13.0, pnpm 12.8.1. Frozen lock SHA-256:
`b854ebe2e50a42b2e8e07124df065592d99d43b1d1a3a984b66d3835ad86c3e4`.
Pins remain React Native 0.88.1, Hermes `hermes-v260318099.0.5`,
TypeScript 7.0.2 with parser 6.0.2, and Nitro core/adapter/generator
`1.0.0-alpha.nightly.105.g69d365fc9b18`.

## Executed local checks

| Command                                                                                    | Result and scope                                                                                                                |
| ------------------------------------------------------------------------------------------ | ------------------------------------------------------------------------------------------------------------------------------- |
| `pnpm install --frozen-lockfile --store-dir /workspace/.pnpm-store`                        | Passed; exact lock unchanged. Initial offline attempt lacked a cached tarball; the ordinary frozen install populated the store. |
| `node tests/js/nitro-platform-codegen.test.mjs`                                            | Passed; 24 unsupported-contract cases, the explicit-public case and deterministic generated output.                             |
| `pnpm nitro:platform:check`                                                                | Passed; no generated C++ changes.                                                                                               |
| `pnpm typecheck`                                                                           | Passed.                                                                                                                         |
| `pnpm validate:dependencies`                                                               | Passed; actual installed pinned source contracts inspected.                                                                     |
| `pnpm docs:sync`, `pnpm validate:docs`, changed-file Prettier check and `git diff --check` | Passed on this documentation follow-up.                                                                                         |

No local full native build, sanitizer run, QNX cross-build, QNX VM, physical QNX
or Marathon execution was performed for this follow-up. No new runtime behavior
was introduced. The initial implementation's local full-JS socket restriction
remains historical authoring evidence; the later hosted runs below are separate.

## Hosted evidence at the implementation commit

The GitHub API was checked on 2026-11-10. The following workflows completed
successfully for `c09a5b1`:

- [SDK 58 validation](https://github.com/mikevocalz/react-native-qnx/actions/runs/42646750857).
- [Full host renderer](https://github.com/mikevocalz/react-native-qnx/actions/runs/42646751090).
- [Nitro v1 native source and host compile](https://github.com/mikevocalz/react-native-qnx/actions/runs/42646750948).
- [Image loader HTTP/cache](https://github.com/mikevocalz/react-native-qnx/actions/runs/42646751042).
- [Pi tooling quality](https://github.com/mikevocalz/react-native-qnx/actions/runs/42646751134).

These results belong to the named implementation revision. They do not assert
that a later documentation or integration commit has already passed CI.
No human approval was recorded in that snapshot. The open review thread asked
for the six committed handoff records; this follow-up supplies them.

## Skills and ownership

| Source read                                          | Pin                                               | Applied decision                                                                                                         |
| ---------------------------------------------------- | ------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------ |
| Repository QNX Native API / QNX Platform Validation  | Repository sources at the implementation revision | Preserve the constrained generator and report host, generated-output and device evidence separately.                     |
| Margelo `api-design` → `build-nitro-modules` → `cpp` | `1e9e17be6f41c838db472d6a4b943fc26355a3be`        | Reject unrepresentable input; retain the actual v1 class/registration contract and unchanged generated/native ownership. |

The pinned skill source files were read; no personal skill installation is
claimed. Implementation/handoff DRI: the assigned code-generation review agent.
Independent integration reviewer: the parent integrator; review acceptance is
not implied by this document. Human target-qualification owner remains unassigned.

## Remaining work and rollback

Publish this follow-up, inspect its resulting CI and resolve the handoff thread
after reviewing the records. Integrate independent PRs in a controlled order;
the owner's PR 110 smoke test does not include this branch. Preserve all seven
pending v1 gates. General generator expansion needs a separately supported
contract, deterministic generation and native behavior evidence.

There is no new permission, license, premium dependency, persistent data or
runtime performance change. Reverting this follow-up removes its records;
reverting the complete PR restores the older generator acceptance behavior.
