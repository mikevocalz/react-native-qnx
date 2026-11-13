// Fabric half of the ExpoImage view: copies the generated props onto an
// ExpoImageItem and routes the item's signals to the Fabric event emitter.
// The item itself (loading, caching, painting) is views/image/ExpoImageItem.
#include "react-native-qnx/fabric/QtNativeUiBridge.h"

#include "react-native-qnx/views/image/ExpoImageItem.h"

#include <folly/dynamic.h>
#include <react/renderer/componentregistry/ComponentDescriptorProviderRegistry.h>
#include <react/renderer/components/RnqnxExpoImage/ComponentDescriptors.h>
#include <react/renderer/components/RnqnxExpoImage/EventEmitters.h>
#include <react/renderer/components/RnqnxExpoImage/Props.h>
#include <react/renderer/mounting/ShadowView.h>

#include <QColor>
#include <QMarginsF>
#include <QObject>
#include <QString>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>

#include <algorithm>
#include <memory>

namespace rnqnx::fabric {
namespace {

namespace rn = facebook::react;
using views::ExpoImageItem;

/// A JSON-shaped value as the QVariant tree the item reads (maps, lists,
/// strings, numbers, booleans; null becomes an invalid QVariant).
QVariant toVariant(const folly::dynamic& value) {
  switch (value.type()) {
    case folly::dynamic::OBJECT: {
      QVariantMap map;
      for (const auto& [key, entry] : value.items()) {
        map.insert(QString::fromStdString(key.asString()), toVariant(entry));
      }
      return map;
    }
    case folly::dynamic::ARRAY: {
      QVariantList list;
      for (const auto& entry : value) list.append(toVariant(entry));
      return list;
    }
    case folly::dynamic::STRING:
      return QString::fromStdString(value.getString());
    case folly::dynamic::INT64:
      return static_cast<qlonglong>(value.getInt());
    case folly::dynamic::DOUBLE:
      return value.getDouble();
    case folly::dynamic::BOOL:
      return value.getBool();
    default:
      return {};
  }
}

QVariantList toList(const folly::dynamic& value) {
  const QVariant converted = toVariant(value);
  if (converted.typeId() == QMetaType::QVariantList) return converted.toList();
  if (converted.typeId() == QMetaType::QVariantMap) return {converted};
  return {};
}

/// expo-image's transition: a number of milliseconds, or { duration, timing,
/// effect }. A bare number is a cross-dissolve of that length.
QVariantMap toTransition(const folly::dynamic& transition) {
  if (transition.isNumber()) {
    return {{QStringLiteral("duration"), std::max(0.0, transition.asDouble())}};
  }
  const QVariant converted = toVariant(transition);
  QVariantMap map = converted.toMap();
  if (map.value(QStringLiteral("effect")).toString() == QLatin1String("none")) {
    map.insert(QStringLiteral("duration"), 0);
  }
  return map;
}

QColor toQColor(const rn::SharedColor& color) {
  return QColor::fromRgba(static_cast<QRgb>(*color));
}

QString pointerEventsName(rn::PointerEventsMode mode) {
  switch (mode) {
    case rn::PointerEventsMode::None: return QStringLiteral("none");
    case rn::PointerEventsMode::BoxNone: return QStringLiteral("box-none");
    case rn::PointerEventsMode::BoxOnly: return QStringLiteral("box-only");
    case rn::PointerEventsMode::Auto: break;
  }
  return QStringLiteral("auto");
}

/// Connects the item's signals to the emitter, which Fabric replaces on every
/// update (so the bridge holds the latest one).
class ExpoImageBridge final : public QObject {
 public:
  explicit ExpoImageBridge(ExpoImageItem* item) : QObject(item) {
    QObject::connect(item, &ExpoImageItem::loadStarted, this, [this] {
      if (emitter_) emitter_->onLoadStart({true});
    });
    // Qt reports bytes received and total (total is -1 until known); the
    // event is out of 1000 when the total is unknown, so it still advances.
    QObject::connect(item, &ExpoImageItem::progressed, this, [this](double loaded, double total) {
      if (emitter_) emitter_->onProgress({loaded, total > 0 ? total : std::max(loaded, 1.0)});
    });
    QObject::connect(item, &ExpoImageItem::loaded, this,
                     [this](const QString& url, double width, double height, const QString& cacheType,
                            const QString& mediaType, bool animated) {
      if (!emitter_) return;
      rn::ViewManagerAdapter_ExpoImageEventEmitter::OnLoad event{};
      event.cacheType = cacheType.toStdString();
      event.source.url = url.toStdString();
      event.source.width = width;
      event.source.height = height;
      event.source.mediaType = mediaType.toStdString();
      event.source.isAnimated = animated;
      emitter_->onLoad(std::move(event));
    });
    QObject::connect(item, &ExpoImageItem::failed, this, [this](const QString& message) {
      if (emitter_) emitter_->onError({message.toStdString()});
    });
    QObject::connect(item, &ExpoImageItem::displayedChanged, this, [this, item] {
      if (emitter_ && item->displayed()) emitter_->onDisplay({true});
    });
  }

  void setEmitter(std::shared_ptr<const rn::ViewManagerAdapter_ExpoImageEventEmitter> emitter) {
    emitter_ = std::move(emitter);
  }

 private:
  std::shared_ptr<const rn::ViewManagerAdapter_ExpoImageEventEmitter> emitter_;
};

ExpoImageBridge* bridgeFor(ExpoImageItem* item) {
  auto* bridge = item->findChild<ExpoImageBridge*>(QString(), Qt::FindDirectChildrenOnly);
  return bridge != nullptr ? bridge : new ExpoImageBridge(item);
}

void applyViewProps(ExpoImageItem* item, const rn::ViewManagerAdapter_ExpoImageProps& props) {
  item->setTestId(QString::fromStdString(props.testId));
  item->setNativeId(QString::fromStdString(props.nativeId));
  item->setPointerEvents(pointerEventsName(props.pointerEvents));
  item->setHitSlop(QMarginsF(props.hitSlop.left, props.hitSlop.top, props.hitSlop.right, props.hitSlop.bottom));
  item->setAccessible(props.accessible);
  item->setAccessibilityLabel(QString::fromStdString(props.accessibilityLabel));
  item->setAccessibilityRole(QString::fromStdString(props.accessibilityRole));
  item->setAccessibilityHint(QString::fromStdString(props.accessibilityHint));
  const auto& value = props.accessibilityValue;
  item->setAccessibilityValue(value.text ? QString::fromStdString(*value.text)
                                         : value.now ? QString::number(*value.now) : QString());
  const auto& state = props.accessibilityState;
  item->setAccessibilityStateDisabled(state && state->disabled);
  item->setAccessibilityStateSelected(state && state->selected);
  item->setAccessibilityStateChecked(state && state->checked == rn::AccessibilityState::Checked);
}

}  // namespace

std::vector<std::string> expoImageComponentNames() {
  return {std::string(rn::ViewManagerAdapter_ExpoImageComponentDescriptor::ConcreteShadowNode::Name())};
}

void addExpoImageComponentDescriptors(rn::ComponentDescriptorProviderRegistry& registry) {
  registry.add(rn::concreteComponentDescriptorProvider<rn::ViewManagerAdapter_ExpoImageComponentDescriptor>());
}

bool applyExpoImageProps(QQuickItem* quickItem, const rn::ShadowView& view) {
  const auto* props = dynamic_cast<const rn::ViewManagerAdapter_ExpoImageProps*>(view.props.get());
  auto* item = qobject_cast<ExpoImageItem*>(quickItem);
  if (props == nullptr || item == nullptr) return false;
  bridgeFor(item)->setEmitter(
      std::dynamic_pointer_cast<const rn::ViewManagerAdapter_ExpoImageEventEmitter>(view.eventEmitter));

  applyViewProps(item, *props);
  item->setContentFit(props->contentFit.empty() ? QStringLiteral("cover")
                                                : QString::fromStdString(props->contentFit));
  item->setPlaceholderContentFit(QString::fromStdString(props->placeholderContentFit));
  item->setContentPosition(toVariant(props->contentPosition));
  item->setTransition(toTransition(props->transition));
  item->setBlurRadius(props->blurRadius);
  const bool tinted = *props->tintColor != 0;
  item->setHasTint(tinted);
  if (tinted) item->setTintColor(toQColor(props->tintColor));
  item->setCachePolicy(props->cachePolicy.empty() ? QStringLiteral("memory-disk")
                                                  : QString::fromStdString(props->cachePolicy));
  item->setPriority(QString::fromStdString(props->priority));
  item->setAllowDownscaling(props->allowDownscaling);
  item->setAutoplay(props->autoplay);
  item->setRecyclingKey(QString::fromStdString(props->recyclingKey));
  item->setPlaceholder(toList(props->placeholder));
  // Last: loading reads everything above.
  item->setSource(toList(props->source));
  return true;
}

}  // namespace rnqnx::fabric
