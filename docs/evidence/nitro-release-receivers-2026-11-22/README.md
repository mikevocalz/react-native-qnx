# Nitro release receiver safety, 2026-11-22

The experimental window API exposed an actual release crash on the QNX VM:
`first.dispose(); first.snapshot` dereferenced a null abstract receiver in
Nitro alpha136's `HybridFunction` wrapper. Log-only checkpoints passed observer
removal and disposal, then stopped at the getter. The preserved core dump and
GDB backtrace identify that wrapper and a null receiver, rather than an SDK
library fault. No release gate is promoted by this receipt.

The reviewed pnpm patch removes four `NITRO_DEBUG` guard pairs around existing
object, NativeState and receiver-type checks. It retains their diagnostics,
the final-class check, package version, object layout and ABI. Root frozen
installation and generated consumer transport carry the same patch. The fix
throws a JavaScript error for an invalid receiver in release builds.

The actual-Hermes [release control](linux-release-result.json) rebuilt all 20
Nitro core/adapter translation units and the fixture with `-O2 -DNDEBUG`.
The original header control died with SIGSEGV; the patched control passed
valid, disposed getter/method, detached, undefined, plain forged and wrong
NativeState receivers, plus an independent live receiver. The disposed getter
is the first negative case. No Qt or mock JSI runtime participates. That GCC
bootstrap recorded upstream attribute/return warnings and is not described as
a strict warning-clean build.

The production CMake regression uses a separate complete release core because
`RuntimeData::Entry<T>` has a debug-only field. It never links the ordinary
Debug Nitro archive. Compile-time errors require `NDEBUG` and prohibit
`NITRO_DEBUG`. Separately, Apple Clang 21 compiled all 20 core/adapter files and
the fixture with the CI warning profile (`-Wall -Wextra -Wpedantic -Werror`),
`-O2 -DNDEBUG` and only the existing nullability-extension exception: **21/21
objects, zero diagnostics**. This Clang result is compilation, not execution.
The linked project CI result remains a separate check.

On the actual QNX QEMU/TCG/software VM, the same unmodified
[proof source](vm-proof-source.txt) and Hermes bytecode crashed the original
application and passed the patched application in both Default and Hidden
styles. The original disposed-getter assertion was retained. Both runs
completed handle independence, snapshot/inset geometry, title/layout updates,
invalid inputs, close IDs, explicit unmanaged close-guard refusal, subscription
removal and disposal. The separately cross-built Nitro Qt fixture passed its
worker/callback/promise/error/checkpoint/drain/close-race checks. This is GUI
owner window API evidence; it does not claim a dedicated JS owner in the
unmodified playground.

The fixed application used an incremental task graph: 38 affected commands
rebuilt all Nitro core and affected consumers; unaffected Qt/RN objects were
reused. The fresh Qt unit's 26 ELF hashes did not change. Guest verification
matched 188 core files and 14 fixed artifacts. Live `pidin libs` verified all
12 loaded Qt/QPA/QML entries under the new task prefix, with no old Qt fallback.
Both saved PNGs are 1280x800 and were visually inspected.

[Hashes and scope](result.json) identify the artifacts and external raw evidence,
including the original core, crash/checkpoint logs and live mappings. Existing
fonts were reused explicitly. The genuine lean Hermes/JSI artifacts were
imported from their pinned MinSizeRel build; they were not rebuilt or claimed
as O2 release qualification. Two missing allowed platform libraries were staged
separately from the licensed SDK for this private VM. The original provisional
Qt notice collateral was preserved; the later corrected packaging unit carries
the same 26 ELF bytes and its own receipt. No SDK binaries, core dump, private
VM image or runtime libraries are committed here. Physical board, physical
input, performance, publication and full release acceptance remain outside
this evidence.
