# Gesture input cancellation — Linux native receipt

Date: 2026-11-17. Base: `5f3563dfd4d30f8915b50dbdfa2f438751480f0d`.
DRI: PR #149 native-runtime workstream. Independent reviewer: SVG workstream
agent, with a separate root review of contact ownership.

## Change and evidence

Detaching, moving or disabling a handler now cancels its current contact before
discarding the old attachment. Cancellation keeps the last coordinates and
pointer count in the terminal event. A detector removes its handlers as a batch,
so cancelling an exclusive predecessor cannot activate another handler being
removed. Window deactivation and touch cancellation cancel every handler,
including arbitrary Qt pointer IDs and completed taps waiting without contacts.

The registry snapshots events while changing state, then delivers them after the
mutation completes. A synchronous native sink can therefore drop handlers without
invalidating recognizer references or map iterators. Nested events remain ordered
and are delivered without recursive callbacks. A cancelled handler can accept a
fresh contact while another unrelated contact remains down; a failed recognizer
still holding its own contact stays terminal until that contact leaves.

The original implementation failed 15 assertions in the first three new
regression cases. Independent review then supplied a removed-detector reproducer
that incorrectly activated a waiting tap and a cancellation-sink reproducer that
triggered an AddressSanitizer heap-use-after-free. Expanded tests reproduced both
findings and the held-contact reattachment failure before their fixes. After the
fixes, the complete 24-case native test executable passed. The independent
reviewer separately rebuilt and reran both reproducer programs successfully.

```sh
g++ -std=c++20 -O1 -g -Wall -Wextra -Wpedantic -Werror \
  -fsanitize=address,undefined -fno-sanitize-recover=undefined \
  -fno-omit-frame-pointer -Ivnext/include \
  vnext/src/gesture/GestureHandlerCore.cpp \
  tests/native/gesture-handler/GestureHandlerCoreTest.cpp \
  -o /tmp/rnqnx-pr149-gesture-after
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
  UBSAN_OPTIONS=halt_on_error=1 /tmp/rnqnx-pr149-gesture-after
```

Observed with GCC 14.2.0 on Linux: exit 0, `RNQNX_GESTURE_CORE_OK`, no ASan or
UBSan diagnostics. Leak detection is disabled because LeakSanitizer cannot run
under the execution environment's ptrace supervision; this is not leak evidence.

## Sources and decisions

| Source read | Decision |
| --- | --- |
| Installed `react-native-gesture-handler@3.4.0`, `android/src/main/java/com/swmansion/gesturehandler/core/GestureHandler.kt`, `isEnabled`, `cancel()` and `fail()` | External cancellation emits `CANCELLED` for both begun and active handlers; recognition failure remains distinct. |
| Repository `qnx-native-api` and `qnx-platform-validation` skills | Preserve the current renderer, opt-in switches and separate host/target acceptance. |
| Margelo `api-design`, `build-nitro-modules`, `cpp` at `1e9e17be6f41c838db472d6a4b943fc26355a3be` | Preserve the upstream library contract and the ADR-0020 bootstrap exception; retain one owner and avoid callbacks during mutable container traversal. |
| Callstack `react-native-best-practices` at `59e9d17446a248b21d86f078fcd0bd2d36db1ca4` | Validate observable native behavior and document the distinction from frame or application performance. |

The upstream skill sources were read at the locked revisions. This receipt does
not claim that they were installed as personal skills. No premium Nitro Forge
capability was used. No fresh CodeRabbit service review is claimed.

## Limits and rollback

This executable tests the Qt-independent native registry and detector binding
logic. It does not execute Qt event delivery, Hermes, Fabric, an imported
Reanimated application, or physical QNX. The Qt cancellation route and emitter
removal ordering were reviewed in source; integrated host tests and target
acceptance remain separate requirements. No dedicated React JS thread,
busy-JS frame guarantee, new recognizer support, or layout/scroll qualification
is claimed. All feature defaults and package pins remain unchanged.

Rollback is the cancellation commit itself: revert its registry, binding,
Qt-input/emitter-order, test and receipt changes together. The PR remains draft
until its required runtime and target gates have evidence.
