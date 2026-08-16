#pragma once

#include <QObject>
#include <QQuickItem>

#include <functional>

#include <react/renderer/imagemanager/primitives.h>

namespace rnqnx::fabric {

/// Turns a QML `Image` status change into image load lifecycle callbacks.
class QtImageBridge final : public QObject {
  Q_OBJECT

 public:
  using LoadCallback =
      std::function<void(const facebook::react::ImageSource&)>;
  using ErrorCallback =
      std::function<void(const facebook::react::ImageErrorInfo&)>;
  using VoidCallback = std::function<void()>;

  QtImageBridge(
      QQuickItem* item,
      facebook::react::ImageSource source,
      VoidCallback onLoadStart,
      LoadCallback onLoad,
      VoidCallback onLoadEnd,
      ErrorCallback onError,
      QObject* parent = nullptr);

 public Q_SLOTS:
  void notify();

 private:
  QQuickItem* item_;
  facebook::react::ImageSource source_;
  VoidCallback onLoadStart_;
  LoadCallback onLoad_;
  VoidCallback onLoadEnd_;
  ErrorCallback onError_;
  int lastStatus_{-1};
  bool startFired_{false};
  bool endFired_{false};
};

}  // namespace rnqnx::fabric
