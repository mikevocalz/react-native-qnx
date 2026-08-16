#pragma once

#include <QObject>
#include <QQuickItem>

#include <functional>

namespace rnqnx::fabric {

/// Turns a QML `Switch` value change into a callback.
///
/// The item is parented to the QML item, so it is destroyed with the switch.
class QtSwitchBridge final : public QObject {
  Q_OBJECT

 public:
  QtSwitchBridge(
      std::function<void(bool)> onChange,
      QQuickItem* item,
      bool initialValue,
      QObject* parent = nullptr);

  void setExpectedValue(bool value);

 public Q_SLOTS:
  void notify();

 private:
  std::function<void(bool)> onChange_;
  QQuickItem* item_;
  bool lastValue_;
};

}  // namespace rnqnx::fabric
