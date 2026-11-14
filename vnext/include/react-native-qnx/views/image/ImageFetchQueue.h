#pragma once

#include <QString>

#include <cstdint>
#include <deque>
#include <functional>

namespace rnqnx::views {

/// expo-image's `priority` prop: when more than one load is waiting, the higher
/// priority starts first.
enum class ImagePriority { Low, Normal, High };

/// Reads "low", "normal" or "high"; anything else is Normal.
ImagePriority parseImagePriority(const QString& text);

/// Starts at most `maxConcurrent` tasks at once and keeps the rest waiting,
/// highest priority first and first come first served inside one priority.
/// A started task holds its slot until release() is called for it. Not
/// thread-safe: use it on the thread that owns the loads.
class ImageFetchQueue final {
 public:
  using Id = std::uint64_t;
  using Start = std::function<void()>;

  explicit ImageFetchQueue(int maxConcurrent);

  /// The queue every ExpoImage view and the ImageLoader share, so `priority`
  /// orders loads across the whole app: at most kSharedConcurrency fetches
  /// at once.
  static ImageFetchQueue& shared();
  static constexpr int kSharedConcurrency = 6;

  /// Runs \a start now when a slot is free, otherwise queues it. \a start may
  /// call back into the queue.
  Id submit(ImagePriority priority, Start start);
  /// Drops a task that has not started. Returns false when \a id is unknown or
  /// already running; a running task is ended with release().
  bool cancel(Id id);
  /// Frees the slot of a finished or abandoned running task and starts the
  /// best waiting one.
  void release();

  int running() const { return running_; }
  int waiting() const;
  int maxConcurrent() const { return max_; }

 private:
  struct Entry {
    Id id;
    Start start;
  };
  void pump();

  int max_;
  int running_{0};
  Id nextId_{1};
  std::deque<Entry> lanes_[3];  // indexed by ImagePriority
  bool pumping_{false};
};

}  // namespace rnqnx::views
