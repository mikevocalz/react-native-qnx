# Qt accessibility and back navigation

This slice supplies React Native's existing `AccessibilityInfo` and
`BackHandler` contracts through the shared Qt host. It is an RN core adapter,
not a new application-facing native-module system. Nitro v1 remains the
application-module API.

Enable `RNQNX_ENABLE_PLATFORM_SERVICES=ON` with
`RNQNX_ENABLE_ACCESSIBILITY_BRIDGE=ON` in a Linux or QNX Qt host. Both options
default OFF. The public imports are available with the option OFF; invoking a
native operation reports the missing provider. QNX and Marathon share source
but retain their separate JavaScript identity and separately bundled public
entries. These Linux host fixtures execute both entries with one native host
test executable.

## Public contract

```ts
import { AccessibilityInfo, BackHandler } from 'react-native';

const back = BackHandler.addEventListener('hardwareBackPress', () => {
  if (!drawer.isOpen()) return false;
  drawer.close();
  return true;
});
// A subscription belongs to its caller; removal is idempotent.
back.remove();

const reader = AccessibilityInfo.addEventListener('screenReaderChanged', (enabled) => {
  presentation.setAssistiveClientActive(enabled);
});
const active = await AccessibilityInfo.isScreenReaderEnabled();
reader.remove();

try {
  const reduced = await AccessibilityInfo.isReduceMotionEnabled();
  presentation.setReducedMotion(reduced);
} catch (error) {
  // Qt has no public system reduced-motion provider in the selected SDK.
  // Keep the app's explicit preference; do not invent an OS setting.
  presentation.reportUnavailablePreference(error);
}
```

The reader query reflects `QAccessible::isActive()`: an active Qt accessibility
client, which can include a screen reader or another assistive tool. It does
not prove that speech is enabled. Contrast follows the selected Qt backend's
`QAccessibilityHints::contrastPreference`; `NoPreference` reports false.
Announcements submit `QAccessibleAnnouncementEvent` to Qt. Actual spoken
output still requires a working platform accessibility bridge and client.

Only supported change events can be subscribed. Reduced motion, grayscale,
inverted colors, bold text, transparency, cross-fade preferences, timeout
preferences and announcement-completion callbacks have no selected Qt
provider and fail explicitly. An app must guard optional subscriptions as
well as optional queries. A queued-announcement requirement is unsupported;
Qt politeness is best-effort guidance to the accessibility client.

Back navigation consumes the owning window's `Qt::Key_Back` release, dispatches
React Native's handlers in reverse registration order, and closes only that
window if none handles it. Escape remains available to controls. Open React
Native modals receive `onRequestClose` instead of `hardwareBackPress`.

### Availability and errors

| Public operation                                                          | Qt behavior                                                                            |
| ------------------------------------------------------------------------- | -------------------------------------------------------------------------------------- |
| `isScreenReaderEnabled`, `isAccessibilityServiceEnabled`                  | Promise of the current Qt accessibility-client activation state                        |
| `screenReaderChanged`, `accessibilityServiceChanged`, deprecated `change` | Independent removable subscriptions to Qt activation changes                           |
| `isHighTextContrastEnabled`, `isDarkerSystemColorsEnabled`                | Promise of the Qt high-contrast preference                                             |
| `highTextContrastChanged`, `darkerSystemColorsChanged`                    | Independent removable subscriptions to the Qt contrast signal                          |
| `setAccessibilityFocus`, `sendAccessibilityEvent(handle, 'focus')`        | Validate a live exposed mounted item, take actual Qt focus and submit a Qt focus event |
| `announceForAccessibility`, `announceForAccessibilityWithOptions`         | Submit a Qt announcement with polite or assertive priority; `queue: true` rejects      |
| Other preference queries/events and non-focus accessibility events        | Explicit unsupported error                                                             |

Known unavailable operations use real JavaScript `Error` objects with stable
`code` values: `ERR_QT_ACCESSIBILITY_UNSUPPORTED` means Qt has no selected
provider for the operation; `ERR_QT_ACCESSIBILITY_UNAVAILABLE` means the
optional native services were not enabled. Optional preference integrations
may retain an app-owned value for those two codes. Invalid arguments, stale
React tags and other native errors remain visible to the caller.

`BackHandler` reuses the exact pinned React Native 0.88.1 Android JavaScript
handler implementation against Qt's internal `DeviceEventManager` module.
That preserves last-registered-first dispatch, callback deduplication,
timestamped events and idempotent removal. It neither changes `Platform.OS`
nor requires an Android runtime. A modal consumes system back before the
handler list. Explicit `exitApp()` requests closure of the host window;
normal Qt close policy can decline that request. Auto-repeat and modified
back keys do not dispatch extra application actions. Module invalidation
disconnects native producers before the runtime queue closes; deferred work
holds weak state rather than a retained JSI runtime.

## Generic View semantics and registered actions

The existing accessibility bridge still controls generic View semantics.
With platform services enabled, a scoped public `QAccessible` factory handles
only generic Views owned by this renderer. Other Qt objects retain their
native factories. Names, descriptions, roles and check/selection state use
the public QML `Accessible` properties. The `role` prop takes precedence over
`accessibilityRole`; the public view configuration admits both, value text,
expanded/busy/modal state and explicit hidden-subtree props.
Changes to expanded, busy and modal state submit public Qt state-change
events; value text changes submit a public Qt value-change event after the
new metadata is queryable. Event delivery remains guarded if a synchronous
accessibility observer destroys the target.

The expanded role mappings include combobox, menu/menuitem/menubar,
dialog, list/option, slider/adjustable, progressbar, tab/tablist,
toolbar, tooltip, grouping, tree/treeitem and table/row/cell/header roles.
A role mapping supplies semantic identity. It does not add a numeric value
setter, table model, selection model or editable text implementation to a
generic View. `accessibilityValue.text` is exposed as text; this slice does
not claim a Qt value interface for arbitrary numeric slider setters.

The public `role` must also be accepted by the pinned RN parser. RN 0.88.1
accepts `list` and `option`, but does not accept `listbox`. Shared controls
keep `listbox` for the web and use `list` for the native container, which
maps to Qt's List role. A generic View needs `accessible={true}` to expose
its own semantic node; ordinary ignored wrappers retain their accessible
descendants.

| Registered RN action | Public Qt action |
| -------------------- | ---------------- |
| `activate`           | `pressAction`    |
| `longpress`          | `showMenuAction` |
| `increment`          | `increaseAction` |
| `decrement`          | `decreaseAction` |

Each action reaches the actual RN `onAccessibilityAction` event emitter.
Only supported, unique registered names are admitted; other names reject
the mutation explicitly. Disabled, invisible, transparent, unregistered,
handler-less and obsolete receiver generations do not dispatch. Native focus
is advertised separately when the item supports it. QML has no public
`Accessible.onShowMenuAction` signal, so the public C++ action interface is
required for an independent longpress action.

Ordinary ignored layout wrappers preserve their semantic descendants.
Explicit `accessibilityElementsHidden` or `no-hide-descendants` roots instead
form an invisible, empty accessibility boundary so Qt's native window
traversal cannot flatten through them into native text. The boundary does
not change descendants' QML bindings, visual visibility or input enablement.
Focus validation also rejects hidden ancestors because cached native Qt
interfaces can remain valid after they leave the exposed tree. Semantic hit
testing follows visual z order, ancestor clipping and nested accessible
children. No target screen-reader speech has been established by these
native tree checks.

## Implementation references

The API was inspected before implementation against installed RN 0.88.1
`AccessibilityInfo.js`, `BackHandler.android.js`, the core device event
emitter, and the native DeviceEventManager specification. Qt contracts were
checked against the selected Qt 6.11.1 public accessibility, announcement,
contrast and Qt Quick accessibility APIs. This does not upgrade dependencies.

The required Margelo skills were read in API-design, Nitro, then C++ order
at `1e9e17be6f41c838db472d6a4b943fc26355a3be`: define explicit contracts,
retain the Nitro application boundary, and keep native ownership on the Qt
thread with weak queued callbacks. Callstack React Native guidance at
`59e9d17446a248b21d86f078fcd0bd2d36db1ca4` informed the real-renderer
interaction tests. These adapters implement RN's own internal module
contracts; application native modules continue to use Nitro v1.

## Validation boundary

`RNQNX_BUILD_PLATFORM_SERVICES_TESTS=ON` registers production Metro/Hermes
fixtures for both QNX and Marathon identities. It also exercises explicit
unavailability when services are OFF. The enabled native semantic-tree test
checks roles, actions, identity, hidden roots, native binding preservation,
clipping, focus and teardown. The host CI matrix enables these tests in both
the ON and OFF configurations and fails if their inventory is absent.

The Linux host run at source
`92e88b9111b29c9090f9bac63b906dad4cf10f74` used Qt 6.11.1, the pinned
RN 0.88.1, production Metro bundles, Hermes and the real Fabric mounting
path. The [host receipt](../evidence/qt-platform-services-host-2026-11-10.json)
records source and artifact hashes, configuration, toolchain, test output
and the local GCC warning exceptions used for unchanged upstream code.

| Configuration                           | Executed checks                                                                | Result                                                                                          |
| --------------------------------------- | ------------------------------------------------------------------------------ | ----------------------------------------------------------------------------------------------- |
| Platform services ON, desktop input ON  | Native semantic tree, QNX and Marathon public API fixtures, mounting host test | 4/4 CTests passed; the mounting executable contains 21 cases                                    |
| Platform services OFF, desktop input ON | Both public API fixtures and the mounting host test                            | 3/3 CTests passed; unavailable providers reject explicitly and all 21 mounting cases still pass |
| Enabled configuration restored          | The same semantic-tree, public API and mounting checks after rebuilding        | 4/4 CTests passed; restored artifacts match the first enabled run                               |

The existing mutation-adapter and ShadowTree mounting tests also passed
2/2 CTests on the restored enabled graph: seven adapter cases and five
ShadowTree cases. They cover ordinary insertion order, updates, removal,
nested trees, error reporting and actual Yoga geometry through RN commits.

The public API fixtures compare snapshots with actual Qt state, exercise
independent subscription removal, deliver synthetic Qt activation/contrast
signals and Back events, and observe actual Qt focus, announcement, state
and value-change events. They test registered accessibility actions through
the RN event emitter, modal Back priority, owning-window default closure,
invalid arguments and queued-callback invalidation. These checks do not
change an operating-system accessibility preference or observe spoken
output.

The Modal regression uses a public RN Modal declared inside a clipped,
offset 180-by-120 View. Its native content escapes that ancestor at the
window origin; the full-flex child reports actual Yoga and `onLayout`
dimensions of 320-by-240, then 480-by-360 after a window resize. `onShow`
remains once per presentation. A second presentation is removed after a
resize is queued but before the Qt sizing callback runs, with no revived
portal or late `onShow`. Five of the 21 mounting cases separately verify
portal clipping, logical child indices, invalid removal, tag reuse and
teardown.

ASan/UBSan instrument the platform-services runtime and
QtQuickMountingLayer test executables, with leak detection disabled. The
framework's static libraries are not globally instrumented. The separate
pure Qt semantic-tree and the two Fabric integration executables are not
sanitizer-instrumented. An
earlier five-test accessibility regression run is retained as separate,
historical evidence in the receipt. Its passing same-surface remount probe
logged an RN LeakChecker diagnostic for three components; that earlier run
is not described as warning-free or as a final-source rerun.

The OFF check above deliberately retains desktop input to exercise the mixed
feature configuration; it is separate from the hosted all-options-OFF matrix.
No QNX/Marathon device, screen-reader speech or physical-keyboard acceptance
is implied by these synthetic Qt events and Linux host results. The feature
flags retain their default OFF values.
