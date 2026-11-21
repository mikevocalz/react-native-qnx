#pragma once

#include <QString>

#include <cstdint>
#include <functional>
#include <map>
#include <mutex>

namespace rnqnx::views {

/// One decoded picture reaching the screen or a caller: from the ExpoImage
/// view, `Image.loadAsync` or `readFromCacheAsync`.
struct ImageLoadedEvent {
  QString url;
  int width{0};
  int height{0};
};

/// Fans an ImageLoadedEvent out to whoever listens: the host's installed binding
/// forwards it to expo-image's `imageLoaded` module event (what expo-observe
/// subscribes to). Thread-safe: with the JS thread on (ADR-0021) subscribe runs
/// there while notify runs on the GUI thread or a worker. Listeners run
/// outside the lock.
class ImageLoadedNotifier final {
 public:
  using Token = std::uint64_t;
  using Listener = std::function<void(const ImageLoadedEvent&)>;

  static ImageLoadedNotifier& shared();

  Token subscribe(Listener listener);
  void unsubscribe(Token token);
  void notify(const ImageLoadedEvent& event);

 private:
  std::mutex mutex_;
  std::map<Token, Listener> listeners_;
  Token next_{1};
};

}  // namespace rnqnx::views
