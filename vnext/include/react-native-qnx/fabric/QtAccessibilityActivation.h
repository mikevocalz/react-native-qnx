#pragma once
#include <QObject>
#include <QPointer>
#include <QQuickItem>
#include <QString>
#include <QStringList>
#include <cstdint>
#include <functional>
#include <string>
namespace rnqnx::fabric {
struct AccessibilityActivationPolicy {
  bool accessible{false};
  bool registered{false};
  bool hasHandler{false};
  bool disabled{false};
};
// One immutable mounted-props generation, parented to its View. No runtime/JSI
// ownership. Qt's default advertised actions are not the RN registered list.
class QtAccessibilityActivation final : public QObject {
  Q_OBJECT
 public:
  using Emit = std::function<void()>;
  QtAccessibilityActivation(QQuickItem& item, AccessibilityActivationPolicy policy, Emit callback);
#if RNQNX_ENABLE_PLATFORM_SERVICES
  using NamedEmit = std::function<void(const std::string&)>;
  QtAccessibilityActivation(QQuickItem& item, AccessibilityActivationPolicy policy,
      QStringList actions, NamedEmit callback);
  QStringList actionNames() const;
  void performAction(const QString& action);
#endif
  bool connectPress(QString& error);
  void invalidate() noexcept;
  std::uint64_t generation() const { return generation_; }
 public Q_SLOTS:
  void activate();
 private:
  QPointer<QQuickItem> item_;
  AccessibilityActivationPolicy policy_;
  Emit emit_;
#if RNQNX_ENABLE_PLATFORM_SERVICES
  QStringList actions_;
  NamedEmit namedEmit_;
#endif
  QMetaObject::Connection connection_;
  const std::uint64_t generation_;
  bool live_{true};
};
}
