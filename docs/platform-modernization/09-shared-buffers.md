# 09 — Measured shared-memory transport and native buffer ownership

## Publication baseline and Nitro v1 gate

This branch is based on main `1baad40a98362ce151ea73bd6f0a6e1d3020926e`. PRs #42, #44, #45, #46 and #48, plus program #47, are merged at this publication snapshot. Earlier source pins below identify inspected evidence rather than unmerged work. No runtime result is promoted to a new revision without revalidation. The current owner policy keeps premium Forge packages optional and outside core build, conformance and release requirements.

Application-facing native work must follow the owner's [Nitro v1 API-first contract](https://github.com/mikevocalz/react-native-qnx/blob/f06bc134d71fe410207525e0348b2fa2481c369b/docs/platform-modernization/NITRO_V1_API_POLICY.md): actual v1 class/registration/constructor APIs, aligned core/runtime/generator versions, typed operations, explicit errors and cleanup, and owned asynchronous work. API sketches below describe semantics; they are not older-version binding implementations or proof of supported code generation.

## Status and decision gate

This PR contains a **documentation-only plan**. It adds no shared-memory allocator, media API, GPU import path, or zero-copy benchmark. Implement shared buffers only if a measured cross-process workload shows that copying is a material bottleneck. Retaining the simpler copied-message path is a valid outcome of the first slice.

Do not put Fabric mutations, ordinary props, small service observations, or an already in-process Forge engine behind shared-memory IPC merely because QNX supports it. Keep the existing Hermes/Fabric/Yoga/Qt Quick renderer. This work belongs beneath a native media/data provider and its typed resource contract.

## Source inspection and lessons

The RN QNX baseline is [`e84b51430c239bc89dbd48ac35afab89f7c77c01`](https://github.com/mikevocalz/react-native-qnx/tree/e84b51430c239bc89dbd48ac35afab89f7c77c01). [#44](https://github.com/mikevocalz/react-native-qnx/pull/44), inspected at `e9026fac8ce8deb5a8e6ba4c35d1dcc5592d478d`, supplies the owned completion route. Its [dispatcher documentation](https://github.com/mikevocalz/react-native-qnx/blob/e9026fac8ce8deb5a8e6ba4c35d1dcc5592d478d/docs/nitro/QT_DISPATCHER.md) requires native producers to stop before host teardown; draining callbacks does not end external buffer ownership.

QNX's [shared-memory/message-passing guidance](https://qnx.com/developers/docs/8.0/com.qnx.doc.neutrino.sys_arch/topic/ipc_Shared_memory_and_messages.html) describes combining bulk memory with small synchronization messages. Its synchronous example keeps data stable while one side is blocked, and it explicitly reserves the added complexity for unusually high bandwidth. An asynchronous RN pipeline needs its own lease/publication protocol; the example's lifetime does not transfer automatically to JavaScript callbacks or GPU work.

The user's archive is pinned at [`b1fd3e9410c1a6a0d7217c3be7e324e900470d2a`](https://github.com/Himmele/My-Blog-Repository/tree/b1fd3e9410c1a6a0d7217c3be7e324e900470d2a/QNX%20Neutrino%20RTOS/Programming). Its inspected [`MemoryManager.cpp`](https://github.com/Himmele/My-Blog-Repository/blob/b1fd3e9410c1a6a0d7217c3be7e324e900470d2a/QNX%20Neutrino%20RTOS/Programming/MemoryManager/MemoryManager.cpp) casts addresses to `unsigned int` and rounds allocations to four bytes. It is educational material, not the AArch64 allocator for this platform. Use reviewed target APIs and explicit ownership; never transmit a process pointer as a buffer reference.

Marathon's [security model at `c2c1b2fe32aeaa51edf75a45045cbfbbc08c4ac5`](https://github.com/patrickjquinn/Marathon-Shell/blob/c2c1b2fe32aeaa51edf75a45045cbfbbc08c4ac5/docs/SECURITY_MODEL.md) places managed apps in isolated environments, including private temporary/shared-memory mounts. A QNX shared-memory name or Linux file-descriptor integer is not a portable cross-process capability. Both targets need their own reviewed transport and confinement proof.

## First slice: establish whether sharing is worthwhile

Benchmark the actual workload with copied messages first. Record payload sizes/rate, copies and copied bytes per frame, CPU time, latency percentiles, peak resident memory, and end-to-end presentation/input behavior. Include small controls and representative large payloads rather than optimizing a synthetic maximum alone. Use the same device, release build, application sequence, display configuration, and tracing overhead for comparisons.

Before implementation, the owner and reviewer must record a workload-specific success threshold and maximum memory budget. If sharing does not materially improve that measured bottleneck, keep the copied path and close the optimization slice with its evidence. “Zero copy” is a claim about a specific boundary, not an automatic description of decoding, Nitro conversion, Qt texture upload, and display presentation together.

## Proposed native protocol and resource model

Use a bounded pool owned by the provider/broker. Negotiate buffer count, total byte budget, dimensions, stride, pixel/sample format, and supported access modes. Device profiles from Plan 11 supply real limits. An initial fixture may use two slots and a 16 MiB total budget; those are test settings, not universal product defaults.

Each descriptor identifies a registered region, service generation, slot/lease generation, byte offset, and length. The receiver verifies the authorized region, generation, format, mapping size, and arithmetic before reading. Check `offset <= regionSize` and `length <= regionSize - offset`; reject invalid stride/dimension products before allocation or access. Region lookup is scoped to the authenticated session. A numeric value supplied by JS cannot authorize a new mapping.

A slot moves through explicit ownership: Free → Writing → Ready → Leased → Free. The producer cannot overwrite Leased data. A release names the exact lease generation and is idempotent; an old release must never free a newly reused slot. The first implementation should keep ownership metadata in the broker and exchange small control messages, reducing the need for a complex shared allocator.

Prove publication/visibility against the target's supported synchronization and cache-coherency rules. If process-shared pthread primitives are selected, initialize them explicitly with `PTHREAD_PROCESS_SHARED` as required by the [QNX 8 migration guide](https://www.qnx.com/developers/docs/8.0/com.qnx.doc.qnxsdp.migration/topic/kernel.html). Do not assume a default C++ mutex is process-shared or that a DMA buffer has ordinary CPU-memory semantics. GPU/Screen/Wayland buffer import and fence handling require a separate measured slice.

Backpressure belongs in the contract. For live preview, a full pool can drop the newly produced frame and increment an observable counter; it cannot overwrite a leased frame. Recording or lossless export must instead reject unsupported throughput or use a separately bounded blocking policy. Do not silently apply preview dropping to a lossless workflow.

## Proposed application contract and examples

Expose a native resource object, not raw addresses or mapping names. The following is **proposed TypeScript**, not generated Nitro code or an existing package API:

```ts
interface FrameLease {
  readonly width: number;
  readonly height: number;
  readonly strideBytes: number;
  readonly format: 'rgba8' | 'bgra8';
  readonly byteLength: number;
  copyToArrayBuffer(): Promise<ArrayBuffer>;
  release(): Promise<void>;
}
interface FrameStream {
  next(): Promise<FrameLease>;
  dispose(): Promise<void>;
}
```

The ready stream owns negotiation. Frame metadata is immutable and cheap to read. Pending `next()` calls use bounded admission and a documented wait budget; disposal rejects them. `release()` is idempotent and ends this resource's use; future methods reject with an Error. A completed copy remains valid independently of the lease.

Define the race contract explicitly: `release()` closes admission for new operations on that lease, while a copy or native presentation already admitted retains its own reference/pin until completion. Resolve release only after those admitted native users finish and the exact slot-generation release is acknowledged; a retained JavaScript handle alone does not keep an operation admitted. `stream.dispose()` stops the producer, rejects pending/new `next()` calls, marks all outstanding leases closed to new operations, and completes their release protocol. It waits for admitted native users, not for the application to call `release()` on every retained handle. A later `release()` is still idempotent. No worker or GPU user may read an unmapped/recycled slot. The first CPU-copy slice has bounded payload and queue sizes; graphics presentation remains a separate fence/lifetime gate. If a native user cannot finish or be safely contained, report disposal failure and retain/quarantine the mapping rather than reporting success after unmapping live storage. Generated support for these shapes must be audited: the existing PlatformInfo generator supports only its small synchronous proof.

**Native consumption without eager JS byte conversion:**

```ts
const frame = await stream.next();
try {
  await preview.present(frame);
} finally {
  await frame.release();
}
```

Here `preview` is a proposed native consumer integrated with the existing Qt component backend. Its completion must mean it has finished retaining the resource, including any required graphics fence, before release can recycle the slot. Do not implement a second renderer.

**Explicit independent copy for a caller that needs bytes:**

```ts
const frame = await stream.next();
let bytes: ArrayBuffer;
try {
  bytes = await frame.copyToArrayBuffer();
} finally {
  await frame.release();
}
await exportBytes(bytes);
```

**Failure and stream cleanup:**

```ts
try {
  await showPreview(stream);
} catch (error) {
  showMediaError(error instanceof Error ? error : new Error(String(error)));
} finally {
  await stream.dispose();
}
```

These are review examples using injected consumers. An optional future zero-copy JS view must pin its backing mapping for every surviving JS reference and finalizer. Explicit release cannot invalidate a live ArrayBuffer. Start with the safer explicit-copy API until that lifetime is independently proven. Account native allocation size to the runtime using the supported Nitro mechanism; do not hide a large pool from memory pressure.

## Delivery slices, dependencies, and roster

| Slice / proposed owner       | Files and deliverable                                                             | Independent reviewer         |
| ---------------------------- | --------------------------------------------------------------------------------- | ---------------------------- |
| Measurement, performance DRI | Proposed benchmark fixture and `docs/evidence/` copy baseline                     | Qt performance reviewer      |
| Protocol/lease engineer      | Proposed `native/platform/qnx/services/buffers/` and focused portable lease types | QNX/security engineer        |
| Nitro/C++ engineer           | Proposed specs and resource bindings under `packages/qnx-nitro-runtime/`          | JS/runtime lifetime engineer |
| Marathon engineer            | Separate `native/platform/linux-host/services/buffers/` transport                 | Shell confinement reviewer   |
| QA engineer                  | Proposed malformed-descriptor, crash, pressure, and disposal tests                | Independent regression owner |

All human assignments are open. First prove copied baseline; then one native producer/consumer with bounded leases; then a typed runtime resource; finally consider graphics imports or a JS view. A proposed `RNQNX_ENABLE_SHARED_BUFFERS` flag defaults OFF and rejects unsupported production targets. Do not enable it transitively with a generic service flag.

Dependencies: Plans 03 and 11 for measurement/device budgets, Plans 06–08 for authorized sessions, bounded control transport, and recovery; Plan 01/#44 for runtime lifetime. Plan 10's in-process Forge bindings do not depend on this optimization unless a measured use case establishes that dependency.

## Skills and proof requirements

Read and applied in order: Margelo [`api-design`, `build-nitro-modules`, `cpp`](https://github.com/margelo/react-native-skills/tree/1e9e17be6f41c838db472d6a4b943fc26355a3be/skills) at `1e9e17be6f41c838db472d6a4b943fc26355a3be`, then Callstack [`react-native-best-practices`](https://github.com/callstackincubator/agent-skills/blob/59e9d17446a248b21d86f078fcd0bd2d36db1ca4/skills/react-native-best-practices/SKILL.md) at `59e9d17446a248b21d86f078fcd0bd2d36db1ca4`.

| Skill                         | Decision changed                                                    | Evidence status                                |
| ----------------------------- | ------------------------------------------------------------------- | ---------------------------------------------- |
| `api-design`                  | Resource ownership and explicit copying are visible in the contract | Source review; proposed API only               |
| `build-nitro-modules`         | Native buffers remain resource objects; no eager giant structs      | Source review; generator/runtime proof pending |
| `cpp`                         | RAII mappings/leases, checked sizes, explicit release ownership     | Source review; compile and runtime pending     |
| `react-native-best-practices` | Require a measured copy bottleneck before complexity                | Source review; no benchmark executed           |

Require copy-versus-release, copy-versus-stream-dispose, retained-handle-after-dispose, and presentation-versus-release tests against the stated admission/pinning semantics. Require tests for out-of-range descriptors, overflow, wrong session, stale generations, double release, consumer crash, producer restart, pool exhaustion, slow consumer, retained JS copy, and stream disposal with outstanding leases. A broker may reclaim a dead consumer's leases only after establishing that the owning process/context is gone. A retained live consumer is backpressure, not permission to reuse memory.

Run repeated acquire/release and restart cycles; assert bounded slots/mappings and no use-after-free under host sanitizers. Inspect real physical QNX lifetime/cache behavior and separate Marathon confinement/descriptor-transfer behavior. Host sanitizers do not prove graphics fences, hardware DMA coherency, or QNX kernel semantics.

Verified baseline command names include `pnpm validate:dependencies`, `pnpm typecheck`, `pnpm test:js`, `pnpm test:cpp:host`, `pnpm validate:qnx-nitro`, and `pnpm qnx:validate:805`. Add the actual buffer fixtures to real test targets and record exact commands with logs. **No proposed buffer test, native build, or benchmark was run by this documentation PR.** The physical QNX and Marathon evidence rows remain independently pending.

## Security, rollout, and rollback

Use least-privilege mappings and access rights; an unpredictable name alone is not authorization. Avoid physical-memory access or broader capabilities than the workload requires. Do not log captured payloads. Review new module licenses and deployment rights; no archived allocator or proprietary source is copied here.

Roll out to one conformance workload only after measured benefit and lifetime gates pass. Rollback closes producers, finishes consumers, releases leases, then disables the feature and restores the bounded copied path. Do not unmap live buffers as a rollback shortcut. The next PR is the comparative benchmark and explicit go/no-go record, followed by a lease implementation only if that evidence warrants it.
