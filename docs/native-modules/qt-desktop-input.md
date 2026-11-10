# Qt keyboard, focus and pointer hover

This change delivers desktop input through the existing React Native Fabric
event emitters and Qt Quick mounting tree. It adds no second renderer and no
dependency on a component library. Applications can install shared controls as
ordinary packages; the platform owns their primitive input delivery.

## Configuration and ownership

Enable `RNQNX_ENABLE_DESKTOP_INPUT=ON` together with
`RNQNX_ENABLE_INPUT_LIFECYCLE=ON`. Both default OFF. Configuration fails if
desktop input is enabled without the input lifecycle contract or the full
Qt/Hermes/React host graph. Linux, macOS and QNX are the supported configuration
families; each target still needs its own execution evidence.

`QtTouchDispatcher` owns `QtDesktopInput` on the Qt thread. It observes only its
own window, uses the mounted shadow view's immutable Fabric event emitter, and
removes focus connections, pointer ancestry and armed key activation when a tag
is deleted. Window deactivation clears hover and pending activation. It never
owns a JavaScript runtime or invokes a component callback directly.

The RN platform props extension supplies `focusable`, `onKeyDown`,
`onKeyDownCapture`, `onKeyUp` and `onKeyUpCapture`. Its include root is selected
for the entire RN graph only while the flag is ON, so every object agrees on
the `ViewProps` layout. Installed React Native sources are not patched. Native
trait hooks preserve Views with keyboard/focus behavior through flattening.

## Input contract

| Surface                       | Behavior                                                                                                                                                                               |
| ----------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Generic View focus            | Real `ref.focus()` / `ref.blur()`, native focus events, and `focusable` participation in Qt's focus chain. Disabled controls are skipped; disabling the focused control delivers blur. |
| Key events                    | Fabric `keyDown` / `keyUp` include key, modifiers, timestamp, repeat flag and target. Navigation keys use the shared controls' expected names, including `ContextMenu`.                |
| Ordinary Pressable activation | Enter/Space arm on the first key press and deliver one non-pointer click on release to the same focused target. Pointer taps remain on the existing touch/responder path.              |
| Custom key handlers           | A handler on the focused target owns its activation semantics. An ancestor's Escape listener does not suppress an ordinary child Pressable's Enter/Space activation.                   |
| Tab and Shift-Tab             | Native Qt focus traversal, skipping unavailable items and bounded to the topmost mounted modal. The asynchronous JS event cannot retroactively cancel native focus traversal.          |
| Mouse hover                   | Actual RN pointer enter/leave/over/out/move events, with ancestor transitions; the native feature flag enables Pressability's pointer-based `onHoverIn` / `onHoverOut`.                |
| Pointer targeting             | Visual stacking, clipping, visibility, enabled state and the four RN `pointerEvents` modes are respected. Generic View overflow follows its RN clipping prop.                          |
| Editable text                 | The existing TextInput bridge retains native editing and focus-event ownership. The generic dispatcher does not synthesize text or duplicate its focus events.                         |
| Back navigation               | `Qt::Key_Back` is left to the separate platform-services contract. Escape is available to controls.                                                                                    |

Key handling is an explicit native contract: synchronous browser
`preventDefault()` semantics are not inferred from a later JavaScript
callback. Controls with a native key handler must implement the activation keys
they own. Native Tab traversal is used instead of DOM queries or browser focus
nodes.

## Actual Fabric interaction probe

`rnqnx_qt_controls_consumer` creates a Qt Quick window, the same QML primitive
sources as the playground, the normal mutation adapter, QtTouchDispatcher and
QtReactHost. It evaluates a production Metro bundle in Hermes, starts a real
Fabric surface, and sends native Qt mouse/key events to the window. App state
and rendered-item assertions verify the outcome. Calls from the driver only
change controlled props or switch a fixture stage.

The default fixture imports public React Native primitives and tests 20 steps:
hover entry/exit, public imperative focus/blur, disabled traversal, ordinary
and custom keyboard activation, pointer-events exclusion, disabling a focused
control, reenabling and surface teardown. It is a normal CTest target required
by the full-renderer validation script when desktop input is enabled.

An external installed-package fixture can use the same runner:

```sh
QT_QUICK_BACKEND=software \
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1 \
build/linux-host-renderer/bin/rnqnx_qt_controls_consumer \
  -platform offscreen \
  --bundle /absolute/path/to/qnx.bundle.js \
  --app RNQnxQtControlsProbe \
  --expected-platform qnx \
  --screenshot /absolute/path/to/control-probe.png
```

Use the separately bundled Marathon artifact with `--expected-platform
marathon`. The runner checks a runtime read of the public `Platform` object's
OS property; the fixture avoids Metro's constant folding with `Reflect.get`.
Native system constants continue to report the actual host operating system.

The external fixture protocol defines `__qtProbeSteps`, `__qtProbeCheck`,
`__qtProbeAction`, `__qtProbePlatform`, and an optional diagnostic
`__qtProbeSnapshot`. Step actions are native hover, click, focus, key and window
leave, plus assertion and controlled-state-change steps. A visible-item check
requires nonzero geometry, visible ancestors, positive opacity and intersection
with the viewport and clipping ancestors. Each condition has a bounded wait;
JavaScript errors and mount failures fail the probe immediately.

## Validation boundary

Execution results will be attached after the new native graph runs. A compiled
bundle, public export, source mapping or synthetic callback unit test does not
establish control runtime acceptance. The new feature flags remain OFF by
default while required target validation is outstanding.

The local native environment uses Qt 6.11.1, RN 0.88.1, Hermes, Linux x86_64,
GCC 14 and the pinned offline source graph. Upstream RN sources use several
Clang-specific pragmas and diagnostics. Any local GCC warning exceptions are
recorded with the execution receipt and remain outside the repository's build
configuration; the hosted Clang lane retains its normal warning policy.

An offscreen Linux result proves the named Qt/Fabric behavior for that bundle
and host. It does not qualify QNX Screen, a physical keyboard or pointer,
Marathon's Wayland shell, screen-reader speech, IME, multi-touch, RNGH,
Reanimated or Worklets. Selected package controls also retain their provider
and application-theme requirements. No performance improvement is claimed.

## Review and rollback

The implementation follows the repository's native API and platform-validation
skills, Margelo's API-design/build-nitro/C++ sequence at
`1e9e17be6f41c838db472d6a4b943fc26355a3be`, and Callstack's RN guidance at
`59e9d17446a248b21d86f078fcd0bd2d36db1ca4`. RN core event plumbing remains a core
adapter; application native modules continue to use the existing Nitro v1
contract. The separate native API, shared-control and integration reviews are
recorded with their respective repositories' evidence.

Disable `RNQNX_ENABLE_DESKTOP_INPUT` and rebuild the complete native graph to
return to the generic RN platform props and the prior pointer path. Disabling
the flag requires a native rebuild because the RN props layout differs. No
application data migration is involved. Optional control and engine packages
remain independently removable.
