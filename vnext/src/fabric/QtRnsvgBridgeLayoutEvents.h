#pragma once

// onSvgLayout for react-native-svg elements.
//
// Upstream's native element reports its client rect to JS when that rect
// changes. Here the rect comes from the <Svg> root's scene
// (RnsvgRootItem::reportedBounds, measured for all reporters of a root in one
// traversal per layout pass), which is valid only after the root has laid
// its tree out. A reporter therefore waits for the root's sceneLaidOut()
// signal, once per layout pass and never inside paint(), and sends an event
// only when the integer rect differs from the last one observed and either
// React or a native subscriber is interested. Tracking stays active without
// listeners so adding one preserves the same changed-bounds semantics.

#include <RnsvgItems.h>
#include <react/renderer/core/ReactPrimitives.h>

#include <QMetaObject>
#include <QObject>
#include <QPointer>
#include <QVector>

#include <functional>
#include <memory>
#include <mutex>

namespace facebook::react {
class EventEmitter;
}

namespace rnqnx::fabric::rnsvg_bridge {

// One holder per mutation adapter. A reporter only borrows it; replacing or
// clearing native interest reaches existing reporters without a props update.
class SvgLayoutNativeEventInterest final {
 public:
  using Predicate = std::function<bool(facebook::react::Tag)>;
  void set(Predicate predicate);
  std::shared_ptr<const Predicate> snapshot() const;

 private:
  mutable std::mutex mutex_;
  std::shared_ptr<const Predicate> predicate_;
};

class SvgLayoutReporter final : public QObject {
  Q_OBJECT

 public:
  // Creates the reporter on `element` (as its child) or updates its emitter.
  static void attach(views::RnsvgElementItem* element, std::shared_ptr<const facebook::react::EventEmitter> emitter,
                     facebook::react::Tag tag, bool hasReactListener);
  static void setNativeEventInterest(QQuickItem* item, std::weak_ptr<const SvgLayoutNativeEventInterest> interest);

  struct Rect {
    int x{0}, y{0}, width{0}, height{0};
    bool operator==(const Rect& o) const { return x == o.x && y == o.y && width == o.width && height == o.height; }
  };
  // The rect JS receives for `bounds`: truncated toward zero like upstream's
  // int cast. An enormous value is clamped and a non-finite one reads as 0, so
  // a hostile box cannot overflow the int.
  static Rect toRect(const QRectF& bounds);

  ~SvgLayoutReporter() override;

 private Q_SLOTS:
  void rebind();
  void evaluate();

 private:
  explicit SvgLayoutReporter(views::RnsvgElementItem* element);

  views::RnsvgElementItem* element_;
  QPointer<views::RnsvgRootItem> root_;
  // parentChanged of the element and of every ancestor up to its root: any
  // re-parenting along that chain can change which root the element is under.
  QVector<QMetaObject::Connection> links_;
  std::shared_ptr<const facebook::react::EventEmitter> emitter_;
  std::weak_ptr<const SvgLayoutNativeEventInterest> nativeInterest_;
  facebook::react::Tag tag_{};
  bool hasReactListener_{false};
  Rect last_;  // upstream starts at CGRectZero and reports changes from it
};

}  // namespace rnqnx::fabric::rnsvg_bridge
