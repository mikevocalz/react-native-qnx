#include "react-native-qnx/fabric/QtSvgBridge.h"

#include <QtSvgItem.h>
#include <react/renderer/components/RnqnxQtSvg/EventEmitters.h>
#include <react/renderer/components/RnqnxQtSvg/Props.h>

#include <utility>
#include <algorithm>

namespace rnqnx::fabric {

QtSvgBridge::QtSvgBridge(views::QtSvgItem& item) : QObject(&item), item_(item) {
  QObject::connect(&item, &views::QtSvgItem::documentLoaded, this,
      [this](const QSizeF& size, const QRectF& box) {
        const auto emitter = emitter_;
        if (!emitter) return;
        facebook::react::QnxSvgDocumentEventEmitter::OnDocumentLoad event{};
        event.width = size.width();
        event.height = size.height();
        event.viewBoxX = box.x();
        event.viewBoxY = box.y();
        event.viewBoxWidth = box.width();
        event.viewBoxHeight = box.height();
        emitter->onDocumentLoad(event);
      });
  QObject::connect(&item, &views::QtSvgItem::documentFailed, this,
      [this](const QString& error) {
        const auto emitter = emitter_;
        if (emitter) emitter->onDocumentError({error.toStdString()});
      });
}

void QtSvgBridge::apply(
    const facebook::react::QnxSvgDocumentProps& props,
    std::shared_ptr<const facebook::react::QnxSvgDocumentEventEmitter> emitter) {
  emitter_ = std::move(emitter);
  using Mode = facebook::react::QnxSvgDocumentResizeMode;
  switch (props.resizeMode) {
    case Mode::Contain: item_.setResizeMode(views::SvgResizeMode::Contain); break;
    case Mode::Cover: item_.setResizeMode(views::SvgResizeMode::Cover); break;
    case Mode::Stretch: item_.setResizeMode(views::SvgResizeMode::Stretch); break;
  }
  // One byte above the bound is sufficient for the native size-first rejection.
  // The item owns accepted bytes and suppresses unchanged bounded sources. Do
  // not retain another unbounded copy of a rejected Fabric string here.
  const auto length = std::min(props.xml.size(),
      static_cast<std::size_t>(views::kSvgMaxDocumentBytes) + 1);
  item_.setDocument(QByteArrayView(props.xml.data(), static_cast<qsizetype>(length)));
}

} // namespace rnqnx::fabric
