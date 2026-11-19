#pragma once

#include "react-native-qnx/views/image/ImageFetchQueue.h"

#include <QFutureWatcher>
#include <QObject>
#include <QPromise>
#include <QString>
#include <QThreadPool>

#include <atomic>
#include <cstdint>
#include <exception>
#include <map>
#include <memory>
#include <type_traits>
#include <utility>
#include <variant>

namespace rnqnx::views {

struct ImageTaskError {
  QString message;
};

/// Runs image computation on a bounded shared worker queue. Work captures
/// plain native values only; completion and its destruction stay on this
/// QObject's owner thread. Destroying the queue abandons queued operations
/// and suppresses completion of work already running, without waiting for it.
/// A supplied admission queue must outlive native completion delivery; a
/// supplied thread pool must outlive the work submitted to it.
class ImageTaskQueue final : public QObject {
 public:
  explicit ImageTaskQueue(ImageFetchQueue* queue = nullptr, QThreadPool* pool = nullptr)
      : queue_(queue ? queue : &ImageFetchQueue::shared()),
        pool_(pool ? pool : QThreadPool::globalInstance()) {}

  ~ImageTaskQueue() override {
    // Remove waiting starts first, before releasing any running slot.
    for (auto it = pending_.begin(); it != pending_.end();) {
      it->second->cancelled->store(true);
      if (!it->second->started) {
        queue_->cancel(it->second->queued);
        it = pending_.erase(it);
      } else {
        ++it;
      }
    }
    pending_.clear();
    // Running workers retain their admission slots until they finish. Their
    // native-only watchers outlive this owner; QObject disconnects and drops
    // the owner completion captures below during destruction.
  }

  template <typename Work, typename Done>
  void run(Work work, Done done) {
    using Value = std::invoke_result_t<Work>;
    using Result = std::variant<Value, ImageTaskError>;
    static_assert(!std::is_void_v<Value>);
    const auto id = nextId_++;
    auto pending = std::make_shared<Pending>();
    pending_.emplace(id, pending);
    const auto queued = queue_->submit(ImagePriority::Normal,
        [this, id, pending, work = std::move(work), done = std::move(done)]() mutable {
      pending->started = true;
      auto promise = std::make_shared<QPromise<Result>>();
      promise->start();
      auto* watcher = new QFutureWatcher<Result>();
      QObject::connect(watcher, &QFutureWatcher<Result>::finished, this,
          [this, id, watcher, done = std::move(done)]() mutable {
        Result result = watcher->result();
        pending_.erase(id);
        // Drop owner/JSI captures before native cleanup releases admission:
        // starting the next task can synchronously tear down this owner.
        auto complete = std::move(done);
        complete(std::move(result));
      });
      QObject::connect(watcher, &QFutureWatcher<Result>::finished, watcher,
          [watcher, queue = queue_]() {
        queue->release();
        watcher->deleteLater();
      });
      watcher->setFuture(promise->future());
      // Do not copy owner callbacks or QObject/JSI state to a worker.
      pool_->start([promise, cancelled = pending->cancelled, work = std::move(work)]() mutable {
        if (!cancelled->load()) {
          try {
            promise->addResult(Result(std::in_place_index<0>, work()));
          } catch (const std::exception& error) {
            promise->addResult(Result(std::in_place_index<1>, ImageTaskError{QString::fromUtf8(error.what())}));
          } catch (...) {
            promise->addResult(Result(std::in_place_index<1>, ImageTaskError{QStringLiteral("Image operation failed")}));
          }
        }
        promise->finish();
      });
    });
    if (!pending->started) pending->queued = queued;
  }

 private:
  struct Pending {
    ImageFetchQueue::Id queued{0};
    bool started{false};
    std::shared_ptr<std::atomic_bool> cancelled{std::make_shared<std::atomic_bool>(false)};
  };
  ImageFetchQueue* queue_;
  QThreadPool* pool_;
  std::uint64_t nextId_{1};
  std::map<std::uint64_t, std::shared_ptr<Pending>> pending_;
};

}  // namespace rnqnx::views
