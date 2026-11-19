#include "react-native-qnx/views/image/ImageFetchQueue.h"

#include <QString>

#include <algorithm>
#include <utility>

namespace rnqnx::views {

ImagePriority parseImagePriority(const QString& text) {
  if (text == QLatin1String("low")) return ImagePriority::Low;
  if (text == QLatin1String("high")) return ImagePriority::High;
  return ImagePriority::Normal;
}

ImageFetchQueue::ImageFetchQueue(int maxConcurrent) : max_(std::max(1, maxConcurrent)) {}

ImageFetchQueue& ImageFetchQueue::shared() {
  // Never destroyed: a view torn down during process exit still releases its
  // slot, and must find the queue alive.
  static ImageFetchQueue* const queue = new ImageFetchQueue(kSharedConcurrency);
  return *queue;
}

int ImageFetchQueue::waiting() const {
  int count = 0;
  for (const auto& lane : lanes_) count += static_cast<int>(lane.size());
  return count;
}

ImageFetchQueue::Id ImageFetchQueue::submit(ImagePriority priority, Start start) {
  const Id id = nextId_++;
  lanes_[static_cast<int>(priority)].push_back(Entry{id, std::move(start)});
  pump();
  return id;
}

bool ImageFetchQueue::cancel(Id id) {
  for (auto& lane : lanes_) {
    const auto found = std::find_if(lane.begin(), lane.end(), [id](const Entry& e) { return e.id == id; });
    if (found != lane.end()) {
      lane.erase(found);
      return true;
    }
  }
  return false;
}

void ImageFetchQueue::release() {
  if (running_ > 0) --running_;
  pump();
}

void ImageFetchQueue::pump() {
  // A started task may finish at once and call release(); the loop below
  // already refills the slot, so the nested call returns.
  if (pumping_) return;
  pumping_ = true;
  while (running_ < max_) {
    Start next;
    for (int lane = 2; lane >= 0 && !next; --lane) {
      if (lanes_[lane].empty()) continue;
      next = std::move(lanes_[lane].front().start);
      lanes_[lane].pop_front();
    }
    if (!next) break;
    ++running_;
    next();
  }
  pumping_ = false;
}

}  // namespace rnqnx::views
