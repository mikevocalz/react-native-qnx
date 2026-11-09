#pragma once

#include <QObject>
#include <QPointer>

class QQuickWindow;

namespace rnqnx::views {

// Explicit, per-window workaround for a target whose wheel direction is reversed.
// The default leaves Qt input unchanged. Construct/destroy on the window's Qt
// owner thread; destruction removes the filter. The window is not owned and may
// be destroyed first. Keep this object alive until event delivery returns.
class QtWheelDirectionFilter final : public QObject {
 public:
  explicit QtWheelDirectionFilter(QQuickWindow& window, bool invert = false);
  ~QtWheelDirectionFilter() override;

  QtWheelDirectionFilter(const QtWheelDirectionFilter&) = delete;
  QtWheelDirectionFilter& operator=(const QtWheelDirectionFilter&) = delete;

 protected:
  bool eventFilter(QObject* watched, QEvent* event) override;

 private:
  QPointer<QQuickWindow> window_;
  bool redispatching_ = false;
};

}  // namespace rnqnx::views
