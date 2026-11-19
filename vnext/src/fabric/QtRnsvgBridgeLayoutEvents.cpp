#include "QtRnsvgBridgeLayoutEvents.h"

#include <folly/dynamic.h>
#include <react/renderer/core/EventEmitter.h>

#include <algorithm>
#include <cmath>

namespace rnqnx::fabric::rnsvg_bridge {
namespace {

constexpr double kMaxCoordinate = 1.0e9;  // well inside int; no real <Svg> is larger

int clampedInt(double v) {
  if (!std::isfinite(v)) return 0;
  return static_cast<int>(std::clamp(v, -kMaxCoordinate, kMaxCoordinate));
}

}  // namespace

void SvgLayoutNativeEventInterest::set(Predicate predicate) {
  auto next = predicate ? std::make_shared<const Predicate>(std::move(predicate)) : nullptr;
  std::shared_ptr<const Predicate> previous;
  {
    std::lock_guard lock(mutex_);
    previous = std::move(predicate_);
    predicate_ = std::move(next);
  }
  // Destroy callback captures outside the lock, just as invocation is outside.
}

std::shared_ptr<const SvgLayoutNativeEventInterest::Predicate> SvgLayoutNativeEventInterest::snapshot() const {
  std::lock_guard lock(mutex_);
  return predicate_;
}

SvgLayoutReporter::Rect SvgLayoutReporter::toRect(const QRectF& bounds) {
  if (!bounds.isValid() || bounds.isEmpty()) return {};
  return Rect{clampedInt(bounds.x()), clampedInt(bounds.y()), clampedInt(bounds.width()), clampedInt(bounds.height())};
}

SvgLayoutReporter::SvgLayoutReporter(views::RnsvgElementItem* element) : QObject(element), element_(element) {}

SvgLayoutReporter::~SvgLayoutReporter() {
  if (root_) root_->unwatchBounds(element_);
}

void SvgLayoutReporter::attach(views::RnsvgElementItem* element,
                               std::shared_ptr<const facebook::react::EventEmitter> emitter,
                               facebook::react::Tag tag, bool hasReactListener) {
  if (element == nullptr) return;
  auto* reporter = element->findChild<SvgLayoutReporter*>(QString(), Qt::FindDirectChildrenOnly);
  if (reporter == nullptr) reporter = new SvgLayoutReporter(element);
  reporter->emitter_ = std::move(emitter);
  reporter->tag_ = tag;
  reporter->hasReactListener_ = hasReactListener;
  reporter->rebind();
}

void SvgLayoutReporter::setNativeEventInterest(QQuickItem* item,
                                              std::weak_ptr<const SvgLayoutNativeEventInterest> interest) {
  if (item == nullptr) return;
  if (auto* reporter = item->findChild<SvgLayoutReporter*>(QString(), Qt::FindDirectChildrenOnly)) {
    reporter->nativeInterest_ = std::move(interest);
  }
}

// Insert mutations arrive child by child, so an element can be attached to a
// parent before that parent reaches the <Svg>, and a subtree can later move
// under another one. Watching every link from the element up to its root
// catches each of those, whatever order they arrive in.
void SvgLayoutReporter::rebind() {
  for (const auto& link : links_) disconnect(link);
  links_.clear();
  views::RnsvgRootItem* root = nullptr;
  for (QQuickItem* item = element_; item != nullptr; item = item->parentItem()) {
    links_.push_back(connect(item, &QQuickItem::parentChanged, this, &SvgLayoutReporter::rebind));
    root = qobject_cast<views::RnsvgRootItem*>(item->parentItem());
    if (root != nullptr) break;
  }
  if (root == root_.data()) {
    if (root != nullptr) root->requestLayout();  // new emitter: report the current box
    return;
  }
  if (root_) {
    disconnect(root_.data(), &views::RnsvgRootItem::sceneLaidOut, this, &SvgLayoutReporter::evaluate);
    root_->unwatchBounds(element_);
  }
  root_ = root;
  if (root_) {
    connect(root_.data(), &views::RnsvgRootItem::sceneLaidOut, this, &SvgLayoutReporter::evaluate);
    root_->watchBounds(element_);
    root_->requestLayout();  // one polish per commit; the pass ends in sceneLaidOut()
  }
}

void SvgLayoutReporter::evaluate() {
  if (!root_ || !emitter_) return;
  const Rect next = toRect(root_->reportedBounds(element_));
  if (next == last_) return;
  last_ = next;
  if (!hasReactListener_) {
    QPointer<SvgLayoutReporter> alive(this);
    bool interested = false;
    {
      const auto holder = nativeInterest_.lock();
      const auto interest = holder ? holder->snapshot() : nullptr;
      if (interest) interested = (*interest)(tag_);
    }
    // A self-clearing callback can release its last owner when the snapshot
    // dies. Check only after both it and the holder have been released.
    if (!alive || !interested) return;
  }
  folly::dynamic layout = folly::dynamic::object("x", next.x)("y", next.y)("width", next.width)("height", next.height);
  folly::dynamic payload = folly::dynamic::object("layout", std::move(layout));
  // The same event name and payload the generated RNSVG*EventEmitter::onSvgLayout
  // sends. Those emitter classes are not what the package's shadow nodes
  // create (they use ViewEventEmitter), so the event goes out through the
  // base class.
  emitter_->dispatchEvent("svgLayout", std::move(payload));
}

}  // namespace rnqnx::fabric::rnsvg_bridge
