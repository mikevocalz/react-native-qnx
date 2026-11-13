#pragma once

#include "react-native-qnx/gesture/GestureHandlerCore.h"

#include <QObject>
#include <QTimer>

#include <chrono>

class QQuickWindow;

namespace rnqnx::gesture {

/// Feeds window pointer events (mouse and touch) to the recognizer registry.
/// It observes only: every event still reaches QtTouchDispatcher and the items,
/// so React responders keep working next to Gesture Handler. Arbitration with
/// Qt scrolling is not implemented (docs/reanimated-gesture-handler-qnx.md).
class QtGestureInput final : public QObject {
 public:
  QtGestureInput(Registry& registry, QQuickWindow& window);

 protected:
  bool eventFilter(QObject* watched, QEvent* event) override;

 private:
  static double nowMs();
  void rearm();

  Registry& registry_;
  QTimer deadline_;
};

}  // namespace rnqnx::gesture
