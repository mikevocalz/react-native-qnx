#pragma once

#include <QImage>
#include <QMutex>
#include <QString>

#include <cstdint>
#include <unordered_map>

namespace rnqnx::views {

/// A decoded picture that JavaScript holds as an expo-image `ImageRef`.
struct ImageRefEntry {
  QImage image;
  QString mediaType;
  bool animated{false};
};

/// The pictures behind `ImageRef` objects. `Image.loadAsync` and
/// `readFromCacheAsync` add one, `ImageRef.release()` removes it, and the
/// ExpoImage view reads it when `source` is an ImageRef. Ids are never reused
/// in a process. All members are thread-safe.
class ImageRefRegistry final {
 public:
  static ImageRefRegistry& shared();

  /// Stores \a entry and returns its id (never 0).
  std::uint32_t add(ImageRefEntry entry);
  /// The entry for \a id; a null image when \a id is unknown or released.
  ImageRefEntry find(std::uint32_t id) const;
  /// Returns false when \a id is unknown.
  bool release(std::uint32_t id);
  int size() const;

 private:
  mutable QMutex mutex_;
  std::unordered_map<std::uint32_t, ImageRefEntry> entries_;
  std::uint32_t next_{1};
};

}  // namespace rnqnx::views
