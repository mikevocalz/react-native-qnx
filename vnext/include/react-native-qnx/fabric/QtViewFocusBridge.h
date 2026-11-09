#pragma once

#include <QMetaObject>
#include <QObject>
#include <QQuickItem>

#include <functional>
#include <optional>

namespace rnqnx::fabric {

/// Observes a generic View's real Qt focus, separately from TextInput events.
/// The item owns this receiver; the adapter disconnects it while detached and
/// replaces its callback when Fabric supplies a newer event emitter.
class QtViewFocusBridge final : public QObject {
 public:
  using Changed = std::function<void(bool)>;

  explicit QtViewFocusBridge(QQuickItem& item);
  void update(Changed changed, std::function<void()> activate, bool tabFocusable);
  void setMounted(bool mounted);

 protected:
  bool eventFilter(QObject* watched, QEvent* event) override;

 private:
  void applyTabPolicy();
  QQuickItem& item_;
  Changed changed_;
  std::function<void()> activate_;
  QMetaObject::Connection connection_;
  bool mounted_ = false;
  bool tabFocusable_ = false;
  std::optional<int> pressedKey_{std::nullopt};
};

}  // namespace rnqnx::fabric
