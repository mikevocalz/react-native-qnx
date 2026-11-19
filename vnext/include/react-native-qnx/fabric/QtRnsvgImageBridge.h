#pragma once

// The Fabric half of the react-native-svg <Image> element: its props, its
// load through the host's image loader, and its onLoad event.
//
// QtRnsvgBridge calls convertImageProps() from its element table, in place of
// the generic converter, and attachImageLoading() right after it has applied
// the converted props to the element. The loading itself lives in
// native/qt/rnsvg/image (ImageLoader, RnsvgImageBinding) and the provider in
// QtRnsvgImageProvider.h; this file only adapts React types to them.

namespace facebook::react {
class Props;
struct ShadowView;
}  // namespace facebook::react

namespace rnqnx::views {
class RnsvgElementItem;
}

namespace rnqnx::views::rnsvg {
struct ElementProps;
}

namespace rnqnx::fabric::rnsvg_bridge {

// Converts RNSVGImageProps into `out`: the renderable props, the box, the
// href (`src.uri`), preserveAspectRatio, and the load state carried over from
// `previous` when the href is unchanged. Returns false if `props` is not
// RNSVGImageProps. Same signature as the element table's Converter.
bool convertImageProps(const facebook::react::Props& props, views::rnsvg::ElementProps& out,
                       const views::rnsvg::ElementProps& previous);

// Starts (or keeps) the load for the element's current `imageUri` and wires
// onLoad to the view's event emitter. Call after element->setProps() on every
// commit of an RNSVGImage view. Qt GUI thread only.
//
// onLoad is dispatched as `load` with `{source: {width, height, uri}}`, the
// payload and event name RNSVGImageEventEmitter::onLoad sends, through the
// base EventEmitter because the package's shadow node creates a plain
// ViewEventEmitter. It goes out from a queued call on the GUI thread, the same
// thread ExpoImage and the other Qt components emit from. react-native-svg's
// Image declares no error event, so a failed load emits nothing.
void attachImageLoading(views::RnsvgElementItem* element, const facebook::react::ShadowView& view);

}  // namespace rnqnx::fabric::rnsvg_bridge
