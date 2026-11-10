# PR 107: SegmentFetcher native-boundary review

Reviewed 2026-11-10 against implementation commit
`507d06c11ae19418e3342c5763491370230e6baf`, based on
`0c91be9bbb4e7cd6dabccf8cdfe441c7fa57c629`.
The initial follow-up added the missing [native source citation](../cited-seams.md#segmentfetcher-callback-boundary-2026-11-10)
and required handoff records. The later CI repair below changes only the host
supervisor test fixture; native implementation, dependencies, renderer and
release gates remain unchanged.

## Published follow-up CI repair

The [SDK 58 validation job](https://github.com/mikevocalz/react-native-qnx/actions/runs/42843403021/job/129924318791)
at published head `3883efbd657d623972be5fc687f4bf2ffcf9085a` passed install,
formatting, dependency/type checks and both QNX bundle steps, then failed in
`runtime-baseline.test.mjs` at `assert.ok(childPid > 0)`. The fixture created its
PID file before writing its contents. The parent observed the newly created
empty file and interpreted it as PID zero. The hosted log later records runner
cleanup of the remaining Python child. This is a readiness race in an existing
test fixture, separate from SegmentFetcher.

The Python fixture now writes and closes a sibling `.pid.pending` file before
atomically replacing the readiness path. The existing assertions still require
a live child, expected TERM/HUP supervisor exit, interrupted receipt and complete
child cleanup. No timeout or assertion is weakened.

Focused validation on Linux x86-64, Node 26.13.0 and Python 3.12.14:

- `node tests/js/runtime-baseline.test.mjs`: passed, including TERM/HUP cleanup.
- `node tests/js/runtime-baseline-qnx.test.mjs`: passed; synthetic remote-shell
  fixtures, no QNX execution.
- A separate in-memory execution of the first test inserted `time.sleep(0.1)`
  immediately before the pending PID write: passed both interrupt scenarios.
  The deliberate delay was not added to repository code.
- `pnpm typecheck`, `pnpm docs:sync`, `pnpm validate:docs`, changed-file
  Prettier and `git diff --check`: passed.

The failed hosted run remains recorded as a failure. The final published head
must pass its own CI. This repair adds no native build or target evidence;
reverting it restores the fixture's readiness race. The parent integrator is
the independent reviewer and publication owner.

## Native contract

The existing implementation validates actual argument count before accessing
`args[2]`, then verifies object kind and callability. Invalid calls throw a
real JSI Error. Valid calls retain the existing explicit unsupported-segment
callback, invoke it once and allow its original exception to propagate.
Segment loading remains unsupported.

The frozen installed RN 0.88.1 `TurboModule.h` forwards the caller's actual
argument count; the declared metadata arity only supplies the host function's
length. The installed `jsi.h` defines that count, object/callable detection,
conversion and Error constructor. Exact paths, symbols and file SHA-256 values
are recorded in the cited-seams ledger. This is an existing RN-internal
TurboModule contract, not an application-facing native API.

## Candidate and local checks

Host: Linux x86-64, Node 26.13.0, pnpm 12.8.1. Frozen lock SHA-256:
`b854ebe2e50a42b2e8e07124df065592d99d43b1d1a3a984b66d3835ad86c3e4`.
React Native remains 0.88.1, Hermes `hermes-v260318099.0.5`, and Nitro
core/adapter/generator `1.0.0-alpha.nightly.105.g69d365fc9b18`.

| Command or check                                                                           | Result and scope                                                                |
| ------------------------------------------------------------------------------------------ | ------------------------------------------------------------------------------- |
| `pnpm install --offline --frozen-lockfile --store-dir /workspace/.pnpm-store`              | Passed against the populated shared cache; no dependency or lock change.        |
| Installed `TurboModule.h` / `jsi.h` inspection and `sha256sum`                             | Passed; exact source contracts recorded, including arity versus supplied count. |
| `pnpm typecheck`, `pnpm validate:dependencies`                                             | Passed.                                                                         |
| `pnpm docs:sync`, `pnpm validate:docs`, changed-file Prettier check and `git diff --check` | Passed on this documentation follow-up.                                         |
| New local native build / ASan run                                                          | Not run; this follow-up changes documentation only.                             |
| QNX cross-build, VM, physical QNX and Marathon                                             | Not run for this candidate.                                                     |

## Hosted native evidence supersedes the earlier authoring gap

The original PR report stated that a local native configure was blocked by the
missing Clang toolchain. Preserve that as a historical authoring result.
Subsequent GitHub execution **did compile and run the native regression** at
`507d06c`.

The [SDK full-host renderer workflow](https://github.com/mikevocalz/react-native-qnx/actions/runs/42647596895)
completed successfully for both input-lifecycle OFF and ON. In the inspected
[OFF job](https://github.com/mikevocalz/react-native-qnx/actions/runs/42647596895/job/129264964842),
the log records compilation of `QtReactHostHostTest.cpp`, linking
`qt_react_host_host_test`, its successful execution, and **14/14 CTests passed**.
The implementation registers `segmentFetcherValidatesCallbackBoundary` in that
test executable outside the bundle-only guard. It covers zero/one/two
arguments, non-callable third arguments, both method names, a single callback,
and preservation of the callback's original exception.

Also successful at that implementation head:

- [SDK 58 validation](https://github.com/mikevocalz/react-native-qnx/actions/runs/42647596756).
- [Shared runtime AppState](https://github.com/mikevocalz/react-native-qnx/actions/runs/42647596698).
- [Image loader HTTP/cache](https://github.com/mikevocalz/react-native-qnx/actions/runs/42647597303).
- [Experimental WebSocket host tests](https://github.com/mikevocalz/react-native-qnx/actions/runs/42647596786).

These are dated hosted Linux/Qt/Hermes results. They do not prove physical QNX,
Marathon shell execution, full-system sanitizer instrumentation or future-head
CI. The parent integrator must inspect the published follow-up's own checks.

## Skills, ownership and continuation

| Source read                                          | Pin                                               | Applied decision                                                                                                  |
| ---------------------------------------------------- | ------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------- |
| Repository QNX Native API / QNX Platform Validation  | Repository sources at the implementation revision | Cite actual installed native symbols and separate local, hosted and device results.                               |
| Margelo `api-design` → `build-nitro-modules` → `cpp` | `1e9e17be6f41c838db472d6a4b943fc26355a3be`        | Preserve caller-visible errors, exact v1 boundaries and existing runtime ownership; no new binding or dispatcher. |

The skill sources were read; no personal skill installation is claimed.
Implementation/handoff DRI: the assigned runtime review agent. Independent
integration reviewer: the parent integrator; acceptance remains a separate
review. Human device-qualification owner remains unassigned.

Publish the follow-up, verify its CI and resolve the cited-seams and handoff
threads after inspecting the committed records. The owner's PR 110 smoke test
does not contain this independent fix. The final integrated native candidate
still needs its own checks; all seven v1 release gates remain unchanged.

No new privilege, licensing, premium dependency or persistent-data change is
introduced. No runtime performance measurement is claimed. Reverting the
initial documentation follow-up removes its provenance/handoff records;
reverting the complete PR restores the older unchecked argument access.
