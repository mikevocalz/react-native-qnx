#pragma once

// Runs the registered close guards one at a time, in registration order, for
// one close request. JavaScript thread only. No Nitro, JSI or Qt: the guard
// callbacks are std::function, so the order, cancel and teardown rules test
// without a runtime (docs/nitro/API_CONTRACTS.md, "Closing, guards and who can
// be asked").

#include "WindowCloseRequest.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

namespace rnqnx::nitro {

class CloseGuardSequencer final {
 public:
  using Invoke = std::function<void(const window::CloseRequestInfo&)>;
  // Reports the outcome of a request: true closes, false keeps the window.
  using Finish = std::function<void(std::uint64_t requestId, std::uint64_t epoch, bool allow)>;

  explicit CloseGuardSequencer(Finish finish) : finish_(std::move(finish)) {}

  void add(std::uint64_t id, Invoke invoke);
  void remove(std::uint64_t id);
  // Drops every guard and any request in flight without reporting it. The
  // GUI side finishes a pending close on its own (runtime teardown).
  void clear();
  std::size_t size() const { return guards_.size(); }

  // Starts a request. The guards asked are those of `request.guardIds` that
  // are still registered. With none, the request finishes allowed.
  void begin(const window::CloseRequestInfo& request);
  // Guard `guardId` answered. An answer from a guard that is not the one being
  // asked (it was removed, or answered twice) is dropped. Removing the guard
  // being asked counts as an allow.
  // One guard answered. `allow` is ignored for a request that is not
  // cancellable. Stale or unknown request ids are dropped.
  void resolve(std::uint64_t requestId, std::uint64_t guardId, bool allow);

 private:
  struct Guard {
    std::uint64_t id;
    Invoke invoke;
  };
  void askNext();

  Finish finish_;
  std::vector<Guard> guards_;
  struct Active {
    window::CloseRequestInfo request;
    std::vector<std::uint64_t> remaining;  // ids still to ask, in order
    std::size_t next{0};
    std::uint64_t asking{0};  // the guard whose answer is awaited
  };
  std::optional<Active> active_;
};

}  // namespace rnqnx::nitro
