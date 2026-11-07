#include "Probe.h"

#include <pthread.h>

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <stdexcept>
#include <thread>

namespace rnqnx::systems {
namespace {
struct LocalValue {
  int value;
  std::atomic<int>* destructors;
};

void destroyLocalValue(void* value) noexcept {
  // Context storage outlives the joined worker; TLS does not own this pointer.
  static_cast<LocalValue*>(value)->destructors->fetch_add(1, std::memory_order_relaxed);
}

class Key final {
 public:
  Key() {
    if (pthread_key_create(&value_, destroyLocalValue) != 0) {
      throw std::runtime_error("tls-key-create");
    }
  }
  ~Key() {
    if (open_) pthread_key_delete(value_);
  }
  void close() {
    if (pthread_key_delete(value_) != 0) throw std::runtime_error("tls-key-delete");
    open_ = false;
  }
  Key(const Key&) = delete;
  Key& operator=(const Key&) = delete;
  pthread_key_t get() const { return value_; }

 private:
  pthread_key_t value_{};
  bool open_ = true;
};

struct Gate {
  std::mutex mutex;
  std::condition_variable changed;
  unsigned ready = 0;
  bool released = false;
};

struct Worker {
  LocalValue local;
  pthread_t self{};
  int setResult = 0;
  bool isolated = false;
};

class JoinedWorkers final {
 public:
  explicit JoinedWorkers(Gate& gate) : gate_(gate) {}
  ~JoinedWorkers() { releaseAndJoin(); }
  JoinedWorkers(const JoinedWorkers&) = delete;
  JoinedWorkers& operator=(const JoinedWorkers&) = delete;

  void start(unsigned index, Worker& worker, pthread_key_t key, bool injectMismatch) {
    threads_.at(index) = std::thread([this, &worker, key, injectMismatch] {
      worker.self = pthread_self();
      worker.setResult = pthread_setspecific(key, &worker.local);
      {
        std::unique_lock lock(gate_.mutex);
        ++gate_.ready;
        gate_.changed.notify_all();
        gate_.changed.wait(lock, [this] { return gate_.released; });
      }
      if (injectMismatch && pthread_setspecific(key, nullptr) != 0) worker.setResult = -1;
      auto* observed = static_cast<LocalValue*>(pthread_getspecific(key));
      worker.isolated = observed == &worker.local && observed->value == worker.local.value;
    });
  }

  pthread_t id(unsigned index) { return threads_.at(index).native_handle(); }

  void releaseAndJoin() {
    {
      std::lock_guard lock(gate_.mutex);
      gate_.released = true;
    }
    gate_.changed.notify_all();
    for (auto& thread : threads_) {
      if (thread.joinable()) thread.join();
    }
  }

 private:
  Gate& gate_;
  std::array<std::thread, 2> threads_;
};
} // namespace

void probeThreads(bool injectTlsMismatch) {
  Key key;
  std::atomic<int> destructors{0};
  Gate gate;
  std::array<Worker, 2> workers{{{{17, &destructors}}, {{29, &destructors}}}};
  // Construct last: all contexts, the gate and TLS key outlive every join.
  JoinedWorkers threads(gate);
  for (unsigned i = 0; i < workers.size(); ++i) threads.start(i, workers[i], key.get(), injectTlsMismatch && i == 0);
  {
    std::unique_lock lock(gate.mutex);
    if (!gate.changed.wait_for(lock, std::chrono::seconds(3), [&] { return gate.ready == 2; })) {
      throw std::runtime_error("thread-start-deadline");
    }
    for (unsigned i = 0; i < workers.size(); ++i) {
      if (!pthread_equal(threads.id(i), workers[i].self)) {
        throw std::runtime_error("thread-identity");
      }
    }
    if (pthread_equal(threads.id(0), threads.id(1)) || pthread_getspecific(key.get()) != nullptr) {
      throw std::runtime_error("tls-main-isolation");
    }
  }
  threads.releaseAndJoin();
  for (const auto& worker : workers) {
    if (worker.setResult != 0 || !worker.isolated) throw std::runtime_error("tls-worker-isolation");
  }
  if (destructors.load(std::memory_order_relaxed) != 2) throw std::runtime_error("tls-destructor-count");
  key.close();
}
} // namespace rnqnx::systems
