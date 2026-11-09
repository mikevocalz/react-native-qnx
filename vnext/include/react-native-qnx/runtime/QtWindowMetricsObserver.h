#pragma once

#include "react-native-qnx/runtime/WindowMetrics.h"

#include <QObject>
#include <QPointer>
#include <QTimer>

#include <functional>
#include <vector>

class QQuickItem;
class QScreen;
class QWindow;

namespace rnqnx::runtime {

/// Owner-thread observation of one React client area and its actual window.
/// The callback receives a coherent snapshot after coalesced geometry changes.
/// Destruction disconnects all producers and discards any queued notification.
class QtWindowMetricsObserver final : public QObject {
 public:
  using Changed = std::function<void(const WindowMetrics&)>;

  QtWindowMetricsObserver(QWindow& window, QQuickItem& client, Changed changed);
  ~QtWindowMetricsObserver() override;

  const WindowMetrics& current() const noexcept;
  void setFontScale(double scale);
  void close() noexcept;

 protected:
  bool eventFilter(QObject* watched, QEvent* event) override;

 private:
  WindowMetrics read() const;
  void watchScreen(QScreen* screen);
  void scheduleRefresh();
  void refresh();

  QPointer<QWindow> window_;
  QPointer<QQuickItem> client_;
  QTimer pending_;
  std::vector<QMetaObject::Connection> screenConnections_;
  Changed changed_;
  WindowMetrics current_;
  double fontScale_ = 1.0;
  bool active_ = true;
};

}  // namespace rnqnx::runtime
