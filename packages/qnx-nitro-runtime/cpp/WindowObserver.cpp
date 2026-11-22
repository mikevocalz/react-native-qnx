#include "WindowObserver.hpp"
#include <utility>
namespace rnqnx::nitro::windowapi {
WindowSnapshot windowSnapshot(const QVariantMap& value) {
  const auto inset = value.value(QStringLiteral("insets")).toMap();
  return WindowSnapshot{
    value.value(QStringLiteral("title")).toString().toStdString(),
    value.value(QStringLiteral("x")).toDouble(), value.value(QStringLiteral("y")).toDouble(),
    value.value(QStringLiteral("width")).toDouble(), value.value(QStringLiteral("height")).toDouble(),
    value.value(QStringLiteral("contentWidth")).toDouble(), value.value(QStringLiteral("contentHeight")).toDouble(),
    value.value(QStringLiteral("visible")).toBool(), value.value(QStringLiteral("focused")).toBool(),
    value.value(QStringLiteral("minimized")).toBool(), value.value(QStringLiteral("closeRequested")).toBool(),
    value.value(QStringLiteral("closeRequestId")).toString().toStdString(),
    value.value(QStringLiteral("controlsLayout")).toString().toStdString(),
    WindowInsets{inset.value(QStringLiteral("top")).toDouble(), inset.value(QStringLiteral("right")).toDouble(),
                 inset.value(QStringLiteral("bottom")).toDouble(), inset.value(QStringLiteral("left")).toDouble()},
    value.value(QStringLiteral("canSetPosition")).toBool(), value.value(QStringLiteral("canResize")).toBool(),
    value.value(QStringLiteral("canPreventClose")).toBool(), value.value(QStringLiteral("canMinimize")).toBool()};
}
WindowObserver::WindowObserver(std::weak_ptr<rnqnx::window::WindowSessionState> source,
    std::shared_ptr<std::atomic<bool>> handleActive,
    margelo::nitro::Callback<void(WindowSnapshot)> callback,
    std::shared_ptr<margelo::nitro::Dispatcher> dispatcher)
    : source_(std::move(source)), handleActive_(std::move(handleActive)),
      callback_(std::move(callback)), dispatcher_(std::move(dispatcher)) {}
bool WindowObserver::live() const {
  if (!active_.load() || !handleActive_->load()) return false;
  const auto source = source_.lock();
  if (!source) return false;
  std::lock_guard lock(source->mutex);
  return source->owner != nullptr;
}
void WindowObserver::stop() { active_.store(false); }
void WindowObserver::enqueue(std::uint64_t sequence, const QVariantMap& value, bool initial) {
  if (!live()) return;
  bool post = false;
  {
    std::lock_guard lock(mutex_);
    if (initial) {
      initial_ = windowSnapshot(value);
      initialized_ = true;
      if (latest_ && latestSequence_ <= sequence) latest_.reset();
      latestSequence_ = std::max(latestSequence_, sequence);
    } else if (sequence > latestSequence_) {
      latest_ = windowSnapshot(value);
      latestSequence_ = sequence;
    }
    if (initialized_ && !scheduled_) { scheduled_ = true; post = true; }
  }
  if (post) {
    const auto weak = weak_from_this();
    try { dispatcher_->runAsync([weak] { if (auto self = weak.lock()) self->drain(); }); }
    catch (...) { stop(); } // Owner admission closed: no later callback may enter JS.
  }
}
void WindowObserver::drain() {
  std::optional<WindowSnapshot> initial, latest;
  {
    std::lock_guard lock(mutex_);
    initial.swap(initial_);
    latest.swap(latest_);
    scheduled_ = false;
  }
  if (initial && live()) callback_.callSync(*initial);
  if (latest && live()) callback_.callSync(*latest);
}
} // namespace rnqnx::nitro::windowapi
