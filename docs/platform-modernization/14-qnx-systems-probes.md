# Plan 14 — Modern QNX systems regression probes

## Publication baseline and Nitro v1 gate

This branch is based on main `1baad40a98362ce151ea73bd6f0a6e1d3020926e`. PRs #42, #44, #45, #46 and #48, plus program #47, are merged at this publication snapshot. Earlier source pins below identify inspected evidence rather than unmerged work. No runtime result is promoted to a new revision without revalidation. The current owner policy keeps premium Forge packages optional and outside core build, conformance and release requirements.

Application-facing native work must follow the owner's [Nitro v1 API-first contract](https://github.com/mikevocalz/react-native-qnx/blob/f06bc134d71fe410207525e0348b2fa2481c369b/docs/platform-modernization/NITRO_V1_API_POLICY.md): actual v1 class/registration/constructor APIs, aligned core/runtime/generator versions, typed operations, explicit errors and cleanup, and owned asynchronous work. API sketches below describe semantics; they are not older-version binding implementations or proof of supported code generation.

## Status and purpose

This is a **documentation-only implementation plan**. It specifies bounded C++20/QNX 8 probes for the systems concepts in the supplied archive that are not fully covered by the UI and service plans. No probe, native API, scheduler change, driver, or device test is implemented here.

The publication baseline is [`1baad40a98362ce151ea73bd6f0a6e1d3020926e`](https://github.com/mikevocalz/react-native-qnx/tree/1baad40a98362ce151ea73bd6f0a6e1d3020926e), including merged [#42](https://github.com/mikevocalz/react-native-qnx/pull/42), [#44](https://github.com/mikevocalz/react-native-qnx/pull/44), [#45](https://github.com/mikevocalz/react-native-qnx/pull/45), and the default-OFF input implementation [#46](https://github.com/mikevocalz/react-native-qnx/pull/46). Preserve those implementations and their revision-specific evidence. Systems probes supplement the shared Fabric/Hermes/Yoga/Qt Quick runtime; they do not replace its observable application tests.

## Archive inventory and inspection boundary

The [supplied revision](https://github.com/Himmele/My-Blog-Repository/tree/b1fd3e9410c1a6a0d7217c3be7e324e900470d2a/QNX%20Neutrino%20RTOS/Programming) is `b1fd3e9410c1a6a0d7217c3be7e324e900470d2a`; the Programming tree is `e6a12a8f5168711253e1432f169923298694b37b`. Its recursive inventory contains **19 categories and 530 files**, including generated/build outputs. That is a path inventory, not a claim that 530 source bodies or the bundled PDFs were reviewed.

Representative bodies read for this work include `Neutrino/{Threads,ThreadLocalStorage,Mutex_POSIX,Atomic,Scheduler,Signals_sync,TimerTimeout}.c`, `SMP/SMP.C`, `File Systems/FS.C`, `Debugging/DEBUG.C`, `Dynamic Linking/LoadShLib.c`, `Process Manager/fork_exec.c`, and `MultiThreadedResourceManager/MultiThreadedResourceManager.c`. Earlier review also inspected `MemoryManager/MemoryManager.cpp`. These examples motivate modern fixtures; none is vendored or executed.

| Archive category | Disposition and implementation owner |
| --- | --- |
| AdvancedGraphicsTest | Legacy GF graphics; retain concepts, use existing Qt/Screen paths in 01/03/12 |
| Boost | Audit a real dependency need; prefer existing C++20 facilities; no old Boost bundle import |
| Character I-O | Bounded descriptor/terminal behavior and error probes here |
| DLL-Test | Review C/C++ ABI boundaries; isolated loader fixtures here |
| Debugging | Current symbols, crash receipts and trace provenance; 01/03/08 |
| Dynamic Linking | Versioned initialization and safe lifetime/unload tests here |
| File Systems | Permission, creation mode, offsets, short I/O and cleanup probes here |
| MemoryBenchmark | Reproducible measurement in 03/09; no inherited benchmark claims |
| MemoryManager | Replace allocator assumptions with size/alignment/ownership tests; 09 owns shared buffers |
| MultiThreadedResourceManager | Bounded pool, message validation and concurrent-client tests; 07 owns transport |
| NetworkBenchmark | Bounded loopback and fault tests; 03 measures, 06 owns public APIs |
| Neutrino | Threads, synchronization, atomics, TLS, timers and signals here; IPC in 07 |
| Photon | Historical UI reference only; no Photon renderer or widget port |
| Process Manager | Spawn, child failure/reaping and descriptor inheritance; recovery stays in 08 |
| RPC | Typed versioned service semantics in 06/07; no unreviewed legacy RPC stack |
| Resource Managers | Namespace and authorization probes here; production services stay in 06/07 |
| ResourceManagerClient | Denied access, malformed requests and exact returned byte counts |
| SMP | Measured affinity/priority behavior; no interrupt-lock or runmask transplant |
| SingleThreadedResourceManager | Baseline protocol/permission behavior, compared with bounded concurrent service |

## Proposed probe groups

**Threads, synchronization, atomics and TLS.** Use returned thread identifiers, explicit startup handshakes, finite work and joins. Test mutex ownership, condition predicates/spurious wakeups, semaphore limits, cancellation cleanup, and TLS isolation/destructors on thread exit. Use standard atomics with documented memory ordering for shared in-process state; `volatile` is not synchronization. Test supported primitives rather than assuming every C++20 library feature is implemented by the selected QNX toolchain. The archive's sleeps and hardcoded thread IDs are not readiness or cleanup guarantees.

**Scheduling, inversion and SMP.** Capture default priorities, policies, CPU placement and contention first. A controlled inversion fixture compares the selected mutex protocol under an approved test policy, with a bounded external watchdog. Compare floating threads with an explicitly supported runmask only when measurement justifies it. QNX scheduling depends on priorities and eligible clusters; consult [scheduling](https://www.qnx.com/developers/docs/8.0/com.qnx.doc.neutrino.sys_arch/topic/kernel_Scheduling_priority.html) and [multicore guidance](https://qnx.com/developers/docs/8.0/com.qnx.doc.neutrino.prog/topic/multicore.html). Do not copy Linux priorities, elevate the UI indiscriminately, or use the archive's interrupt locks in application code. Hardware interrupt migration is a separate driver-specific IST investigation under the [QNX 8 migration guide](https://www.qnx.com/developers/docs/8.0/com.qnx.doc.qnxsdp.migration/topic/kernel.html).

**Clocks, timers and signals.** Use monotonic elapsed/deadline measurements, bounded timer repetition, overrun observations, and teardown races. Separate wall time from duration. Validate [clock-specific condition waits](https://qnx.com/developers/docs/8.0/com.qnx.doc.neutrino.lib_ref/topic/p/pthread_cond_clockwait.html) against installed headers. Block the chosen signals before creating workers and handle them in a designated [sigwaitinfo](https://www.qnx.com/developers/docs/8.0/com.qnx.doc.neutrino.lib_ref/topic/s/sigwaitinfo.html) thread; never call JSI or Qt views from an asynchronous signal handler. Plan 07 retains SEND/REPLY deadline and unblock tests.

**Resource managers and device I/O.** Create a temporary, access-restricted test endpoint. Test open modes, per-open offsets, read/write/devctl validation, truncated/oversized payloads, unknown commands, denial, disconnect and bounded concurrency. Report bytes actually committed, including short I/O. The inspected archive manager exposes `0666` and reports the requested write count while copying at most 4095 bytes; neither behavior becomes the new default. Inspect dispatch/framework locking before alleging or fixing a shared-state race. Follow [QNX resource-manager security guidance](https://qnx.com/developers/docs/8.0/com.qnx.doc.security.system/topic/manual/write_resource_manager.html).

**Message queues and pipes.** Compare bounded native IPC fixtures with Plan 07's request/reply transport. Message queues preserve message boundaries: test queue capacity, `EMSGSIZE`, nonblocking admission, timed waits and cleanup including unlink. Pipes are byte streams: test partial transfers, nonblocking `EAGAIN`, EOF after the last writer closes, `EPIPE`/signal handling after readers close, and descriptor cleanup. Use a monotonic outer watchdog even where an API's timed operation takes a realtime absolute deadline. The [QNX 8 migration guide](https://www.qnx.com/developers/docs/8.0/com.qnx.doc.qnxsdp.migration/topic/kernel.html) moves message queues into the kernel and removes the separate `libmq`; audit actual headers/linkage instead of importing an old library recipe. These are native comparison controls, not a second public transport API.

**Memory, filesystem and network boundaries.** Exercise AArch64 pointer/offset width, checked allocation arithmetic, required alignment, allocation failure and RAII cleanup. Do not reuse the archived integer-address allocator. Test explicit file creation permissions, EOF, unsupported seeks, interrupted/partial I/O, and bounded temporary storage. Network fixtures use loopback, partial transfers, disconnects and explicit timeout/cancellation; hostname resolution and blocking calls stay off the GUI/JS owner. These fixtures validate adapters, not replacement networking or persistence libraries.

**Processes and dynamic libraries.** Prefer [posix_spawn](https://qnx.com/developers/docs/8.0/com.qnx.doc.neutrino.lib_ref/topic/p/posix_spawn.html) with explicit file actions; exercise launch failure, signal/nonzero exit, reaping and descriptor leaks. Keep potentially blocking initialization off the UI owner. Test a versioned leaf-DSO contract, missing symbols, ABI mismatch, initialization failure and dependency closure. [dlopen](https://qnx.com/developers/docs/8.0/com.qnx.doc.neutrino.lib_ref/topic/d/dlopen.html) can encounter partially initialized dependencies; [dlclose](https://qnx.com/developers/docs/8.0/com.qnx.doc.neutrino.lib_ref/topic/d/dlclose.html) does not make retained function pointers safe. Stop workers and release callbacks/TLS-owned resources before unload. Registered Nitro modules remain process-lifetime unless a real unregister/lifetime mechanism is demonstrated.

## Nitro v1 API-first boundary

Keep these diagnostic probes as native executables; do not expose arbitrary scheduling, file paths, `devctl`, spawning, or dynamic loading to application JavaScript. If an application-facing feature emerges, Plan 06 must first approve its domain contract, failure/cleanup examples and actual **Nitro v1** bindings.

The selected graph pins core/adapter/Nitrogen to `1.0.0-alpha.nightly.105.g69d365fc9b18`. Its [upstream template](https://github.com/margelo/nitro/blob/ea71f21b65210fc7f11c593a5adb1dd12b3cdd2e/packages/template/src/specs/Example.nitro.ts) uses `export declare class ... extends HybridObject`, platform decorators and generated class exports. [Decorator source](https://github.com/margelo/nitro/blob/ea71f21b65210fc7f11c593a5adb1dd12b3cdd2e/packages/nitro-core/js/src/HybridObjectDecorators.ts) treats annotations as codegen inputs; [platform parsing](https://github.com/margelo/nitro/blob/ea71f21b65210fc7f11c593a5adb1dd12b3cdd2e/packages/nitrogen/src/getPlatformSpecs.ts) accepts Apple/Android. Do not invent `@Qnx`, use v0.37 interface conventions as the implementation API, or fabricate general QNX generation. The existing PlatformInfo adapter is a narrow exception.

Reuse the existing v1 smoke after the native installer, on its owning runtime:

```js
const { NitroModules } = require('nitro-core');
const QnxPlatformInfo = NitroModules.getHybridClass('QnxPlatformInfo');
const info = new QnxPlatformInfo();
if (info.add(20, 22) !== 42) throw new Error('Native contract mismatch');
```

No new diagnostic module is implied. The inspected [HybridClassRegistry](https://github.com/margelo/nitro/blob/ea71f21b65210fc7f11c593a5adb1dd12b3cdd2e/packages/nitro-core/native/cpp/include/NitroModules/HybridClassRegistry.hpp) exposes registration/lookup without an unregister API, reinforcing the unload boundary.

## Slices, roster and skills

| Slice | Proposed owner/files | Independent reviewer |
| --- | --- | --- |
| Portable finite fixtures | C++ systems DRI; proposed `tests/native/qnx-systems/` | C++ lifetime reviewer |
| QNX-only process/resource fixtures | QNX engineer; same directory with target-specific translation units | Security/IPC reviewer |
| Build and evidence harness | Build engineer; CMake registration and proposed probe-run script | QNX regression owner |
| Application integration checks | Runtime engineer; existing #44 tests and Plan 12 workload | Qt/JSI reviewer |

Human assignments remain unassigned. Start with threads/TLS/short-I/O fixtures, then target-native boundaries, then measured scheduling/loader cases. A proposed `RNQNX_BUILD_QNX_SYSTEMS_PROBES` flag defaults OFF; QNX production fixtures reject unsupported targets, while portable host tests remain explicitly labeled. No broad capability grants or boot changes are added.

Read/applied skills: Margelo `api-design` → `build-nitro-modules` → `cpp` at `1e9e17be6f41c838db472d6a4b943fc26355a3be` establish the domain boundary, generator audit and RAII ownership; Callstack `react-native-best-practices` at `59e9d17446a248b21d86f078fcd0bd2d36db1ca4` requires measured contention/latency. Generic skill examples yield to the audited v1 source. Source reading is not CLI installation, native execution or target proof.

## Acceptance, handoff and rollback

Each fixture must have a finite budget, external termination path, exact pass/fail/unsupported status, cleanup assertions and retained diagnostics. Require negative cases to fail observably, no orphaned workers/children/descriptors, and repeatability across documented trials. Record latency distributions, CPU/memory and configuration rather than claiming hard-real-time JavaScript or universal SMP gains.

Existing commands include `pnpm validate:dependencies`, `pnpm test:cpp:host`, `pnpm validate:qnx-nitro`, and `pnpm qnx:validate:805`; none yet implements this proposed suite. Implementation must register actual CMake/CTest targets and publish exact build/run commands and artifact hashes. **No native tests or device experiments were run for this plan.** Separate portable host/sanitizer, QNX cross-build, VM, physical QNX and relevant Marathon regression evidence.

Use the program catalog's Plan IDs for handoff: 01 owns runtime/recovery setup; 03 owns performance methodology; 06 public services; 07 IPC; 08 supervision; 09 buffers; 10 generic package hooks and optional licensed-consumer compatibility; 12 non-premium integrated conformance. No premium fixture is a prerequisite. Plan 15 supplies the complementary official-document/example inventory. Reuse their fixtures instead of creating competing implementations.

Review SDK/module redistribution rights and redact crash data; copy no archived source, binaries or PDFs. Rollback disables the probe option, stops only owned fixtures, removes their temporary endpoints, and leaves application/runtime/boot behavior intact. The next PR is a finite thread/TLS and short-I/O probe slice with truthful host and target results.
