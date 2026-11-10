#pragma once

#include <QObject>
#include <QQuickItem>
#include <QPointer>

#include <functional>

namespace rnqnx::fabric {

/// Turns a QML `Switch` user toggle into a callback; JS writes do not echo.
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
  void setValue(bool value);
  void setOnChange(std::function<void(bool)> onChange);

 public Q_SLOTS:
  void notify();

 private:
  std::function<void(bool)> onChange_;
  QPointer<QQuickItem> item_;
  bool lastValue_;
};

}  // namespace rnqnx::fabric
