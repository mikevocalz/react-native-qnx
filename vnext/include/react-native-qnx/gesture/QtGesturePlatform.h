#pragma once

#include "react-native-qnx/fabric/QtQuickMountingLayer.h"
#include "react-native-qnx/fabric/ShadowViewMutationAdapter.h"
#include "react-native-qnx/gesture/GestureHandlerCore.h"
#include "react-native-qnx/runtime/QtReactHost.h"
#include "react-native-qnx/worklets/QtWorkletsPlatform.h"

#include <memory>

class QQuickWindow;

namespace rnqnx::gesture {

/// react-native-gesture-handler 3 on Qt.
///
/// Native pieces: the `RNGestureHandlerModule` TurboModule, the three Fabric
/// components (detector, root view, button; each mounts as a plain container
/// item), a Qt pointer filter on the window, and the recognizer registry in
/// GestureHandlerCore. Recognized transitions go to the detector view's
/// EventEmitter as `onGestureHandler*` (or `*Reanimated*` for worklet handlers,
/// which Reanimated runs on the UI thread).
///
/// Call order: `registerComponents()` before the first QtReactHost exists;
/// `install(...)` after the host and Worklets platform, before
/// `host.installTurboModuleBinding()`.
void registerComponents();

class QtGesturePlatform {
 public:
  static std::shared_ptr<QtGesturePlatform> install(
      runtime::QtReactHost& host,
      fabric::ShadowViewMutationAdapter& adapter,
      fabric::QtQuickMountingLayer& layer,
      QQuickWindow& window,
      const std::shared_ptr<worklets::QtWorkletsPlatform>& worklets);

  virtual ~QtGesturePlatform() = default;
  virtual Registry& registry() = 0;
};

}  // namespace rnqnx::gesture
