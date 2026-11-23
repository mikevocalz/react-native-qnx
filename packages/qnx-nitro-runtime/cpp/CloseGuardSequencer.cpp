#include "CloseGuardSequencer.hpp"

#include <algorithm>
#include <utility>

namespace rnqnx::nitro {

void CloseGuardSequencer::add(std::uint64_t id, Invoke invoke) {
  guards_.push_back({id, std::move(invoke)});
}

void CloseGuardSequencer::remove(std::uint64_t id) {
  const bool wasAsked = active_ && active_->asking == id;
  guards_.erase(std::remove_if(guards_.begin(), guards_.end(),
                               [id](const Guard& guard) { return guard.id == id; }),
                guards_.end());
  if (wasAsked) {
    active_->asking = 0;
    askNext();
  }
}

void CloseGuardSequencer::clear() {
  guards_.clear();
  active_.reset();
}

void CloseGuardSequencer::begin(const window::CloseRequestInfo& request) {
  active_ = Active{request, request.guardIds, 0};
  askNext();
}

void CloseGuardSequencer::askNext() {
  while (active_ && active_->next < active_->remaining.size()) {
    const auto id = active_->remaining[active_->next++];
    const auto found = std::find_if(guards_.begin(), guards_.end(),
                                    [id](const Guard& guard) { return guard.id == id; });
    if (found == guards_.end()) continue;  // removed since the request
    // Copy: the guard may remove itself.
    auto invoke = found->invoke;
    active_->asking = id;
    const auto request = active_->request;
    invoke(request);
    return;
  }
  if (!active_) return;
  const auto request = active_->request;
  active_.reset();
  finish_(request.requestId, request.epoch, true);
}

void CloseGuardSequencer::resolve(std::uint64_t requestId, std::uint64_t guardId, bool allow) {
  if (!active_ || active_->request.requestId != requestId || active_->asking != guardId) return;
  active_->asking = 0;
  if (!allow && active_->request.cancellable) {
    const auto request = active_->request;
    active_.reset();
    finish_(request.requestId, request.epoch, false);
    return;
  }
  askNext();
}

}  // namespace rnqnx::nitro
