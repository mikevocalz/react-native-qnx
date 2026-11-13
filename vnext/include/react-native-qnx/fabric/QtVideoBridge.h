#pragma once

#include <QObject>
#include <memory>
#include <vector>
#include <optional>
#include <string>

class QQuickItem;

namespace facebook::react {
class ComponentDescriptorProviderRegistry;
struct ShadowView;
class QnxQtVideoProps;
class QnxQtVideoEventEmitter;
}
namespace rnqnx::views {
class QtVideoItem;
}

namespace rnqnx::fabric {

/// The Fabric component this feature mounts (for the extra-component table).
std::vector<std::string> videoComponentNames();
void addVideoComponentDescriptors(facebook::react::ComponentDescriptorProviderRegistry& registry);
/// Applies props to the QtVideoItem and creates its bridge on first use; the
/// bridge is a child of the item, so it is deleted with it.
bool applyVideoProps(QQuickItem* item, const facebook::react::ShadowView& view);

/// Owned by the mounted item. Applies Fabric props and forwards the item's
/// playback signals to the current event emitter. Prop values are only
/// written when they change, so an unrelated commit never restarts playback.
class QtVideoBridge final : public QObject {
 public:
  explicit QtVideoBridge(views::QtVideoItem& item);
  void apply(const facebook::react::QnxQtVideoProps& props,
             std::shared_ptr<const facebook::react::QnxQtVideoEventEmitter> emitter);

 private:
  views::QtVideoItem& item_;
  std::shared_ptr<const facebook::react::QnxQtVideoEventEmitter> emitter_;
  struct Applied {
    std::string source, poster, audioDevice;
    bool paused, muted, repeat, controls, fullscreen;
    double volume, rate;
    int resizeMode;
  };
  std::optional<Applied> last_;
};

} // namespace rnqnx::fabric
