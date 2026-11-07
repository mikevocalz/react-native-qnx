#include "QtNitroDispatcher.hpp"

#include <stdexcept>
#include <utility>

namespace rnqnx::nitro {

QtNitroDispatcher::QtNitroDispatcher(
    std::shared_ptr<runtime::QtRuntimeTaskQueue> queue)
    : queue_(std::move(queue)) {
  if (!queue_) {
    throw std::invalid_argument("Qt Nitro dispatcher requires a host task queue");
  }
}

void QtNitroDispatcher::runSync(std::function<void()>&& function) {
  queue_->runSync(std::move(function));
}

void QtNitroDispatcher::runAsync(std::function<void()>&& function) {
  queue_->post(std::move(function));
}

bool QtNitroDispatcher::isOnDispatcher() {
  return queue_->isOnOwnerThread();
}

void QtNitroDispatcher::enterJS(facebook::jsi::Runtime&) {
  queue_->enterJS();
}

void QtNitroDispatcher::leaveJS(facebook::jsi::Runtime&) noexcept {
  queue_->leaveJS();
}

}  // namespace rnqnx::nitro
