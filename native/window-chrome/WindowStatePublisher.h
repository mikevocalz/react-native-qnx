#pragma once

// Immutable window snapshots for the Nitro windows API (docs/nitro/API_CONTRACTS.md,
// "Reads and threads"). No Qt in this header: the Nitro adapter includes it
// and reads the latest snapshot from the JavaScript thread without touching a
// Qt object.

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <utility>

namespace rnqnx::window {

// Outer window size and position in device-independent pixels.
struct WindowState {
  std::uint64_t sequence{0};
  std::string title;
  double width{0};
  double height{0};
  double x{0};
  double y{0};
  bool focused{false};
  bool visible{false};
  bool maximized{false};
  std::string controlsLayout;

  // Equality ignores `sequence`: a republish of the same values is no change.
  bool sameValues(const WindowState& other) const;
};

// The title bar inset (docs/nitro/API_CONTRACTS.md, "Inset and SafeAreaView").
struct WindowInset {
  double top{0};
  double right{0};
  double bottom{0};
  double left{0};
  double overlayX{0};
  double overlayY{0};
  double overlayWidth{0};
  double overlayHeight{0};

  bool operator==(const WindowInset& o) const {
    return top == o.top && right == o.right && bottom == o.bottom && left == o.left &&
           overlayX == o.overlayX && overlayY == o.overlayY && overlayWidth == o.overlayWidth &&
           overlayHeight == o.overlayHeight;
  }
};

// The GUI thread publishes; any thread reads. The pointer swap holds a short
// mutex that calls nothing.
class WindowStatePublisher final {
 public:
  WindowStatePublisher();

  // Stores `next` with the next sequence number. Returns nullptr and keeps
  // the old snapshot when nothing changed. Otherwise returns the new snapshot.
  std::shared_ptr<const WindowState> publish(WindowState next);
  std::shared_ptr<const WindowState> current() const;

  // Coalescing for "state changed" posts. `markStatePending` returns true
  // when the caller must schedule a task, false when one is already waiting.
  // The task calls `takePending`, which clears the flag and returns the
  // latest snapshot. Focus and visibility never use this.
  bool markStatePending();
  std::shared_ptr<const WindowState> takePending();

  // The inset follows the same rules: a snapshot, and one pending post at most.
  // Returns false when the inset did not change.
  bool publishInset(const WindowInset& next);
  std::shared_ptr<const WindowInset> currentInset() const;
  bool markInsetPending();
  std::shared_ptr<const WindowInset> takeInsetPending();

 private:
  mutable std::mutex mutex_;
  std::shared_ptr<const WindowState> current_;
  std::uint64_t sequence_{0};
  bool pending_{false};
  std::shared_ptr<const WindowInset> inset_{std::make_shared<const WindowInset>()};
  bool insetPending_{false};
};

inline bool WindowState::sameValues(const WindowState& other) const {
  return title == other.title && width == other.width && height == other.height && x == other.x &&
         y == other.y && focused == other.focused && visible == other.visible &&
         maximized == other.maximized && controlsLayout == other.controlsLayout;
}

inline WindowStatePublisher::WindowStatePublisher() : current_(std::make_shared<const WindowState>()) {}

inline std::shared_ptr<const WindowState> WindowStatePublisher::publish(WindowState next) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (sequence_ > 0 && current_->sameValues(next)) return nullptr;
  next.sequence = ++sequence_;
  current_ = std::make_shared<const WindowState>(std::move(next));
  return current_;
}

inline std::shared_ptr<const WindowState> WindowStatePublisher::current() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return current_;
}

inline bool WindowStatePublisher::markStatePending() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (pending_) return false;
  pending_ = true;
  return true;
}

inline std::shared_ptr<const WindowState> WindowStatePublisher::takePending() {
  std::lock_guard<std::mutex> lock(mutex_);
  pending_ = false;
  return current_;
}

inline bool WindowStatePublisher::publishInset(const WindowInset& next) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (*inset_ == next) return false;
  inset_ = std::make_shared<const WindowInset>(next);
  return true;
}

inline std::shared_ptr<const WindowInset> WindowStatePublisher::currentInset() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return inset_;
}

inline bool WindowStatePublisher::markInsetPending() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (insetPending_) return false;
  insetPending_ = true;
  return true;
}

inline std::shared_ptr<const WindowInset> WindowStatePublisher::takeInsetPending() {
  std::lock_guard<std::mutex> lock(mutex_);
  insetPending_ = false;
  return inset_;
}

}  // namespace rnqnx::window
