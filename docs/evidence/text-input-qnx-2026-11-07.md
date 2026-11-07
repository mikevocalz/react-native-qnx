# TextInput edit contract: host and physical QNX receipt

The native Qt bridge passed on the physical QNX Pi400. The real React TextInput
fixture passed host checks and the corrected target driver passed physical
positive, interception and recovery checks with matching artifact readback. This is a bounded Plan 04 subset; it does not establish complete
TextInput or IME support.

## Candidate and source boundary

The candidate is `15100b8dc83d434ec48323e027544d793f3f1593` on
`codex/text-input-integration`, following implementation `6558a66`, visual-tree
editor discovery `d21a26b`, and the earlier contract `987e7ed`. The
[dated contract](../validation/text-input-contract-2026-11-07.md) records the
source audit and proposed acceptance cases; its original proposal status is
historical, not the implementation result.

The slice retains Fabric, Hermes, Yoga and the Qt Quick renderer. It adds an
owned native edit token, stale text/selection rejection, JS-write feedback
suppression, UTF-16 selection, standard `setTextAndSelection`, and teardown
connection cleanup. Shared QML supplies the editors. Per-instance React state
uses retained vanilla Zustand stores with selectors; native handles remain in
refs. Zustand is pinned to `5.1.0`. No broader React state migration or new
public Nitro API is claimed.

The experimental `RNQNX_ENABLE_TEXT_INPUT_CONTRACT` option defaults OFF.
Secure multiline is unsupported and fails closed. Secure input-method queries
and commits are rejected, including sensitive surrounding/selected text queries.
These source-level boundaries do not qualify clipboard, accessibility or an
external input provider's privacy.

## Actual checks

| Gate                                           | Result and scope                                                                                                                                                                 |
| ---------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Full host suite, contract ON                   | Passed 26/26 before the test-driver-only change in `15100b8`. This count is not a fresh full-suite result at that revision.                                                      |
| Corrected driver, focused host rerun           | Passed 3/3 at `15100b8`: React probe, native bridge and strict interception control. Production sources were unchanged by this driver revision.                                  |
| Fresh all-OFF host suite                       | Passed 12/12; `LastTest.log` dated November 7, 10:08.                                                                                                                            |
| Physical QNX native bridge                     | Passed, child exit 0: `RNQNX_TEXT_INPUT_BRIDGE_OK count stale utf16 no-echo secure-query denial lifecycle`. This standalone Qt bridge result does not include the React fixture. |
| Initial physical React fixture                 | Failed, child exit 1: `RNQNX_TEXT_INPUT_FAIL TextInput driver failed`. Its driver attempted inline source evaluation unsupported by the lean target Hermes configuration.        |
| Corrected React cross-build                    | Passed using the compiled fixture driver and HBC; see `text-input-qnx-driver-build.log`.                                                                                         |
| Corrected React physical positive and recovery | Both passed, exit 0, with `RNQNX_TEXT_INPUT_OK react key preedit commit utf16 stale selection no-echo focus teardown remount`.                                                   |
| Corrected React physical interception          | Passed the negative control: exit 1 at `RNQNX_TEXT_INPUT_FAIL JavaScript TextInput change count mismatch`.                                                                       |
| Target artifact readback and cleanup           | Executable and HBC size/hash matched readback. The original application PID `9433114` remained alive before and after each run; no probe or supervisor child remained.           |

The corrected driver invokes functions already compiled into the fixture HBC
through `runtime.global().getPropertyAsFunction(...).call(...)` inside the
host task queue's `runSync`. It preserves the owned JavaScript entry/checkpoint;
it does not enable runtime source compilation. The visual-tree discovery fix
finds the mounted Qt editor rather than assuming QObject child ownership.

The React fixture sends synthetic Qt key and input-method events to live
editors and observes actual React callbacks. It covers ordinary preedit/commit,
UTF-16 selection, stale updates, no feedback from JS writes, independent fields,
focus, teardown and remount. The interception control consumes native change
before React and requires exit 1 at `JavaScript TextInput change count mismatch`.
Its wrapper rejects success markers, timeout/crash outcomes and sanitizer
messages. The physical runs used the same executable/HBC under a 35-second supervisor
per trial in the isolated `textinput-20261107-verified` target directory.

Host probe targets request ASan/UBSan instrumentation when the supported host
sanitizer option is enabled; leak checking is disabled in their test environment.
This receipt makes no whole-dependency instrumentation or target-sanitizer claim.

## Artifact and log identities

| Artifact                                                        | SHA256                                                             |
| --------------------------------------------------------------- | ------------------------------------------------------------------ |
| Candidate lockfile                                              | `b854ebe2e50a42b2e8e07124df065592d99d43b1d1a3a984b66d3835ad86c3e4` |
| React fixture source                                            | `586d37d6488448d5b62f0fa503708d0d102b050e6ab1cbe9f906b3c0c28eee01` |
| Corrected C++ React probe                                       | `42b0fd7af27bea631a68ad36c1e67e95583eb13ef54c2e3b0cb8723dcceceb89` |
| Physical native bridge executable, 64,816 bytes                 | `c1ba9d38d268db215c080eecb8f235678ec12019be0def3c33c560df3515732f` |
| Corrected React target executable and readback, 6,108,712 bytes | `bf854e3544a694fadd9edf06b0c3c1b1650ff45b55c1059b854425c1c8691968` |
| Corrected React HBC and readback, 715,286 bytes                 | `24160992afdc5421590839416cf3018dcd60ac37d81940ed8edd9fa4e4c03458` |

Local raw evidence resides under `/Users/mikevocalz/qnx-pi400-8.0.5/`:
`text-input-bridge-physical.log` records the bridge pass;
`text-input-react-physical.log` records the failed initial driver.
`text-input-react-verified-physical.log` records the corrected positive,
interception and recovery trials; `text-input-qnx-driver-build.log` records the
corrected cross-build. The target driver source came from runtime commit `8089086`, integrated into
the canonical branch as `15100b8`; the probe source hash above identifies it. Nonfatal Qt warnings
about an undeclared event parameter and a dropped null-instance teardown
`topSelectionChange` remain in the raw log. Local paths are not public bundled artifacts. Shared
receipts exclude private device addresses, credentials, user text, and licensed
Qt/QNX source or runtime contents. Fixtures use synthetic text only.

## Reproduce and hand off

Use the existing licensed host graph and preserve its dependency paths:

```sh
cmake -S . -B build/macos-full-host-tests -DRNQNX_ENABLE_TEXT_INPUT_CONTRACT=ON
cmake --build build/macos-full-host-tests --parallel 4 --target rnqnx_text_input_probe rnqnx_text_input_bridge_test
ctest --test-dir build/macos-full-host-tests -R '^rnqnx_text_input_' --output-on-failure
node tests/js/text-input-state.test.mjs
node tests/js/text-input-qml-source.test.mjs
node tests/js/text-input-fixture-config.test.mjs
```

These are reproduction commands, not an assertion that this documentation edit
reran them. QNX configuration accepts `RNQNX_TEXT_INPUT_TEST_BUNDLE` for an
existing host-produced production Metro bundle and `RNQNX_INPUT_TEST_HERMESC`
for pinned Hermes compiler `260318099.0.5`; it generates
`text-input-probe.hbc`. The target must load HBC and run under a bounded external
owned-process supervisor using the existing licensed Qt/QPA environment. The positive, strict interception, recovery, readback and cleanup outcomes
above close this bounded real React physical gate.

The repository `qnx-native-api` and `qnx-platform-validation` skills informed
ownership, opt-in behavior and separation of native bridge, host React and
physical React evidence. `no-ai-slop` informed the receipt's prose. Reading
these skills is not runtime proof. Runtime validation belongs to the runtime
agent; integration and receipt acceptance belong to `/root`. Independent
review acceptance and hosted CI are not established by this receipt.

No hardware keyboard/HID session, installed language engine, software keyboard
geometry, assistive technology output, clipboard privacy, performance budget,
QNX VM or Marathon/Wayland result is claimed. Window dimensions, orientation,
boot configuration, device image, privileges and license entitlements are outside
this slice. Private evaluation runs grant no production distribution rights.

Rollback disables `RNQNX_ENABLE_TEXT_INPUT_CONTRACT` and restores the recorded
known-good application. Source rollback reverts the bounded TextInput series
and restores the matching frozen manifests/lockfile. The owner's continuous
application remained intact during the target trials.
