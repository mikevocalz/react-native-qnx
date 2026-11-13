#pragma once

#include <QString>

#include <cstdint>
#include <functional>
#include <map>

namespace rnqnx::views {

/// One decoded picture reaching the screen or a caller: from the ExpoImage
/// view, `Image.loadAsync` or `readFromCacheAsync`.
struct ImageLoadedEvent {
  QString url;
  int width{0};
  int height{0};
};

/// Fans an ImageLoadedEvent out to whoever listens: the ImageLoader module
/// forwards it to expo-image's `imageLoaded` module event (what expo-observe
/// subscribes to). Single-threaded: subscribe and notify on the Qt/JS thread.
class ImageLoadedNotifier final {
 public:
  using Token = std::uint64_t;
  using Listener = std::function<void(const ImageLoadedEvent&)>;

  static ImageLoadedNotifier& shared();

  Token subscribe(Listener listener);
  void unsubscribe(Token token);
  void notify(const ImageLoadedEvent& event);

 private:
  std::map<Token, Listener> listeners_;
  Token next_{1};
};

}  // namespace rnqnx::views
