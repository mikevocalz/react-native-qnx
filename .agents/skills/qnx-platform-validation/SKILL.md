---
name: qnx-platform-validation
description: Validate changes to react-native-qnx and its Marathon target with exact source provenance and separate host, cross-build, VM, and physical-device evidence. Use for PR review, runtime or renderer changes, performance claims, native lifecycle tests, deployment runbooks, and release readiness.
---

# QNX and Marathon validation workflow

Read `AGENTS.md`, the affected delivery plan, and `docs/platform-modernization/SKILLS.md`. Resolve paths from the repository root. Select tests by changed behavior and concrete risk; do not run unrelated suites merely to increase the count.

## Freeze the candidate

Record the base and head commit, working-tree modifications, dependency lock hash, React Native/Hermes/Nitro identities, compiler/sysroot, Qt modules/plugins, build type, flags, board/image, display mode, and app bundle hash. Record the source tree against which patches apply. Keep credentials, signing keys, licensed archives, private device addresses, and unrelated personal data out of shared evidence.

Read current PR evidence as well as main-branch handoff documents. Dated evidence in a prerequisite PR stays attached to that revision; it is not an automatic pass for a new branch. Distinguish a compile, a loaded module, a rendered screen, and a measured interaction.

## Choose meaningful gates

1. For documentation or skills only, validate the changed Markdown links, structured records, skill frontmatter, source references, and dependency coverage. Report runtime and device tests as not run because no runtime behavior changed. Do not fabricate native evidence to make a planning PR look complete.
2. For JS/dependency work, inspect the actual package scripts, use the pinned package manager and frozen lockfile, typecheck, validate dependency/source contracts, and bundle the affected platform. Verify QNX resolver/bundle behavior separately when Marathon tooling changes.
3. For native changes, compile the affected graph with feature flags OFF and ON; reject unsupported configurations. Run behavior and error tests under the repository's supported host sanitizers. Explain sanitizer scope and disabled checks; do not turn a partially instrumented build into a whole-system claim.
4. Cross-build against the exact licensed QNX SDP and target Qt dependency closure. Compiler success proves ABI/source compatibility only for that graph, not target execution or visual correctness.
5. On the authorized target, execute a bounded test with captured status, output, and an external watchdog where appropriate. Test the user's observable behavior, failure recovery, and shutdown. Never reset, flash, or rewrite boot configuration as an incidental test step.
6. Run Linux offscreen, Linux Wayland, actual Marathon shell, QNX VM, and QNX physical lanes separately when relevant. Use host simulations for deterministic cases but label them as simulations. Record unavailable capabilities explicitly.

## Review ownership and failure behavior

Exercise failed initialization, permission denial/revocation, service death/restart, native worker error, cancellation races, queue closure, late callback, stale generation, and repeated disposal where the change touches them. Assert that no JS callback enters a destroyed runtime and no Qt item is mutated from a worker. Separate operation cancellation, caller timeout, server side-effect cancellation, and shutdown completion.

Use a non-premium shared conformance application for integrated gestures, text composition, list scrolling, lifecycle, and services when those features exist. Licensed Forge package checks are separate, opt-in consumer tests; they are not a platform build, CI or release prerequisite. Do not substitute property snapshots or passing type checks for visible behavior or actual accessibility output.

## Measure before changing scheduling

Capture comparable baseline and candidate trials on the same board/image/build/display mode. Include input-to-presentation timing where available, repeated-frame evidence, p50/p95/p99, missed deadlines, startup, idle wakeups/CPU, and memory. State sample size, warmup, duration, uncertainty, and capture limitations. Use monotonic clocks and record cross-clock alignment.

Derive display budgets from actual refresh timing. Do not copy Marathon's scheduler priorities, frame phase, or cgroup settings into QNX. Treat chosen numerical limits as proposed budgets until measured and agreed. Revert a change that worsens the targeted result without a justified tradeoff.

## Publish a precise PR report

For every gate report command, candidate identity, environment, outcome, artifact/hash, and limitation. Use `passed`, `failed`, `not run`, `blocked`, or `historical prerequisite`; never count a skipped test as a pass. Include source and skills applied, changed paths, DRI/reviewer roles, security/licensing effects, rollback, and the next smallest slice.

Keep future acceptance boxes unchecked. Keep draft status if required target evidence is missing. Preserve useful authorized work while identifying the exact unresolved gate. Do not claim vendor endorsement, safety certification, deterministic JavaScript execution, or production distribution rights from an RTOS or an evaluation run.
