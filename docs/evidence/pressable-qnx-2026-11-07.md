# React Pressable integration: host and physical QNX receipt

Host and physical QNX validation passed for the recorded candidate. This extends the
raw Fabric-JS input probe with an actual production-Metro React Native Pressable
fixture. It does not implement RNGH, Worklets or physical HID delivery.

## Candidate and source boundary

Implementation commits are `3dda270ca6ca95485556468d8007c3a1f427d105` and
`8ac3a3db12692d93aef9b1c7c779db46acec8565`, based on main
`be71bc99e4664825f6c409e0704e57e66512375e`. The source HEAD is `d364811` on `codex/pressable-integration`.
The source changes were integrated through `620fea8` and
`0004d987978b5c08e7183b69497946fc4b13f7fc`; cached-bundle build input
`ff0ac6c9f826112380d9fb16738030eb97431351` was integrated in `d364811`.
Its tiny configuration tests passed separately; the fresh native and target
results below cover the integrated candidate.

RN remains `0.88.1`, React `19.3.0`, Metro `0.87.1`, Expo `58.0.22` and Nitro
alpha105. The lockfile changes only the new RN patch registration and derived
peer/snapshot identities; semantic comparison preserved every package version,
integrity and dependency edge. No Nitro binding or ABI changed.

The original host run exposed a real remount failure: after stopping a surface
with a press pending, the new tree had four mounted native nodes and native
input dispatch advanced, but React's next `onPressIn` never ran. The old target's
cancel had a null instance handle. Pinned RN's new EventTarget dispatch path
skipped null targets entirely, leaving its responder state alive across remount.

The exact three-file RN patch forwards only null-target `topTouchCancel` when
the internal `__RNQNX_INPUT_LIFECYCLE__` flag is strictly true. The host sets that
flag only under the existing default-OFF CMake input option. RN's actual
`processResponderEvent` already accepts null targets. The dispatcher processes
the cancellation, skips DOM dispatch for null, and retains its `finally` error
forwarding. There is no feature-flag fallback to the older responder system,
replacement renderer, synthetic responder implementation or event-loop pumping
inside `stopSurface`.

## Actual checks

- **Host targeted build and CTest: passed.** The integrated candidate's three
  Pressable tests passed on macOS AppleClang/Qt6.12, MinSizeRel, input ON.
  The real component emitted
  `RNQNX_PRESSABLE_OK tap cancel deactivate disabled unmount remount callbacks=11`.
- **Delivery control: passed.** The same executable and fixture with native
  interception must exit1 at `Pressable onPressIn missing`. The wrapper rejects
  a success marker, wrong failure boundary, crash or sanitizer diagnostic.
- **Removed-target causality control: passed.** The same executable and fixture
  with the test-only fix flag disabled must exit1 at `remount press-in missing`.
  Its wrapper has the same failure restrictions. This reproduces the regression
  rather than changing the fixture or replacing React responders.
- **JavaScript/source gates: passed.** Isolated frozen offline installation,
  dependency validation, all JavaScript tests, typecheck, formatting and diff
  checks passed. The installed dev/prod guards are pinned by exact source hashes.
  A test executes the installed dispatcher's Babel-transformed body with
  controlled dependencies to verify null cancellation and error forwarding.
  Those dependency doubles are not the native/component test.
- **Cached fixture configuration: passed, host graph only.** The actual CMake
  module retained normal host/default-QNX Metro generation, consumed an explicit
  cached QNX bundle through the Hermes/HBC dependency, and rejected a missing
  path or directory. This exercised configuration branches with a host compiler,
  not a QNX cross-build or Hermes execution.
- **Fresh full host build and suite: passed.** The guarded MinSizeRel build on
  macOS AppleClang/Qt6.12 completed with input and trace ON; CTest passed 23/23.
  The default configuration with both options OFF passed 16/16.
- **QNX cross-build: passed.** The licensed SDP8.0.5/GCC12.2/Qt6.12 graph
  completed all five build steps using the host production Metro bundle and
  pinned Hermes to generate HBC. The SDK container had no Node executable.
  The existing private Qt evaluation environment was preserved.
- **Physical Pi400 execution: passed.** As `qnxuser`, under a 35-second
  direct-child supervisor, the positive run exited 0 with
  `RNQNX_PRESSABLE_OK tap cancel deactivate disabled unmount remount callbacks=11`.
  The same binary and HBC exited 1 at `Pressable onPressIn missing` with native
  interception and exited 1 at `remount press-in missing` with
  `RNQNX_PRESSABLE_DISABLE_REMOVED_TARGET_FIX` enabled. A recovery positive run
  exited 0 with the same 11 callbacks. The original continuous application,
  PID `9433114`, remained intact; no trial application remained.
- **Earlier attempts: blocked or incomplete.** Low-disk guards stopped earlier
  full builds; the intervening 23-test run used existing artifacts. Those runs
  remain distinct from the fresh full-build pass above. The initial QNX attempt
  lacked Node. An initial launcher failed on an unset variable before invoking
  the probe; it supplies no runtime evidence. The corrected launcher used
  `unset ... || :` and the actual fix-control environment variable above.
  Marathon/Wayland and QNX VM runs were not performed.

The native `instanceHandle is null` warning still prints. The same-artifact
removed-target control establishes the JavaScript nullable cancellation behavior;
the warning alone does not describe the patched dispatcher's result.

The host executable links the ASan runtime, and the existing cache enables host
sanitizers. The probe translation unit and third-party graph are not claimed
fully instrumented; this is not whole-system ASan/UBSan, TSan or target-sanitizer
proof. Existing leak-check exclusions remain. No timing, frame-pacing, memory
budget or physical presentation result was measured.

## Observed artifact identities

| Artifact                              | SHA256                                                             |
| ------------------------------------- | ------------------------------------------------------------------ |
| Lockfile                              | `76fd60a1145aebe41ee0c88d6c573ffdb4859d319f6abc18a4f180c4dd3a0de2` |
| RN patch                              | `93cb95dac314df9c65228d9bd5affcc6131434f488158ebaadaa0dd8618b79a5` |
| Pressable entry                       | `a76c0ff3464ee468def23ad744f5f3e314f0ee6904dbf96a6da046c6c1882f4e` |
| Probe C++ source                      | `4cbe284a745ca24e66634fc041183ab77e3622b89d474298be6169ff70e225ec` |
| Observed host executable              | `7a646dcefe6cc8122b75971047e092b6e5898cfb1b29031758c093c7d6bc80a5` |
| Observed host production Metro bundle | `c9acb3e9eebab08c9b80eead45a6ee1a33fdac3ef91a6575fc6c4a1bf310bb31` |

The last two host hashes were read from the existing integrated host artifacts
following the targeted pass. The target executable and HBC were downloaded for
readback; size and SHA256 matched the deployed artifacts:

| Target artifact |   Bytes | SHA256                                                             |
| --------------- | ------: | ------------------------------------------------------------------ |
| QNX executable  | 6088712 | `c4b0dc23d0734f858aedad9ff52b5992c98a74d2f483b584d22bef3a38bdd218` |
| Hermes HBC      |  726699 | `d19a4b1f40985d77069b21c860cb5ed2fdd20968afbb75d5ce1520cfcf550132` |

Local raw logs are `/Users/mikevocalz/qnx-pi400-8.0.5/pressable-physical.log`
and `/Users/mikevocalz/qnx-pi400-8.0.5/pressable-qnx-build-complete.log`.
They are local evidence paths, not bundled public artifacts. Shared evidence
excludes private device addresses, credentials and licensed dependency contents.

## Reproduce and review

Use the existing licensed/full-host graph and its unchanged dependency paths:

```sh
cmake -S . -B build/macos-full-host-tests -DRNQNX_ENABLE_INPUT_LIFECYCLE=ON
cmake --build build/macos-full-host-tests --parallel 4 --target rnqnx_pressable_probe
ctest --test-dir build/macos-full-host-tests -R '^rnqnx_pressable_' --output-on-failure
node tests/js/pressable-cancellation.test.mjs
node tests/js/pressable-fixture-config.test.mjs
```

The [fixture instructions](../validation/pressable-probe.md) describe the
host-produced production bundle and QNX cached input. The target must load HBC,
not source JavaScript, using the existing private Qt/QPA environment and an
external watchdog. The target statuses and readback identities are recorded above.

DRI `/root/runner_failure`; integration owner `/root`; independent source
reviewer `/root/renderer_build_audit`. The repository native API/validation
skills informed lifetime, opt-in and evidence boundaries. No new application
state, public native API or code-generation support is claimed. The static
fixture needs no application state store; existing RN internals are unchanged
apart from the narrow dispatcher compatibility patch.

No app window dimensions, orientation, boot configuration, device image,
privileges, premium Forge dependency or license entitlement changed in this
slice. Existing private Qt evaluation and QNX terms remain separate from any
production distribution claim. The owner's continuous Hello World application
remained intact during target tests. CodeRabbit reported zero issues at
`0004d98` across 28 changed files. Its review excluded the three-file cached-bundle
follow-up, which received independent source review and a passing configuration
test. No hosted CI result is inferred from these local tests.

Rollback disables `RNQNX_ENABLE_INPUT_LIFECYCLE` and redeploys the recorded
known-good application. A source rollback reverts this fixture/patch series and
reinstalls the matching frozen lockfile. Physical input, pointer coordinate and
synthesis fidelity, responder arbitration with native scrolling, RNGH and
Worklets are separate next slices.

## Hosted configuration-test compatibility follow-up

Initial hosted JavaScript runs rejected the isolated configuration fixture:
it required CMake 3.31.9, while those runners provide 3.31.6. The fixture now
requires the 3.20 APIs it uses. The production project still requires 3.31.9;
no native gate or functional assertion was removed. The actual fixture and full
JavaScript suite passed with an isolated CMake 3.31.6 selected through
`RNQNX_TEST_CMAKE`. This is test-tool compatibility evidence, not another device
run. The updated head needs its own hosted results.
