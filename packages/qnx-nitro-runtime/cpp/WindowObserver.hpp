#pragma once
#include "generated/WindowBindings.hpp"
#include "WindowSession.h"
#include <NitroModules/Dispatcher.hpp>
#include <atomic>
#include <memory>
#include <mutex>
#include <optional>

namespace rnqnx::nitro::windowapi {
WindowSnapshot windowSnapshot(const QVariantMap& value);
// A bounded two-slot mailbox: preserve the initial snapshot, then coalesce
// changes while JS is busy. No unbounded Qt/JS event backlog, no sync GUI->JS.
class WindowObserver final : public std::enable_shared_from_this<WindowObserver> {
 public:
  WindowObserver(std::weak_ptr<rnqnx::window::WindowSessionState> source,
                 std::shared_ptr<std::atomic<bool>> handleActive,
                 margelo::nitro::Callback<void(WindowSnapshot)> callback,
                 std::shared_ptr<margelo::nitro::Dispatcher> dispatcher);
  void enqueue(std::uint64_t sequence, const QVariantMap& value, bool initial);
  void stop();
 private:
  void drain();
  bool live() const;
  std::weak_ptr<rnqnx::window::WindowSessionState> source_;
  std::shared_ptr<std::atomic<bool>> handleActive_;
  margelo::nitro::Callback<void(WindowSnapshot)> callback_;
  std::shared_ptr<margelo::nitro::Dispatcher> dispatcher_;
  std::atomic<bool> active_{true};
  std::mutex mutex_;
  std::optional<WindowSnapshot> initial_;
  std::optional<WindowSnapshot> latest_;
  std::uint64_t latestSequence_{0};
  bool initialized_{false};
  bool scheduled_{false};
};
} // namespace rnqnx::nitro::windowapi
