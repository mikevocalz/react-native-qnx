# Repository skill review exercise

## Scope and prompt

This is a deliberately unsafe proposal used to evaluate the repository's native API and platform-validation guidance. It is not implementation code, a supported API, or a native test. Read `.agents/skills/qnx-native-api/SKILL.md` and `.agents/skills/qnx-platform-validation/SKILL.md`, then review the proposal below as a PR. Identify blocking defects, the smallest useful next slice, and what the reported evidence establishes. Do not execute native code or mark target support complete from the proposal.

## Proposal presented to the reviewer

The proposed Nitro module exposes a synchronous `getTemperature(): number` that performs a fresh QNX device-server read using `MsgSend`. The author calls it from JavaScript as follows and describes it as an asynchronous API:

```ts
function getTemperatureAsync(): Promise<number> {
  return new Promise((resolve) => resolve(nativeTemperature.getTemperature()));
}
```

The native worker copies a timer example that sets `TimerTimeout(CLOCK_REALTIME, _NTO_TIMEOUT_RECEIVE, ...)` before the request. The same module can synchronously wait for a JavaScript callback from its native worker. Shutdown closes the host completion queue, joins the worker, then destroys Hermes.

The author reports a TypeScript signature check and a Linux mocked-provider test. They propose declaring QNX and Marathon support complete. The proposal contains no generated QNX binding receipt, native build or target run.

## Review result

**Disposition: request changes.** The proposal is insufficient for an implementation approval or a QNX/Marathon support claim. This is a document review of the proposal against the two repository skills; it is not a native validation result.

### Blocking findings

1. **The Promise wrapper leaves the device read synchronous.** Its executor calls `nativeTemperature.getTemperature()` immediately on the calling JavaScript thread. Returning a Promise does not move `MsgSend` onto a worker or prevent blocking the runtime owner. A fresh device-server read needs a genuinely asynchronous native operation on an owned execution context. A synchronous getter is appropriate only for an explicitly cached, cheap local value. The proposal also needs to reconcile its synchronous getter with the separately mentioned native worker.

2. **The copied timeout does not establish a bounded request.** `_NTO_TIMEOUT_RECEIVE` does not cover the SEND and REPLY blocking phases that the native API skill requires this `MsgSend` path to address. Specify and verify both phases against the selected SDP headers and server protocol. Define the elapsed-time deadline, clock choice, interruption/error mapping, server unblock behavior, and whether server work can continue after caller timeout. The excerpt does not establish an absolute versus relative deadline, so this review does not attribute a specific clock-jump defect to `CLOCK_REALTIME` alone.

3. **Callback and shutdown dependencies can prevent progress.** A worker that synchronously waits for JavaScript can deadlock when the JavaScript owner is blocked in the getter or joins that worker. Closing completion admission while a producer remains active can also reject completions for already accepted operations. Replace the synchronous callback dependency with an explicit asynchronous protocol; stop/quiesce producers, account for every accepted operation, and keep the owning runtime available for valid completions before destroying it. Define cancellation, late-result rejection, and repeatable disposal. Merely joining before destroying Hermes does not prove this ordering safe.

4. **The binding and support claims lack evidence.** Neither a TypeScript signature nor a Linux mocked provider demonstrates a generated QNX binding, the selected Nitro v1 ABI, native dispatch, or target behavior. The policy describes a constrained generator; this proposal supplies no evidence that its required Promise/callback/error shapes are supported. QNX and Marathon must retain separate, incomplete acceptance gates.

### Smallest useful next slice

Specify one bounded fresh-temperature read: units, capability/permission failures, timeout, cancellation, disposal, and examples covering success, failure, and cleanup. Check that contract against the selected Nitro v1 generator and runtime. Then implement one default-OFF experimental path with one native worker owner and the existing host completion route. Exercise service failure, timeout, callback rejection, and disposal with work pending before expanding the API. If generation support is missing, track that prerequisite explicitly; a hand-authored experimental binding cannot count as completed module support. Keep native-host, QNX cross-build, and each applicable target-runtime gate separate.

### Evidence limits and skill assessment

The author's TypeScript and mocked-provider checks are **reported, not independently verified**: no commands, candidate identity, logs, or artifacts were supplied. At most, those reports concern the checked signatures and mocked behavior. They establish no native liveness, timeout, teardown, performance, or target-support result.

This review ran no code generation, TypeScript/mock tests, native builds, sanitizers, QNX cross-builds, Linux/offscreen/Wayland/Marathon runs, or QNX VM/device runs. No upstream skill installation or source fetch was performed. Exact timer semantics, binding compatibility, and runtime ordering still require current implementation/header inspection and appropriate execution evidence.

Both requested skills were sufficient to identify these blockers, choose a bounded next slice, and prevent overstating evidence. They do not themselves certify an implementation. Additional repository sources consulted were `AGENTS.md` and `docs/platform-modernization/NITRO_V1_API_POLICY.md` in full, plus partial reads of `docs/platform-modernization/SKILLS.md` and `docs/platform-modernization/skills.lock.json`. No delivery plan, linked implementation source, generated binding, or SDP header was inspected. Those omissions limit implementation-specific conclusions; they do not remove the proposal's stated defects.
