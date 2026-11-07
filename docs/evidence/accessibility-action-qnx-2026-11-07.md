# Registered View activation: host and physical QNX receipt

A registered generic View `activate` action now travels from Qt's public
accessibility press signal through the owned native receiver and RN
`ViewEventEmitter` to the actual React callback. Host probes pass after fixing
the JavaScript prop/event boundary. The full host suite and six physical QNX
gates passed. Earlier semantic-query receipts retain their original scope.

## Candidate and behavior

Canonical branch is `codex/accessibility-activate-integration`. Native/action
source `45afdc4e2f95ee88332f2937b314266c6de79d9e` is the cherry-pick of
`b40626`; RN boundary correction
`c182889b962ec44e6295c352750f6edb64d759cd` follows `ffdda60`. The implementation
changes 16 paths; the boundary correction changes three. Prior PR72 merged as
`8efa` with the same code/documentation tree as its reviewed `5aba` candidate.
No dependency, renderer or public Nitro API was added.

The default-OFF `RNQNX_ENABLE_ACCESSIBILITY_BRIDGE` flag guards the native path.
The item-parented `QtAccessibilityActivation` connects
`Accessible.onPressAction` through public `QQmlProperty`/`QMetaMethod` APIs.
It dispatches only with a live owner, accessible View, one registered `activate`
action, a handler, semantic enabled policy and native item enablement. The
adapter replaces and invalidates the receiver on props/emitter updates and
teardown. Posted calls to deleted receivers cannot enter replacement generations;
a callback may synchronously delete its receiver without post-callback access.
Unsupported or duplicate registrations reject explicitly.

The RN `BaseViewConfig` correction admits seven standard accessibility
props/handler attributes and the direct `topAccessibilityAction` event. Its
installed-source execution regression verifies the actual exported tables;
the Metro dependency list now includes that config so the fixture rebuilds.
The React fixture retains its per-instance Zustand store in a ref; native Qt
objects remain outside serialized application state.

This proves a bounded registered activation route. Qt's role-default advertised
action list still does not match the RN registry. Optional RN
`accessibilityActions[].label` values are not mapped to Qt action names/labels;
custom action-label advertisement remains unqualified. Semantic disabled-state
reporting, value semantics, unmapped actions and assistive reader output remain
unqualified. No hardware accessibility input or OS consumer is exercised.

## Actual checks

| Gate                                           | Result and scope                                                                                                                                                                                                                    |
| ---------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Native missing-connection baseline             | Failed without the press connection; establishes that semantic role metadata alone does not deliver activation.                                                                                                                     |
| Standalone native ASan/UBSan                   | Passed 2/2: semantic and activation fixtures, including eligibility, invalidation, deletion and callback self-deletion.                                                                                                             |
| Initial full React focused run                 | Failed 1 of 4 at mount: the QNX BaseViewConfig omitted accessibility props/event transport. This result predates the boundary correction.                                                                                           |
| Corrected full React focused run               | Passed 4/4: real React action probe, native activation fixture, strict interception and unsupported-registration controls.                                                                                                          |
| JavaScript/source gates                        | Full JavaScript suite, typecheck and reported source/config checks passed. The boundary execution regression also passed.                                                                                                           |
| Full host suite, options ON                    | Passed 31/31; `/tmp/rnqnx-action-full-ctest.log`.                                                                                                                                                                                   |
| Fresh all-options-OFF suite                    | Passed 16/16 at `45afdc4` + `c182889` with current main ancestry; `/tmp/rnqnx-action-off-ctest.log`.                                                                                                                                |
| Physical QNX native positive/baseline          | Passed: native 0 with `RNQNX_ACCESSIBILITY_ACTION_NATIVE_OK press guards generation invalidate delete`; missing-connection baseline 1 at `RNQNX_ACCESSIBILITY_ACTION_NATIVE_FAIL Qt press did not reach owned activation receiver`. |
| Physical actual React positive/recovery        | Both passed, exit 0 with `RNQNX_ACCESSIBILITY_ACTION_OK react activate guards label teardown remount`.                                                                                                                              |
| Physical interception/unsupported registration | Passed: interception 1 at `RNQNX_ACCESSIBILITY_ACTION_FAIL JavaScript accessibility action count mismatch`; unsupported registration 0 with `RNQNX_ACCESSIBILITY_ACTION_REJECT_OK unsupported-registration`.                        |
| Physical readback/cleanup                      | All three artifact pairs matched. Original application PID `9433114` remained alive before each trial and afterward; no fixture/supervisor child remained.                                                                          |

The real React probe queries the semantic label/role, invokes Qt's action
interface, observes the actual `activate` payload and a React-driven label
update, then tests guards, teardown and remount. Interception requires exit 1
at `JavaScript accessibility action count mismatch`. Unsupported registration
requires exit 0 with `RNQNX_ACCESSIBILITY_ACTION_REJECT_OK unsupported-registration`
and no stale action delivery; that explicit rejection marker is distinct from
the positive success marker. Strict wrappers reject incorrect failure boundaries,
crashes, timeouts and sanitizer diagnostics.

Host sanitizer results do not establish whole-dependency or target
instrumentation; existing leak exclusions remain. No reader output, physical
HID, IME, timing, memory-growth budget or presentation result is claimed.
Marathon/Wayland and QNX VM were not exercised for this slice.

## Final PR73 integration validation

The final cross-build and physical run used exact source
`79603fa65880107b6683d79052e02e5056fceb61`, including the deferred TextInput
fix and the three prior evidence documents. All 499 tracked source blobs
matched that revision. The private local `final73-source-manifest.sha256` has
SHA-256 `a64f69a2eecc4d519ea0753e6e2679cdd608e17ab88d34e709b3cdd6dbf5b2f1`.
The `c182889` results and artifacts above remain historical evidence for that
candidate. The following artifacts were built and run for this final graph.

Independent fresh production Metro bundles matched the cross-build inputs:
action bundle 464815 bytes, SHA-256
`d2323b9828f79a0206453e1098de46581dc16078e01122b12130f039469c346d`;
TextInput bundle 470772 bytes, SHA-256
`be51b1f80666000ad9715ba86361b82c121be0c90fba6df9ca869f26a6233def`.
Local provenance is `/tmp/rnqnx-pr73-bundle-provenance.json`.

| Physical gate at `79603fa`                       | Status and result                                                                      |
| ------------------------------------------------ | -------------------------------------------------------------------------------------- |
| Action native positive                           | 0; `RNQNX_ACCESSIBILITY_ACTION_NATIVE_OK`                                              |
| Action missing-connection control                | 1; expected `Qt press did not reach owned activation receiver`                         |
| Actual React action positive                     | 0; `RNQNX_ACCESSIBILITY_ACTION_OK`                                                     |
| Action interception control                      | 1; expected `JavaScript accessibility action count mismatch`                           |
| Unsupported action registration                  | 0; `RNQNX_ACCESSIBILITY_ACTION_REJECT_OK unsupported-registration`                     |
| Actual React action recovery                     | 0; `RNQNX_ACCESSIBILITY_ACTION_OK`                                                     |
| Generic View semantic positive/recovery          | 0/0; both `RNQNX_ACCESSIBILITY_VIEW_OK tree identity state update clear reject delete` |
| Full combined native TextInput, two trials       | 0/0; both `RNQNX_TEXT_INPUT_BRIDGE_OK`                                                 |
| Actual React TextInput positive/control/recovery | 0/1/0; success markers around expected `JavaScript TextInput change count mismatch`    |

All 13 gates passed at their expected statuses and marker boundaries. TextInput
ran the original combined fixture with deletion, FIFO, nested update, throwing
observer and cancellation assertions retained. The
[read-only blur receipt](text-input-readonly-blur-qnx-2026-11-07.md) records the
failure history, deferred callback fix and separate exact PR72 results.

| Final artifact                            |   Bytes | SHA-256                                                            |
| ----------------------------------------- | ------: | ------------------------------------------------------------------ |
| `accessibility-action-final73-probe.qnx`  | 6136344 | `59d751335326425a18a7e7e64d71592e8d577652c9f0ed20c855d400edfce8c5` |
| `accessibility-action-final73-probe.hbc`  |  708408 | `7c84881d05a803a8d6b5d23059224ea32eb5a0764e69d01ad6fb27b75aeb83d4` |
| `accessibility-action-final73-native.qnx` |   79680 | `5e240eb87c23d95b738ca2fc84a0e16a6ad556720f010da91e7568f570adefe1` |
| `accessibility-view-final73.qnx`          |  188936 | `89801699f7eaf1229a7831309c039d2ecb363e148fc4f5e7c69e5035697e85bd` |
| `text-input-final73-probe.qnx`            | 6155704 | `8c8c268510ea9980de10c3487a3b0dfd7ec2f938dfbd100da931642a7b98fadb` |
| `text-input-final73-probe.hbc`            |  716048 | `931b3a4dd22d7385bfb7d742f9032bd283d93c1f86368afa39158239b2d54e84` |
| `text-input-final73-native.qnx`           |  223064 | `f7e55f31df08fd9375bc4095f3379331ab5460f1b856877db54f2bccd962e5b6` |

All seven readbacks in `final73-readback` matched local byte counts and hashes.
Raw build, target and machine-readable receipts are respectively
`/Users/mikevocalz/qnx-pi400-8.0.5/final73-build.log`,
`/Users/mikevocalz/qnx-pi400-8.0.5/final73-physical.log` and
`/Users/mikevocalz/qnx-pi400-8.0.5/final73-results.json`.
The unique target directories were `final73-action-20261107-verified`,
`final73-semantic-20261107-verified` and `final73-textinput-20261107-verified`.
Original application PID 9433114 remained alive before and after every gate and
at final checks. Five named process checks found no test leftovers. Boot,
privileges and window configuration were unchanged.

The latest full host graph passed 31/31 at compiled code `e60bae4`
(`/tmp/rnqnx-readonly-deferred-ctest.log`). Canonical JavaScript, typecheck,
architecture and docs checks passed after `0a256e7`; the `8799` follow-up changes
C++ only, and `79603fa` adds documentation only. Hosted CI for the latest
published head remains separate. PR72's updated `b661` source is pushed and ready
for review with CI pending; its old finding is resolved. The PR71 thread awaits
the main-branch merge. PR73 remains draft pending publication of this receipt
and latest CI.

This final run proves actual React registered activation, synthetic Qt semantic
queries/mounting and synthetic TextInput behavior. Qt action-list/custom-label
parity, RN disabled semantic state, value semantics, full assistive technology,
physical HID, IME and performance qualification remain open. No licensed source
or binary was uploaded or published. The receipt update itself reran no runtime
tests; it verified the recorded raw markers and artifact/readback hashes.

## Reproduce and hand off

Use the existing licensed host graph and dependency paths:

```sh
cmake --build build/macos-full-host-tests --parallel 4 --target rnqnx_accessibility_action_probe rnqnx_accessibility_action_native_test
ctest --test-dir build/macos-full-host-tests -R '^rnqnx_accessibility_action_' --output-on-failure
node tests/js/accessibility-view-config.test.mjs
node tests/js/accessibility-action-controls.test.mjs
node tests/js/accessibility-action-fixture-config.test.mjs
```

These are reproduction commands; this receipt edit did not rerun builds.
QNX CMake accepts a host production Metro bundle through
`RNQNX_ACCESSIBILITY_ACTION_TEST_BUNDLE` and pinned Hermes compiler
`260318099.0.5` through `RNQNX_ACCESSIBILITY_ACTION_TEST_HERMESC`, generating HBC.
The runtime agent cross-built source `c182889` through the licensed QNX graph
with QCC/GCC12.2, Qt6.12 and the corrected production Metro bundle/HBC. Each of
the six physical trials ran under a 35-second supervisor in the unique
`accessibility-action-20261107-verified` directory. Existing artifacts and the
continuous application were preserved; boot, privileges and established windows
were unchanged.

Local raw logs are
`/Users/mikevocalz/qnx-pi400-8.0.5/accessibility-action-physical.log` and
`/Users/mikevocalz/qnx-pi400-8.0.5/accessibility-action-qnx-build.log`.
Direct file hashing and byte counts verified the following files and their
corresponding `-readback` files; every pair matched:

| Artifact                          |   Bytes | SHA256                                                             |
| --------------------------------- | ------: | ------------------------------------------------------------------ |
| `accessibility-action-probe.qnx`  | 6125048 | `97fc0f6eeb5f2a62755a5bb81182bd4b78d1d6bc2b6a934dcac9e25db1e83466` |
| `accessibility-action-native.qnx` |   68112 | `534a42628140631e0954e433de32756cbe2b1a263c5877d54a5230867fef4ff8` |
| `accessibility-action-probe.hbc`  |  708400 | `2e4d188b1d3136e3cdaaf38f3c5a46d4909334bbc7fd5ef95f3d3506c40af0c2` |

DRI is `/root/runtime_harness`; integration owner is `/root`, with independent
child source review of the native route and RN boundary correction and CI-agent
review. Repository native/API/validation skills informed lifetime, standard RN
transport and evidence boundaries; `no-ai-slop` informed this prose. Hosted CI
is pending and separate from these completed local native gates.

No credentials, private device addresses, user text or licensed vendor contents
belong in shared evidence. Window dimensions, orientation, boot configuration,
privileges and evaluation-license terms are unchanged. Evaluation runs do not
grant production distribution rights.

Rollback reverts the activation/boundary series with its matching frozen source
and bundle, or disables the native accessibility bridge and redeploys the
recorded known-good application. Keep the earlier View/TextInput/focus evidence
and artifacts intact.
