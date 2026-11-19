#include "react-native-qnx/fabric/QtRnsvgImageBridge.h"

#include "QtRnsvgBridgeInternal.h"
#include "react-native-qnx/fabric/QtRnsvgImageProvider.h"

#include <RnsvgItems.h>
#include <image/ImageBinding.h>
#include <image/ImageElementProps.h>

#include <folly/dynamic.h>
#include <react/renderer/components/rnsvg/Props.h>
#include <react/renderer/core/EventEmitter.h>
#include <react/renderer/mounting/ShadowView.h>

#include <QDebug>

namespace rnqnx::fabric::rnsvg_bridge {

bool convertImageProps(const rn::Props& props, ElementProps& e, const ElementProps& previous) {
  return convertAs<rn::RNSVGImageProps>(
      props, e, previous, [](const rn::RNSVGImageProps& p, ElementProps& e, const ElementProps& previous) {
        renderable(e, p);
        e.x = toLength(p.x);
        e.y = toLength(p.y);
        e.width = toLength(p.width);
        e.height = toLength(p.height);
        e.imageUri = QString::fromStdString(p.src.uri);
        e.imageAlign = QString::fromStdString(p.align.empty() ? std::string("xMidYMid") : p.align);
        e.imageMeetOrSlice = p.meetOrSlice;
        carryImageLoadState(e, previous);
        // Request headers are part of a source in React Native. The decoded-image
        // cache keeps header-bearing responses out of its URI-keyed store (they
        // may be private), so there is nothing to read the image back from.
        // Show a failure rather than load the URI without them.
        if (!e.imageUri.isEmpty() && !p.src.headers.empty() && e.imageUri != previous.imageUri) {
          qWarning().noquote() << "react-native-svg Image: request headers are not supported, not loading" << e.imageUri;
        }
        if (!p.src.headers.empty() && !e.imageUri.isEmpty()) e.imageState = toInt(ImageState::Failed);
      });
}

void attachImageLoading(views::RnsvgElementItem* element, const rn::ShadowView& view) {
  if (element == nullptr) return;
  auto& binding = RnsvgImageBinding::forElement(*element, QtRnsvgImageProvider::shared());

  // The latest emitter, replaced on every commit.
  std::shared_ptr<const rn::EventEmitter> emitter = view.eventEmitter;
  binding.setLoadedCallback([emitter = std::move(emitter)](const ImageLoadedEvent& loaded) {
    if (!emitter) return;
    folly::dynamic source = folly::dynamic::object("width", loaded.size.width())("height", loaded.size.height())(
        "uri", loaded.uri.toStdString());
    folly::dynamic payload = folly::dynamic::object("source", std::move(source));
    emitter->dispatchEvent("load", std::move(payload));
  });

  // A source with headers is shown as failed by convertImageProps and never
  // loaded. Clearing the loader also cancels any request left from a previous
  // href, so it cannot land on this one.
  const auto* props = dynamic_cast<const rn::RNSVGImageProps*>(view.props.get());
  if (props != nullptr && !props->src.headers.empty()) {
    binding.setSource(QString());
    return;
  }
  binding.setSource(element->props().imageUri);
}

}  // namespace rnqnx::fabric::rnsvg_bridge
