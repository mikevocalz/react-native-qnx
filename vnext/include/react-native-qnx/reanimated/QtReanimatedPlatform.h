#pragma once

#include "react-native-qnx/runtime/QtReactHost.h"
#include "react-native-qnx/worklets/QtWorkletsPlatform.h"

#include <memory>

namespace rnqnx::reanimated {

/// Hosts react-native-reanimated on top of QtWorkletsPlatform.
///
/// Registers the `ReanimatedModule` TurboModule (ADR-0020). Reanimated's own
/// JavaScript then calls `installTurboModule()`, which builds the upstream
/// `ReanimatedModuleProxy` with the Qt platform methods:
///
///  - frames come from QtWorkletsPlatform::requestFrame (Qt Quick
///    `afterAnimating`),
///  - property writes that need no layout go to the GUI thread and into the
///    mounted Qt item through `synchronouslyUpdateViewOnUIThread`,
///  - everything else is committed to the Fabric shadow tree from the UI
///    thread, and mounted on the GUI thread through the host's owner poster,
///  - Fabric events reach worklet handlers through `Scheduler::addEventListener`
///    and run on the UI thread.
///
/// Sensors, keyboard events, pseudo-selectors and platform CSS backends report
/// themselves unsupported. Call after `QtWorkletsPlatform::install` and before
/// `host.installTurboModuleBinding()`.
void installReanimated(
    runtime::QtReactHost& host,
    const std::shared_ptr<worklets::QtWorkletsPlatform>& worklets);

}  // namespace rnqnx::reanimated
