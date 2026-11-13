#pragma once

#include <QObject>
#include <memory>
#include <optional>
#include <string>

namespace facebook::react {
class QnxQtVideoProps;
class QnxQtVideoEventEmitter;
}
namespace rnqnx::views {
class QtVideoItem;
}

namespace rnqnx::fabric {

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
