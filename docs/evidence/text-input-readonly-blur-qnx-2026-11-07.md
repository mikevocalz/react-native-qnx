# TextInput read-only blur: deferred delivery and QNX evidence

The corrected TextInput path snapshots native Focus/Blur observations during
JS-owned updates and delivers them after the outer update's Qt property setters
return. The full combined native test passed twice on physical QNX, and the real
React probe passed positive/interception/recovery trials at integration commit
`e60bae468656658148c044d0c860a85607ff389c`. Exact PR72 source has separate
validation below; integration artifacts do not qualify that isolated revision.

## Source and failure history

The initial six-file fix was canonical
`0a256e7e3407b22499b539c211498d74a6759af2`, cherry-picked from `ca99` whose
parent was PR72 head `8efa`. It addressed PR71 review thread
`PRRT_kwDOT4Yfh8603yxY` and PR72 thread `PRRT_kwDOT4Yfh860348y`: setting
read-only released native focus inside a JS-write guard, which swallowed blur.

The native baseline failed at `JS readOnly transition swallowed native blur`.
The actual React baseline exited 1 at
`RNQNX_TEXT_INPUT_FAIL React readOnly transition swallowed native blur`
(`/tmp/rnqnx-readonly-react-baseline.log`). Its temporary test-file changes were
restored. The initial guard/lifetime fix passed 3/3 focused host tests and actual
React QNX positive/interception/recovery trials (0/1/0), but the expanded combined
native QNX test repeatedly exited 134. Added diagnostic markers and isolated
sections passed; neither counted as a pass of the original combined test.

The private function-name trace `text-input-readonly-gdb-barrier-functions.log`
localized deletion of the field during Qt focus notification, with
`QQuickDeliveryAgentPrivate::notifyFocusChangesRecur`, `QPointer` destruction,
and `QtTextInputBridge::updateTabEligibility` in the abort stack. This evidence
motivated moving callbacks outside the property-notification stack; it assigns
no vendor fault.

The follow-up is original `8799b2cf10a4e320540081051779107e28b12169`
(parent `ca99`), cherry-picked as canonical `e60bae4`. Its four C++ files change
`QtTextInputBridge`, `QtQuickMountingLayer` and the native bridge test. Focus/Blur
snapshots enter a FIFO during JS updates or an ongoing drain. Explicit completion
drains after the outer update and preserves accepted observations ahead of
nested updates. Cancellation, invalidation and callback exceptions clear pending
observations. RAII destructors perform cleanup without invoking user callbacks.
After callback delivery the mounting layer preserves failure enum/detail and
checks QPointer and tag identity. Teardown closes every TextInput emitter before
invalidating or deleting any field.

`RNQNX_ENABLE_TEXT_INPUT_CONTRACT` remains default OFF. Text/selection/key/submit
feedback retains JS-write suppression, and focus transitions do not increment
native edit counts. The React fixture checks one blur, unchanged text/count,
restored editing and refocus. Native assertions retain multiple windows,
callback-driven deletion, FIFO snapshots, nested updates, throwing observers,
cancellation and teardown. Secure multiline remains unsupported.

## Validation by candidate

| Gate                                                         | Result and boundary                                                                                                                                                                                                                                                                                       |
| ------------------------------------------------------------ | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Follow-up standalone host ASan/UBSan                         | Passed 1/1; two independent native/fixture reviews passed. Host instrumentation does not establish target or whole-dependency instrumentation.                                                                                                                                                            |
| Integration host CTest                                       | Passed 31/31, `/tmp/rnqnx-readonly-deferred-ctest.log`. This graph includes later accessibility action integration.                                                                                                                                                                                       |
| Canonical JavaScript/typecheck/architecture/docs             | Passed after `0a256e7`; the `8799` follow-up changes C++ only. An earlier broader worker run failed the unrelated runtime-baseline child-PID readiness test; its unchanged targeted retry passed. The later canonical full JavaScript pass supersedes that open gate without erasing the earlier failure. |
| Integrated PR73 source `e60bae4`, physical QNX               | Original unmarked full combined native test exited 0 twice. Actual React positive/interception/recovery exited 0/1/0. Interception failed at `JavaScript TextInput change count mismatch`, followed by successful recovery.                                                                               |
| Isolated PR72 source `8799b2c`, local source checks          | Frozen dependency installation, all 26 JavaScript modules, TypeScript, docs, dependency and source checks passed in `/tmp/rnqnx-pr72-main-integration`. This source excludes later PR73 action changes.                                                                                                   |
| Isolated PR72 source `8799b2c`, cross-build and physical QNX | Passed: full native TextInput test twice (0/0), actual React positive/interception/recovery (0/1/0), and primary semantic mounting positive/recovery (0/0). Each result uses artifacts built from the isolated source.                                                                                    |
| Hosted CI                                                    | Separate candidate gate; both PR72 and PR73 remain drafts pending follow-ups/latest CI.                                                                                                                                                                                                                   |

The strict interception control rejects success markers, wrong failure
boundaries, crashes, timeouts and sanitizer diagnostics.

## Integrated physical artifacts

Private local artifact/log names in this receipt resolve under
`/Users/mikevocalz/qnx-pi400-8.0.5`. The physical lane used the existing Pi 400
QNX 8.0.5 image and licensed private Qt evaluation SDK dependency closure.
`text-input-deferred-build.log` records the affected QNX build; raw trials are
in `text-input-deferred-physical.log`.

| `e60bae4` artifact               |   Bytes | SHA-256                                                            |
| -------------------------------- | ------: | ------------------------------------------------------------------ |
| `text-input-deferred-probe.qnx`  | 6155704 | `8c8c268510ea9980de10c3487a3b0dfd7ec2f938dfbd100da931642a7b98fadb` |
| `text-input-deferred-probe.hbc`  |  716049 | `c1a21163d54ca4fd52099228c8732c3ff79612d1e8a41db453de978aea21eb60` |
| `text-input-deferred-native.qnx` |  223064 | `f7e55f31df08fd9375bc4095f3379331ab5460f1b856877db54f2bccd962e5b6` |

All three readbacks in `text-input-deferred-readback` match their local artifact
hashes. The raw log contains two `RNQNX_TEXT_INPUT_BRIDGE_OK` markers and React
success/failure/success markers at the stated boundary. Each trial used the
unique `textinput-deferred-20261107-verified` target path and a 35-second
supervisor. Original application PID 9433114 remained alive before, after and
at final checks. No test processes remained. Boot, privileges, window dimensions
and orientation were unchanged.

## Isolated PR72 provenance

The isolated checkout freezes all 486 tracked source hashes in
`text-input-pr72-source-manifest.sha256` (SHA-256
`fab24e7808c3a3bc2bdc88e478b1aacce64adf19537dd17e4b8a0478b7badf09`).
Its exact production Metro bundle `text-input-pr72.bundle.js` is 470557 bytes,
SHA-256 `36cc09509cc3d6fe725be016244eddde9215829bce607a5fd0a2ccb7039202e1`.
The corresponding BaseViewConfig hash is
`0702b7b9241ed3f8fd87082b00c5da34484dd3311d93e3a9b0087129c1f9ec74`.

| `8799b2c` artifact            |   Bytes | SHA-256                                                            |
| ----------------------------- | ------: | ------------------------------------------------------------------ |
| `text-input-pr72-probe.qnx`   | 6135152 | `e62e9a023709f07c9c59ece0914f8aec3b60374bfaef3a0047084f5ce71738c5` |
| `text-input-pr72-probe.hbc`   |  715865 | `664a7479db9d7b21a07abfc6af1811c37fa0fca54925cf6111796a1addcaf8df` |
| `text-input-pr72-native.qnx`  |  214976 | `98635617a9fe98f0a1befa1bad7397d4f69e8ddd53d9a8db2bfbe769f0ca94dd` |
| `accessibility-pr72-view.qnx` |  180888 | `1557c55bf4c26fdc7acb3ed0f8fc5960b683acba5399ac12578b2ca817044c48` |

All four SCP readbacks were byte-identical. TextInput readbacks are in
`text-input-pr72-readback`; the semantic readback is
`accessibility-pr72-view-readback.qnx`.
Build logs are `text-input-pr72-build.log` and `accessibility-pr72-build.log`;
raw trials are `text-input-pr72-physical.log` and
`accessibility-pr72-physical.log`. The staged tracked source matched all 486
blobs from `8799b2c`; the later PR73 action files and targets were excluded.
The primary semantic executable was built with
`RNQNX_ACCESSIBILITY_MOUNTING_TEST=1` and emitted
`RNQNX_ACCESSIBILITY_VIEW_OK tree identity state update clear reject delete`
on both trials. This is synthetic Qt semantic-query/mounting evidence.

The unique target directories were `textinput-pr72-20261107-verified` and
`accessibility-pr72-20261107-verified`. All seven physical gates passed at their
expected status and marker. Original PID 9433114 remained alive before/after
every trial and at final checks, with no test-named processes left. The
integrated artifact table remains attached to `e60bae4`.

## Reproduce and scope

Use the existing licensed host graph with the TextInput contract ON:

```sh
cmake -S . -B build/macos-full-host-tests -DRNQNX_ENABLE_TEXT_INPUT_CONTRACT=ON
cmake --build build/macos-full-host-tests --parallel 4 --target rnqnx_text_input_probe rnqnx_text_input_bridge_test
ctest --test-dir build/macos-full-host-tests -R '^rnqnx_text_input_' --output-on-failure
```

These are reproduction commands; this documentation edit did not rerun runtime
builds or target trials. It inspected source, raw log markers and artifact hashes.
The repository QNX validation skill informed candidate separation and validation
boundaries; `no-ai-slop` informed the prose. Runtime validation belongs to
`/root/qt_app_validation`, integration to `/root`.

Evidence covers synthetic Qt/native/actual React behavior. Physical HID,
software keyboard/IME provider, assistive technology, performance, QNX VM,
Marathon/Wayland and product multiwindow qualification remain untested here.
Native multiwindow assertions establish only their bounded fixture behavior.
No licensed SDK content or binaries are published by this receipt. Evaluation
runs grant no production distribution rights; the selected open-source Qt path
remains governed by [ADR-0013](../adr/0013-open-source-qt.md).

Rollback reverts both TextInput fixes with their matching fixtures, or disables
`RNQNX_ENABLE_TEXT_INPUT_CONTRACT` and redeploys the recorded known-good
application. Retain earlier receipts with their original candidate identities.
