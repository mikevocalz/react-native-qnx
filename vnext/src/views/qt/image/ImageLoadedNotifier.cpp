#include "react-native-qnx/views/image/ImageLoadedNotifier.h"

#include <utility>
#include <vector>

namespace rnqnx::views {

ImageLoadedNotifier& ImageLoadedNotifier::shared() {
  static ImageLoadedNotifier notifier;
  return notifier;
}

ImageLoadedNotifier::Token ImageLoadedNotifier::subscribe(Listener listener) {
  const Token token = next_++;
  listeners_.emplace(token, std::move(listener));
  return token;
}

void ImageLoadedNotifier::unsubscribe(Token token) { listeners_.erase(token); }

void ImageLoadedNotifier::notify(const ImageLoadedEvent& event) {
  // A listener may unsubscribe while it runs; call copies.
  std::vector<Listener> snapshot;
  snapshot.reserve(listeners_.size());
  for (const auto& entry : listeners_) snapshot.push_back(entry.second);
  for (const auto& listener : snapshot) listener(event);
}

}  // namespace rnqnx::views
