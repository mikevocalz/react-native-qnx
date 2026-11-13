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

SvgLayoutReporter::Rect SvgLayoutReporter::toRect(const QRectF& bounds) {
  if (!bounds.isValid() || bounds.isEmpty()) return {};
  return Rect{clampedInt(bounds.x()), clampedInt(bounds.y()), clampedInt(bounds.width()), clampedInt(bounds.height())};
}

SvgLayoutReporter::SvgLayoutReporter(views::RnsvgElementItem* element) : QObject(element), element_(element) {}

SvgLayoutReporter::~SvgLayoutReporter() {
  if (root_) root_->unwatchBounds(element_);
}

void SvgLayoutReporter::attach(views::RnsvgElementItem* element,
                               std::shared_ptr<const facebook::react::EventEmitter> emitter) {
  if (element == nullptr) return;
  auto* reporter = element->findChild<SvgLayoutReporter*>(QString(), Qt::FindDirectChildrenOnly);
  if (reporter == nullptr) reporter = new SvgLayoutReporter(element);
  reporter->emitter_ = std::move(emitter);
  reporter->rebind();
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
  folly::dynamic layout = folly::dynamic::object("x", next.x)("y", next.y)("width", next.width)("height", next.height);
  folly::dynamic payload = folly::dynamic::object("layout", std::move(layout));
  // The same event name and payload the generated RNSVG*EventEmitter::onSvgLayout
  // sends. Those emitter classes are not what the package's shadow nodes
  // create (they use ViewEventEmitter), so the event goes out through the
  // base class.
  emitter_->dispatchEvent("svgLayout", std::move(payload));
}

}  // namespace rnqnx::fabric::rnsvg_bridge
