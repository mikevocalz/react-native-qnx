# PR 159: stacked Modal focus and restoration

Base: `d862625e79d6b1dd8a589eaf767e3311757c449a`. Lockfile SHA-256:
`edf0327a216c8963c0ffea2300c37f0b7e29d05510e7f3fb5a3a2a4ad9feea49`.
This follow-up changes the existing Qt Modal focus bridge and its native host
regression test. It adds no public API, dependency or alternate rendering path.

## Defect and change

Every open bridge previously accepted focus inside any Modal. When the covered
app tried to take focus, the lower bridge's earlier signal connection reclaimed
it first; the upper bridge then accepted that lower Modal as a valid target.
A field in the lower Modal could also take focus directly. Escape consequently
reached a covered Modal. If that covered Modal closed before the upper one,
the upper bridge lost the original application focus target.

The existing bridge now keeps non-owning registrations on the Qt GUI owner
thread and selects the most recently opened Modal in the same window. Only that
bridge reclaims focus and handles close requests, and it accepts focus only
inside its own item subtree. Closing a covered Modal rewires dependent
restoration targets before removing the registration. Closing the top Modal
restores the previous field in the next Modal or the original application item.
The bridge destructor removes its registration; guarded Qt pointers do not own
the window, item or other bridges.

## Source, ownership and skills

Read the existing bridge, its Fabric adapter and Modal QML, the native test
fixture, and the repository's architecture and execution contract.
DRI: `hardening_prs`, acting as Qt Quick renderer engineer. Independent source
review by `expo_pr159` found no blocker in the window-scoped stack, focus
restoration or teardown choices. The integrator will rerun the combined slice.

| Skill source                                                                               | Read and applied decision                                                                                                                                                                   |
| ------------------------------------------------------------------------------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Repository `qnx-native-api` and `qnx-platform-validation` at base                          | Keep the existing callback and Fabric path; distinguish real host Qt behavior from QNX target acceptance.                                                                                   |
| Margelo `api-design`, commit `1e9e17be6f41c838db472d6a4b943fc26355a3be`                    | Correct the existing Modal contract without a new JavaScript or native interface.                                                                                                           |
| Margelo `build-nitro-modules`, then `cpp`, same commit                                     | Read in the required order; keep all focus operations on the GUI owner thread and use non-owning guarded registrations with destructor cleanup. No Nitro spec or generated binding changes. |
| Callstack `react-native-best-practices`, commit `59e9d17446a248b21d86f078fcd0bd2d36db1ca4` | Reproduce the focus and restoration failures before changing the implementation; make no timing or performance claim.                                                                       |

These are source reads, not skill installations. No external CodeRabbit review
is claimed by this receipt.

## Validation

Linux Debian 13 x86-64 host, GCC 14.2, CMake 3.31.10, Ninja 1.13.2 and Qt 6.11.1.
The standalone `tests/native/qt-native-ui` target uses the actual Modal QML,
`QtModalBridge`, offscreen `QQuickWindow`, Qt focus operations and Qt key events.
Its close callback stands in for the Fabric event emitter; it does not run
React Native, Fabric or Hermes.

The new tests were first compiled against the original bridge at the base
commit. Each failed independently with exit status 1:

| Negative control                                      | Observed failure                                                                                                                                                 |
| ----------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `onlyTheTopModalReclaimsFocusFromTheCoveredApp`       | The upper Modal did not retain active focus after the app requested it.                                                                                          |
| `aFieldInACoveredModalCannotTakeFocusFromTheTopModal` | The upper Modal did not retain active focus after a lower field requested it.                                                                                    |
| `aModalClosedBelowAnotherLeavesTheUpperModalFocused`  | Application focus was not restored after deleting the lower Modal and then the upper one. The lower Modal had a focused child field before the upper one opened. |

With the correction restored:

- Release compilation of `rnqnx_modal_quick_test` passed with C++20 and
  `-Wall -Wextra -Wpedantic -Werror`.
- The release test run passed all 11 QtTest entries: 9 behavior cases plus
  initialization and cleanup; zero failures or skips.
- A separate Debug build with AddressSanitizer and UndefinedBehaviorSanitizer
  passed the same 11 entries with no diagnostic. The project target used
  `-fsanitize=address,undefined -fno-sanitize-recover=undefined
-fno-omit-frame-pointer`; the installed Qt libraries were not instrumented.
  Leak detection was disabled (`ASAN_OPTIONS=detect_leaks=0:halt_on_error=1`),
  and undefined behavior terminated the process (`UBSAN_OPTIONS=halt_on_error=1`).
- Both runs used `QT_QUICK_BACKEND=software` and `-platform offscreen`.
- Changed-document formatting and `git diff --check` passed.

Reproduce with Qt 6.11.1 available through `CMAKE_PREFIX_PATH`:

```sh
cmake -S tests/native/qt-native-ui -B build/modal-focus -G Ninja \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build/modal-focus --target rnqnx_modal_quick_test --parallel 2
QT_QUICK_BACKEND=software build/modal-focus/rnqnx_modal_quick_test -platform offscreen
```

No QNX cross-build, QNX VM, Pi 400, stock QSTI, physical input, Marathon shell
or performance run was performed for this follow-up. Hosted checks must run
on the integrated PR head; this receipt does not complete broader v1 target
acceptance. No SDK payload, premium dependency, permissions or device state
changed. Rollback reverts this bridge, its regression cases and this receipt.
