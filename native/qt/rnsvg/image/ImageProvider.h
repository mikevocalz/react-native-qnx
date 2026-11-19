#pragma once

#include "ImageSource.h"

#include <functional>
#include <memory>

namespace rnqnx::views::rnsvg {

// Handle for one load in flight. Destroying it cancels the load.
class ImageRequest {
 public:
  virtual ~ImageRequest() = default;
};

// Where an Image element gets pixels. The Fabric host implements it on top of
// its image cache; tests implement it with a fake.
class ImageProvider {
 public:
  using Completion = std::function<void(ImageLoadResult)>;

  virtual ~ImageProvider() = default;

  // Starts loading `uri`. The contract the loader relies on:
  //  - `completion` runs at most once, on the thread that called load();
  //  - it may run before load() returns, in which case the returned handle may
  //    be null;
  //  - it never runs after the returned handle is destroyed, and never from
  //    inside the handle's destructor;
  //  - destroying a handle after the provider itself is gone is safe and does
  //    nothing (a view tree can outlive the host that loaded its images).
  virtual std::unique_ptr<ImageRequest> load(const QString& uri, Completion completion) = 0;
};

}  // namespace rnqnx::views::rnsvg
