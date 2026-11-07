#pragma once

#include "ForgeSerialQueue.hpp"

#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace nitroforge::native {
class ForgeRustSession;
}

namespace rnqnx::forge {

// Internal owner of one native worker. No JS runtime, Qt object, callback, or
// borrowed argument may be captured by a submitted document operation.
class ForgeWorker final : public std::enable_shared_from_this<ForgeWorker> {
 public:
  ForgeWorker();
  ~ForgeWorker();
  ForgeWorker(const ForgeWorker&) = delete;
  ForgeWorker& operator=(const ForgeWorker&) = delete;

  std::shared_ptr<nitroforge::native::ForgeSerialQueue> createQueue();
  void submit(const std::shared_ptr<nitroforge::native::ForgeSerialQueue>& queue,
              nitroforge::native::ForgeSerialQueue::Task task);
  // Called only on the worker after a session opens successfully.
  void trackSession(const std::shared_ptr<nitroforge::native::ForgeRustSession>& session);
  // Owner-thread teardown: reject new work, drain accepted work, close, join.
  void shutdown();

 private:
  void enqueue(nitroforge::native::ForgeSerialQueue::Task task);
  void run();

  // Admission serializes submit against shutdown and scheduler failure. The
  // pending mutex only protects the worker mailbox; tasks run with neither held.
  std::mutex _admissionMutex;
  bool _accepting = true;
  std::mutex _pendingMutex;
  std::condition_variable _ready;
  std::deque<nitroforge::native::ForgeSerialQueue::Task> _pending;
  bool _stopping = false;
  // Keep live Rust handles on the worker even if the caller drops a document
  // while shutdown is rejecting its final close submission.
  std::vector<std::shared_ptr<nitroforge::native::ForgeRustSession>> _sessions;
  std::once_flag _shutdownOnce;
  std::thread _thread;
};

} // namespace rnqnx::forge
