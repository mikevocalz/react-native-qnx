#include "react-native-qnx/views/image/ImageRefRegistry.h"

#include <QMutexLocker>

#include <utility>

namespace rnqnx::views {

ImageRefRegistry& ImageRefRegistry::shared() {
  static ImageRefRegistry registry;
  return registry;
}

std::uint32_t ImageRefRegistry::add(ImageRefEntry entry) {
  QMutexLocker lock(&mutex_);
  const std::uint32_t id = next_++;
  entries_.emplace(id, std::move(entry));
  return id;
}

ImageRefEntry ImageRefRegistry::find(std::uint32_t id) const {
  QMutexLocker lock(&mutex_);
  const auto found = entries_.find(id);
  return found == entries_.end() ? ImageRefEntry{} : found->second;
}

bool ImageRefRegistry::release(std::uint32_t id) {
  QMutexLocker lock(&mutex_);
  return entries_.erase(id) > 0;
}

int ImageRefRegistry::size() const {
  QMutexLocker lock(&mutex_);
  return static_cast<int>(entries_.size());
}

}  // namespace rnqnx::views
