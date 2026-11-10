# Input payload and synthesized-mouse corrections

This slice extends the existing default-OFF `RNQNX_ENABLE_INPUT_LIFECYCLE`
experiment. It does not implement or qualify RNGH, Worklets or Reanimated.

## Behavior

- Touch and mouse `screenX`/`screenY` use Qt global positions, independently
  of window/page positions. Terminal touch events retain their final global
  position instead of copying page coordinates.
- Touch `force` follows `QEventPoint::pressure()` on press, move and release,
  including zero pressure at release. Mouse force behavior is unchanged.
- Mouse events tagged `MouseEventSynthesizedByQt` are not delivered a second
  time through Fabric after Qt touch delivery. Real mouse, system-generated
  and application-generated mouse streams remain unchanged.
- Disabling the experiment preserves the previous input behavior.

## Source and ownership

The implementation stays in `QtTouchDispatcher.cpp` on the existing Qt owner
thread, with no new queue, runtime, native module or renderer. RN 0.88.1's
`BaseTouch.cpp` serializes `screenPoint` to `screenX`/`screenY` and `force`
without coordinate conversion. Qt 6.11.1 `qevent.h`, `qeventpoint.h`,
`qeventpoint.cpp` and `qeventpoint_p.h` were inspected from the upstream
`qt/qtbase` `v6.11.1` source tag, resolved to commit
`59c81a3c2247b821b9b84b4eb8d939b77e07e276`. Qt's public global-position and pressure
getters are used; no private Qt API is introduced. The public Released point
constructor initializes pressure to zero, enabling the native regression test.

Margelo `api-design`, `build-nitro-modules` and `cpp` guidance was read at
`1e9e17be6f41c838db472d6a4b943fc26355a3be`: preserve the framework's public
payload contract, avoid inventing a Nitro facade for third-party libraries,
and keep input dispatch on its existing owner with no extra lifetime state.
This changes an existing RN-core input adapter, not a new Nitro API.

## Validation boundary

`QtInputLifecycleProbe.cpp` now asserts distinct global/page positions,
terminal pressure, synthesized-mouse suppression, and parity through the
actual JavaScript handler. Its existing CMake registration runs in the ON
host-renderer lane; the intercepted-JS negative control remains in place.
`QtReactHostHostTest.cpp` explicitly checks opt-out screen/force compatibility.
The full host renderer's ON/OFF matrices must compile and execute these tests
on the published revision before recording native success.

Local `git diff --check` passed. Local Qt compilation, sanitizers and physical
QNX input were not run in the implementation workspace, which has no Qt
installation or QNX target. A later CI pass is host evidence only. Real
pressure hardware, OS-generated mouse/touch relationships, physical HID,
IME, RNGH recognizers and Worklets runtime integration remain unqualified.
No performance numbers or device acceptance status are changed.

Rollback: disable `RNQNX_ENABLE_INPUT_LIFECYCLE`, or revert this input slice.
