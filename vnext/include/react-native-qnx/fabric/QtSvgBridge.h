#pragma once

#include <QObject>
#include <memory>

namespace facebook::react {
class QnxSvgDocumentProps;
class QnxSvgDocumentEventEmitter;
}
namespace rnqnx::views {
class QtSvgItem;
}

namespace rnqnx::fabric {

/// Owned by the mounted item. Event-emitter replacement follows Fabric updates;
/// unchanged XML does not decode or re-emit load events on unrelated commits.
class QtSvgBridge final : public QObject {
 public:
  explicit QtSvgBridge(views::QtSvgItem& item);
  void apply(const facebook::react::QnxSvgDocumentProps& props,
             std::shared_ptr<const facebook::react::QnxSvgDocumentEventEmitter> emitter);

 private:
  views::QtSvgItem& item_;
  std::shared_ptr<const facebook::react::QnxSvgDocumentEventEmitter> emitter_;
};

} // namespace rnqnx::fabric
