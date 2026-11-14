#pragma once

#include "GestureHandlerCore.h"
#include <algorithm>

namespace rnqnx::gesture {

// Fabric mounts detectors before passive effects create/configure handlers.
// Retain both sides so attachment works in either order. No native resources
// are owned here; Registry and its callbacks must outlive these bindings.
class DetectorBindings {
 public:
  explicit DetectorBindings(Registry& registry) : registry_(registry) {}

  void create(const std::string& name, int tag, const Config& config) {
    registry_.create(name, tag, config);
    configure(tag, config);
  }

  void configure(int tag, const Config& config) {
    registry_.setConfig(tag, config);
    const auto reanimated = config.find("dispatchesReanimatedEvents");
    actions_[tag] = reanimated != config.end() && reanimated->second != 0 ? 1 : 4;
    attach(tag);
  }

  void observe(int viewTag, const std::vector<int>& tags) {
    for (auto it = views_.begin(); it != views_.end();) {
      if (it->second == viewTag && std::find(tags.begin(), tags.end(), it->first) == tags.end()) {
        registry_.detach(it->first);
        it = views_.erase(it);
      } else {
        ++it;
      }
    }
    for (int tag : tags) {
      views_[tag] = viewTag;
      attach(tag);
    }
  }

  void removeView(int viewTag) { observe(viewTag, {}); }

  void drop(int tag) {
    registry_.drop(tag);
    actions_.erase(tag);
    views_.erase(tag);
  }

 private:
  void attach(int tag) {
    const auto view = views_.find(tag);
    const auto action = actions_.find(tag);
    if (view != views_.end() && action != actions_.end()) {
      registry_.attach(tag, view->second, action->second);
    }
  }
  Registry& registry_;
  std::map<int, int> views_;
  std::map<int, int> actions_;
};

}  // namespace rnqnx::gesture
