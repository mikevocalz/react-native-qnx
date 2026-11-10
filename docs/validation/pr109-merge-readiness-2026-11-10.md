# PR 109: macOS SDK host command review

Reviewed 2026-11-10 from PR head
`cb59d6174d3055038bd3248af36978fe65285a93`, based on
`0c91be9bbb4e7cd6dabccf8cdfe441c7fa57c629`.
This follow-up fixes Docker daemon selection in the SDK helper, adds behavioral
regression coverage, updates its runbook and records the required handoff.

## Defect and bounded fix

Docker's [CLI environment-variable reference](https://docs.docker.com/reference/cli/docker/#environment-variables)
states that `DOCKER_CONTEXT` overrides `DOCKER_HOST`. The helper originally
exported the selected Colima socket as `DOCKER_HOST` while leaving an inherited
context in place. Its `inspect`, `start` and SDK-file check could therefore
operate on a different daemon.

`use_profile_socket` now clears `DOCKER_CONTEXT` inside the script process
before exporting the selected socket. The caller's shell and saved context are
not modified. The existing restrictions remain: no new Colima profile or
container, no recreation/reconfiguration, no missing or paused container start,
and no QNX deployment or boot change.

The regression executes the real Bash entry point with inert Colima/Docker
fixtures. A fixture models Docker's documented context precedence and records
every requested command. Before the fix, the ordinary running-host case failed
because Docker still received the unrelated context. After the fix, all twelve
scenarios pass. This is a host-command simulation, not a Docker/Colima VM or
physical device run.

## Candidate identities

- Helper SHA-256: `fa86661623cbff141717f8c38e8ef4f8be2850804461c19098998ed43420c43d`.
- Regression-test SHA-256: `315405e8cbef3307c11467e931c8e31d41461c94bcfb93f5dcc673cfd8d336a0`.
- Frozen lock SHA-256: `b854ebe2e50a42b2e8e07124df065592d99d43b1d1a3a984b66d3835ad86c3e4`.
- Local host: Linux x86-64, Node 26.13.0, pnpm 12.8.1, Bash; CMake 4.4.4
  supplied for the existing full-JS fixtures.
- RN 0.88.1, Hermes `hermes-v260318099.0.5`, Nitro
  `1.0.0-alpha.nightly.105.g69d365fc9b18` and all dependency pins are unchanged.

## Actual validation

| Command or scenario                                                                        | Result                                                                                                                                                                                                                                                                                                                                  |
| ------------------------------------------------------------------------------------------ | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `pnpm install --offline --frozen-lockfile --store-dir /workspace/.pnpm-store`              | Passed; exact lock unchanged.                                                                                                                                                                                                                                                                                                           |
| `node tests/js/qnx-sdk-container.test.mjs` before the fix                                  | Failed as expected: inherited Docker context reaches the wrong-daemon fixture and status returns 1 instead of 0.                                                                                                                                                                                                                        |
| Same test after the fix                                                                    | Passed: 12 scenarios cover running/stopped status, absent profile/container, paused container, VM/container resume order, created container, missing SDP, either start failure, custom names/path, and rejected commands. The Docker-context and parent-environment assertions run within those cases.                                  |
| `bash -n scripts/qnx-sdk-container.sh`                                                     | Passed.                                                                                                                                                                                                                                                                                                                                 |
| `pnpm typecheck`, `pnpm validate:dependencies`                                             | Passed.                                                                                                                                                                                                                                                                                                                                 |
| `pnpm docs:sync`, `pnpm validate:docs`, changed-file Prettier check and `git diff --check` | Passed.                                                                                                                                                                                                                                                                                                                                 |
| `pnpm test:js`                                                                             | Blocked by the existing Marathon development-launcher Unix-socket fixture: `listen EPERM`. Before adding CMake to PATH, an earlier attempt stopped at the missing CMake prerequisite; that prerequisite was then corrected and the suite advanced to the socket restriction. No tests were removed or skipped to turn this into a pass. |
| Local ShellCheck                                                                           | Not run: executable absent. The unchanged Pi tooling workflow must lint the published candidate.                                                                                                                                                                                                                                        |
| Real macOS Colima/Docker status or restart                                                 | Not run in this Linux review workspace.                                                                                                                                                                                                                                                                                                 |
| Native build, QNX cross-build/VM/physical, Marathon shell                                  | Not run; this follow-up changes host tooling, not native platform behavior.                                                                                                                                                                                                                                                             |

## Historical hosted evidence

The following workflows passed on the original `cb59d61` head; they are
predecessor evidence, not a result for this fix:

- [Pi tooling syntax/ShellCheck](https://github.com/mikevocalz/react-native-qnx/actions/runs/42729746745).
- [SDK 58 validation](https://github.com/mikevocalz/react-native-qnx/actions/runs/42729747020).
- [Full host renderer](https://github.com/mikevocalz/react-native-qnx/actions/runs/42729746735).
- [Image loader HTTP/cache](https://github.com/mikevocalz/react-native-qnx/actions/runs/42729746831).

No reviews or unresolved threads were recorded on that snapshot. The
independent source review found the context-selection issue before promotion.

## Skills, ownership and next step

Repository QNX Platform Validation was read at the original PR revision and
applied to separate simulated host-command coverage from real target evidence.
Callstack `writing-user-docs` at
`59e9d17446a248b21d86f078fcd0bd2d36db1ca4` was read and applied to the runbook's
daemon-selection steps. These are source-read/applied records, not claims of
personal skill installation.

Implementation DRI: the assigned SDK-host review agent. Independent reviewer:
the parent integrator, with review acceptance recorded separately. Human
macOS/target-qualification owner remains unassigned.

Publish the follow-up and inspect its own SDK/Pi tooling checks, including
ShellCheck and the unmodified full JS suite. At the next operator-supervised
Mac session, run `scripts/qnx-sdk-container.sh status` with a different
`DOCKER_CONTEXT` in the caller's environment and verify the selected profile;
do not start or alter unrelated containers. PR 102's final-runner instructions
remain a separately labeled dependency, and all seven v1 gates remain pending.

There is no dependency, license, premium-component or platform API change. No
runtime performance claim is made. Reverting this follow-up restores the older
context-selection behavior and removes the regression/records; it does not undo
a previously requested VM/container start. No real host was started by this
review.
