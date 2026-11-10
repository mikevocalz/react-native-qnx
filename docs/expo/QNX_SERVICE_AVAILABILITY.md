# Expo service availability on QNX

Use ordinary `expo-modules-core` imports. With the QNX Metro configuration, a
required native module that is absent throws an error with
`code: 'ERR_QNX_EXPO_MODULE_UNAVAILABLE'` and its `moduleName`.
`requireOptionalNativeModule` retains Expo SDK 58's `null` result for an absent
module. The resolver does not register dummy modules or request permissions.

```ts
import { requireNativeModule, requireOptionalNativeModule } from 'expo-modules-core';

// Optional feature: absence is not a denied permission and not a device result.
const camera = requireOptionalNativeModule('ExpoCamera');
if (camera === null) {
  // Hide the optional camera workflow or explain that this build lacks it.
}

// Required feature: acquisition fails explicitly if this native backend is absent.
const requiredCamera = requireNativeModule('ExpoCamera');
// Use the installed package's typed public API for actual camera operations.
```

The wrapper delegates to the installed Expo implementation, including its
installer, JSI lookup, bridge proxy and TurboModule fallback. It does not catch
initialization failures or replace the returned native object. The native
provider still owns permission requests, denied/revoked authorization, hardware
availability and resource cleanup. A registered module is not proof that a
camera or sensor exists or that permission was granted.

The QNX public React Native entry now exposes the real framework registry,
native event emitter and view-registry exports that Expo Modules Core imports.
These exports do not create an Expo service or a second renderer.

## Backend requirements

At this implementation baseline the built-in QNX Expo registry provides
`ExpoQnxPlatform`, not Camera, Sensors or Notifications. Those workflows need
actual target-specific native providers before they can run:

| Workflow      | Required work beyond JavaScript installation                                                                      |
| ------------- | ----------------------------------------------------------------------------------------------------------------- |
| Camera        | QNX capture provider, actual permission policy, native view/session integration, ownership and teardown           |
| Sensors       | Named physical sensor provider, availability/authorization, timestamped events, subscription removal and shutdown |
| Notifications | QNX notification service, permission policy, delivery/cancellation, lifecycle and unavailable-service behavior    |

Native providers must register through the existing Expo QNX mechanism, use
owned native resources and pass their target-specific permission/lifecycle tests.
Do not provide false permission-denied responses, synthetic sensor readings or
successful no-op notifications to compensate for an absent backend.

## Verification and limits

`node tests/js/expo-qnx-service-availability.test.mjs` executes the installed
Expo lookup source against host registry/installer fixtures. It checks missing
services, real object passthrough, unchanged optional lookup, installer failures,
permission side-effect isolation, and QNX-only resolver behavior.

`node tests/js/expo-qnx-service-metro.test.mjs` builds ordinary public Expo imports
with real Metro for QNX and Marathon. Only the QNX bundle receives the diagnostic
wrapper. Both tests run in the existing JavaScript suite. They do not claim a
native Camera/Sensor/Notification execution or physical QNX acceptance.

The resolver rejects multiple distinct Expo Modules Core lookup implementations
in one QNX bundle. Deduplicate Expo Modules Core to the repository's matched SDK
version rather than silently using the wrong native-module singleton.
