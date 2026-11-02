# Experimental Nitro v1 code generation for QNX and MarathonOS

Status: **working, limited TypeScript-to-C++ binding-generation proof**.
This is **not an upstream Nitrogen QNX/Marathon backend** and does not
establish production device compatibility.

The inspected Nitro v1 alpha `nitrogen/src/getPlatformSpecs.ts`
currently permits `apple` and `android`. It does not expose QNX or
Marathon targets. To avoid maintaining two hand-written C++ registration
blocks, this PR introduces a constrained TypeScript AST codegen adapter
with explicit fail-closed validation.

## Source of truth

`packages/qnx-nitro-runtime/specs/PlatformInfo.nitro.ts` defines a
shared Nitro HybridObject-shaped interface for `platform`,
`architecture` and synchronous `add(left, right)`.

`scripts/nitro-platform-codegen.mjs` uses the installed TypeScript
parser to emit two actual Nitro **v1 HybridClass** descriptor sources:

- `packages/qnx-nitro-runtime/cpp/generated/QnxPlatformInfo.binding.cpp`
- `native/platform/linux-host/nitro/generated/MarathonPlatformInfo.binding.cpp`

Both use the real v1 `HybridClass::create`, constructor registration,
`registerHybridProperty` and `registerHybridMethod` API. QNX uses
its existing native C++ methods; Marathon has a separate Linux native
implementation. This is source generation, not a renderer fork or
repackaged QNX ELF. The Marathon implementation is not yet linked to
a runnable Linux host; that needs its own CMake/runtime PR.

## Reproducible commands

```sh
pnpm install --frozen-lockfile  # Nitro v1 alpha stack from PR #38
pnpm nitro:platform:check       # deterministic generated output
pnpm nitro:platform:generate    # regenerate after spec is reviewed
pnpm codegen:check
pnpm test:js
pnpm typecheck
```

The generator intentionally supports **only** the reviewed proof:
two required string getters and one two-argument numeric method.
It rejects optional properties, callbacks, Promise returns, unsupported
types, unknown class names and additional methods instead of silently
mis-generating native ABI. Do not expand this into a general compiler
without a source-backed Nitrogen extension and reviewed native ABI.

## Independent verification gates

- [ ] Actual Nitro 1 alpha npm packages installed in a frozen lockfile
- [ ] `codegen:check` and all JS/TypeScript tests pass
- [ ] QNX C++20/JSI compile on SDP 8.0.5 and Hermes probe runs on Pi
- [ ] Marathon Linux/musl C++20/JSI compile and runtime probe
- [ ] Negative tests for unsupported type signatures and stale outputs
- [ ] Thread/lifetime/cancellation tests before asynchronous APIs
- [ ] Upstream Nitro/Nitrogen maintainer review of new platforms

Do **not** equate an AST code generator with official v1 Nitrogen support,
or a Linux host test with physical MarathonOS compatibility. See
`docs/nitro/API_CONTRACTS.md` and the Margelo
`api-design`, `build-nitro-modules` and `cpp` skills.

## Rollback

Revert this experimental generator and both generated sources,
restore the previous `QnxPlatformInfo::Class::get` block and QNX
CMake source list. Existing QNX and Qt/Fabric functionality remains.
