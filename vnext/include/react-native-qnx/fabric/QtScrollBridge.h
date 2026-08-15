#pragma once

#include <QObject>

#include <functional>

namespace rnqnx::fabric {

/// Turns a Qt signal into a call to a std::function.
///
/// Qt's functor-based `connect` needs the signal as a compile-time member
/// pointer, and `Flickable::contentYChanged` is not visible to this code —
/// Flickable's class is private to Qt Quick and the item is only known through
/// its metaobject. The string-based `connect` works with a metaobject but
/// requires a real slot on the receiver, which is what this provides.
class QtScrollBridge final : public QObject {
  Q_OBJECT

 public:
  explicit QtScrollBridge(std::function<void()> onSignal, QObject* parent);

 public Q_SLOTS:
  void notify();

 private:
  std::function<void()> onSignal_;
};

}  // namespace rnqnx::fabric
