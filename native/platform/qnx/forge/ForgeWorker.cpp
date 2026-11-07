#include "ForgeWorker.hpp"

#include "ForgeRustSession.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace rnqnx::forge {

ForgeWorker::ForgeWorker() : _thread([this] { run(); }) {}

ForgeWorker::~ForgeWorker() {
  shutdown();
}

std::shared_ptr<nitroforge::native::ForgeSerialQueue> ForgeWorker::createQueue() {
  return nitroforge::native::ForgeSerialQueue::create(
      [weak = weak_from_this()](auto task) {
        const auto worker = weak.lock();
        if (!worker) {
          throw std::runtime_error("QNX Forge runtime is shut down");
        }
        worker->enqueue(std::move(task));
      });
}

void ForgeWorker::submit(
    const std::shared_ptr<nitroforge::native::ForgeSerialQueue>& queue,
    nitroforge::native::ForgeSerialQueue::Task task) {
  std::lock_guard lock(_admissionMutex);
  if (!_accepting) {
    throw std::runtime_error("QNX Forge runtime is shut down");
  }
  // Keep competing submissions out until scheduling either succeeds or fails.
  // This avoids the common queue's concurrent scheduler-failure edge case.
  queue->submit(std::move(task));
}

void ForgeWorker::enqueue(nitroforge::native::ForgeSerialQueue::Task task) {
  {
    std::lock_guard lock(_pendingMutex);
    if (_stopping) {
      throw std::runtime_error("QNX Forge runtime is shut down");
    }
    _pending.push_back(std::move(task));
  }
  _ready.notify_one();
}

void ForgeWorker::trackSession(
    const std::shared_ptr<nitroforge::native::ForgeRustSession>& session) {
  _sessions.emplace_back(session);
}

void ForgeWorker::shutdown() {
  std::call_once(_shutdownOnce, [this] {
    {
      std::lock_guard admission(_admissionMutex);
      _accepting = false;
      std::lock_guard pending(_pendingMutex);
      _stopping = true;
    }
    _ready.notify_one();
    _thread.join();
  });
}

void ForgeWorker::run() {
  for (;;) {
    nitroforge::native::ForgeSerialQueue::Task task;
    {
      std::unique_lock lock(_pendingMutex);
      _ready.wait(lock, [this] { return _stopping || !_pending.empty(); });
      if (_pending.empty()) {
        break;
      }
      task = std::move(_pending.front());
      _pending.pop_front();
    }
    // Every operation is a packaged_task: engine exceptions are delivered to
    // its future, never thrown through ForgeSerialQueue or this worker loop.
    task();
    std::erase_if(_sessions, [](const auto& session) { return !session->isOpen(); });
  }
  for (const auto& session : _sessions) {
    session->close();
  }
  _sessions.clear();
}

} // namespace rnqnx::forge
