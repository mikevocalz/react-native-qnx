# ADR-0017: Compile public-source Qt for QNX at -O2

## Status

Accepted, 2026-11-12. The owner delegated this decision. The first build with
the new flags ran the same day; see
[the -O2 receipt](../evidence/pi400-public-qt-o2-2026-11-12-a79e16a.md)
(runtime tarball
`9ead47f44e7ba79ab9abc08ef59279d5d49a28693b03676988b82bd805ec0cc4`).

## Context

The public-source Qt 6.12.0 build for QNX
([receipt](../evidence/pi400-public-qt-2026-11-12-b51b49c.md)) used the same
configure options as the evaluation build, including `-release -optimize-size`.
The receipt noted that the Release flags carry no `-O` level and left the fix
open.

The build tree in the `qnx805-build` container
(`/home/qnxbuild/qt-6.12.0-public/qnx-build/CMakeCache.txt`), read on
2026-11-12, shows:

| Cache entry                  | Value                     |
| ---------------------------- | ------------------------- |
| `CMAKE_BUILD_TYPE`           | `Release`                 |
| `CMAKE_CXX_FLAGS_RELEASE`    | `-DNDEBUG   -DNDEBUG`     |
| `CMAKE_C_FLAGS_RELEASE`      | `-DNDEBUG   -DNDEBUG`     |
| `CMAKE_CXX_FLAGS_MINSIZEREL` | `-DNDEBUG   -DNDEBUG -O2` |
| `FEATURE_optimize_size`      | `ON`                      |
| `FEATURE_optimize_full`      | `OFF`                     |

No compile line in `qnx-build/build.ninja` contains `-O0` to `-O3`, `-Os` or
`-Oz`. GCC's default without an `-O` flag is `-O0`, so the Qt libraries on the
Pi 400 were compiled unoptimised. The host-tools build in the same container has
`-DNDEBUG -O2`, because it uses the system GCC.

The cause is in Qt's own CMake. `qtbase/cmake/QtCompilerOptimization.cmake`
sets `QT_CFLAGS_OPTIMIZE` (`-O2`) and `QT_CFLAGS_OPTIMIZE_FULL` (`-O3`) for
`QCC`, but sets `QT_CFLAGS_OPTIMIZE_SIZE` only for GCC and Clang.
`qt_internal_set_up_config_optimizations_like_in_qmake` in
`QtFlagHandlingHelpers.cmake` removes CMake's default `-O` flags and, when
`optimize_size` is on, puts `QT_CFLAGS_OPTIMIZE_SIZE` into the Release flags.
For qcc that variable is empty, so Release ends with no `-O` level. MinSizeRel
falls back to `QT_CFLAGS_OPTIMIZE` when the size flag is empty, which is why
it shows `-O2`.

The QNX headers also warn during this build that `_FORTIFY_SOURCE` falls back
to 0, because fortification needs optimisation.

## Decision

1. Qt for QNX is compiled at `-O2`, Qt's normal Release level. Nothing in the
   evidence argues for `-O3` or `-Os`: no timing, size or frame-pacing
   measurement exists for this build, and `-Os` is not available for qcc
   through Qt's configure anyway.
2. `scripts/qt-public/configure-qnx.sh` drops `-optimize-size`, passes
   `-DFEATURE_optimize_size=OFF -DFEATURE_optimize_full=OFF`, and after
   configure exits with an error unless `CMAKE_C_FLAGS_RELEASE` and
   `CMAKE_CXX_FLAGS_RELEASE` both contain `-O2`.
3. The qualified Qt runtime, and any runner or package built on it, must come
   from a build made with these flags. The 2026-11-12 runtime
   (tarball `64afc468edb2641fbd3b121805f2e0b62140fa2086a4b0ab2486eae53ef49312`)
   is unoptimised. Its hashes stay valid as evidence of that build, but they
   are not the qualified artifact hashes; the next build replaces them.
4. The host-tools build keeps its current flags (`-O2` from the system
   compiler).

## Consequences

- Every Qt library hash in the closure manifest changes on the next build. The
  receipt's Pi 400 runs (hello world, Nitro/Qt, pressable) must be repeated on
  the rebuilt runtime before its hashes are recorded as qualified.
- `_FORTIFY_SOURCE` can take effect in Qt code once `-O2` is present. The
  `-O2` build log has no `_FORTIFY_SOURCE` warning; the `-O0` log had 2248.
- With `optimize_size` off, Qt's own `qt_internal_add_optimize_full_flags()`
  compiles QtCore and QtGui at `-O3` (647 of 2992 compile lines in the `-O2`
  build); every other module is at `-O2`. This is upstream behaviour and was
  kept.
- Library size changed: the 16 ELF files went from 122.8 MB to 51.3 MB in
  total. There was no size budget to compare against.
- The RN executables and Hermes are separate CMake builds with their own build
  types and are not affected by this ADR.

## Alternatives considered

- **Keep `-optimize-size`.** Rejected: with qcc in Qt 6.12.0 it yields no
  optimisation at all.
- **`-Os` through `CMAKE_CXX_FLAGS_RELEASE` with
  `QT_USE_DEFAULT_CMAKE_OPTIMIZATION_FLAGS`.** Possible, but no measurement
  shows a size problem on the Pi 400, and it moves Qt off its normal Release
  flags.
- **`-O3` (`-optimize-full`).** No measurement shows a CPU-bound Qt path that
  would gain from it. It can be revisited with frame-pacing evidence.

## Related files

- `scripts/qt-public/configure-qnx.sh`
- `scripts/qt-public/README.md`
- `docs/packaging.md`
- `docs/evidence/pi400-public-qt-2026-11-12-b51b49c.md`
- `docs/evidence/qt-public-runtime-closure-2026-11-12-b51b49c.json`

## Notes

Qt upstream may add a qcc size flag later. If the Qt pin moves, re-read
`QtCompilerOptimization.cmake` before changing these options; the configure
check catches a missing `-O2` either way.
