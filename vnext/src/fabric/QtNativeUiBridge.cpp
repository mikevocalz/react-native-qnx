// Fabric side of the Qt Quick drawer, tab bar and ExpoImage view.
//
// The items are QML (native/qt/nativeui). Each Fabric update copies the
// generated props onto the item's QML properties; the item reports back through
// a bridge object, a QObject child of the item that QML calls (the way the
// Image item calls QtImageBridge). The bridge holds the latest event emitter.
#include "react-native-qnx/fabric/QtNativeUiBridge.h"

#include "react-native-qnx/views/QtCssColor.h"
#include "react-native-qnx/views/QtExtraItems.h"

#include <folly/dynamic.h>
#include <react/renderer/componentregistry/ComponentDescriptorProviderRegistry.h>
#include <react/renderer/mounting/ShadowView.h>

#if RNQNX_ENABLE_QT_NATIVE_UI
#include <react/renderer/components/RnqnxQtDrawer/ComponentDescriptors.h>
#include <react/renderer/components/RnqnxQtDrawer/EventEmitters.h>
#include <react/renderer/components/RnqnxQtDrawer/Props.h>
#include <react/renderer/components/RnqnxQtTabBar/ComponentDescriptors.h>
#include <react/renderer/components/RnqnxQtTabBar/EventEmitters.h>
#include <react/renderer/components/RnqnxQtTabBar/Props.h>
#endif

#include <QByteArray>
#include <QColor>
#include <QObject>
#include <QQmlEngine>
#include <QQuickItem>
#include <QString>
#include <QUrl>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>
#include <qqml.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace rnqnx::fabric {
namespace {

namespace rn = facebook::react;

#if RNQNX_ENABLE_QT_NATIVE_UI

/// A color prop typed as UnsafeMixed: a CSS string (see views/QtCssColor.h) or a
/// processed color number (0xAARRGGBB, which JavaScript may hand over signed).
std::optional<QColor> parseColor(const folly::dynamic& value) {
  if (value.isNumber()) {
    const auto bits = static_cast<std::uint32_t>(static_cast<std::int64_t>(value.asDouble()));
    return QColor::fromRgba(bits);
  }
  if (!value.isString()) return std::nullopt;
  return views::parseCssColor(QString::fromStdString(value.getString()));
}

void setMixedColorProp(QQuickItem* item, const char* name, const folly::dynamic& value) {
  if (const auto color = parseColor(value)) {
    item->setProperty(name, *color);
  }
}

#endif  // RNQNX_ENABLE_QT_NATIVE_UI


#if RNQNX_ENABLE_QT_NATIVE_UI
template <typename Bridge>
Bridge* bridgeFor(QQuickItem* item) {
  auto* bridge = item->findChild<Bridge*>(QString(), Qt::FindDirectChildrenOnly);
  if (bridge == nullptr) {
    bridge = new Bridge(item);
    // QML declares `property var bridge`; hand it the QObject.
    item->setProperty("bridge", QVariant::fromValue(static_cast<QObject*>(bridge)));
  }
  return bridge;
}
#endif

#if RNQNX_ENABLE_QT_NATIVE_UI

// ---- RNQnxDrawer -----------------------------------------------------------

class DrawerBridge final : public QObject {
  Q_OBJECT

 public:
  explicit DrawerBridge(QObject* parent) : QObject(parent) {}

  void setEmitter(std::shared_ptr<const rn::RNQnxDrawerEventEmitter> emitter) {
    emitter_ = std::move(emitter);
  }

  /// A change the user asked for (edge or panel drag, scrim tap, Escape).
  Q_INVOKABLE void requestChange(bool open, const QString& reason) {
    if (emitter_) emitter_->onRequestChange({open, reason.toStdString()});
  }
  /// The drawer settled open or closed, whatever started the move.
  Q_INVOKABLE void transitionEnd(bool open) {
    if (emitter_) emitter_->onTransitionEnd({open});
  }

 private:
  std::shared_ptr<const rn::RNQnxDrawerEventEmitter> emitter_;
};

bool applyDrawer(QQuickItem* item, const rn::ShadowView& view) {
  const auto* props = dynamic_cast<const rn::RNQnxDrawerProps*>(view.props.get());
  if (props == nullptr) return false;
  auto* bridge = bridgeFor<DrawerBridge>(item);
  bridge->setEmitter(std::dynamic_pointer_cast<const rn::RNQnxDrawerEventEmitter>(view.eventEmitter));

  item->setProperty("drawerWidth", props->drawerWidth);
  item->setProperty("side", QString::fromUtf8(rn::toString(props->side)));
  item->setProperty("drawerType", QString::fromUtf8(rn::toString(props->drawerType)));
  item->setProperty("swipeEnabled", props->swipeEnabled);
  setMixedColorProp(item, "scrimColor", props->scrimColor);
  item->setProperty("scrimLabel", QString::fromStdString(props->accessibilityLabel));
  item->setProperty("edgeWidth", props->edgeWidth);
  item->setProperty("swipeExclusionLeft", props->swipeExclusionLeft);
  item->setProperty("topInset", props->topInset);
  // Children may have been restacked since the last signal.
  QMetaObject::invokeMethod(item, "syncPanels");
  // Last: the animation reads the width and side above.
  item->setProperty("open", props->open);
  return true;
}

// ---- RNQnxTabBar -----------------------------------------------------------

class TabBarBridge final : public QObject {
  Q_OBJECT

 public:
  explicit TabBarBridge(QObject* parent) : QObject(parent) {}

  void setEmitter(std::shared_ptr<const rn::RNQnxTabBarEventEmitter> emitter) {
    emitter_ = std::move(emitter);
  }

  /// The tabs last handed to QML, so an unchanged list is not set again.
  QByteArray tabsSignature;

  Q_INVOKABLE void tabPressed(const QString& key) {
    if (emitter_) emitter_->onTabPress({key.toStdString()});
  }
  Q_INVOKABLE void tabLongPressed(const QString& key) {
    if (emitter_) emitter_->onTabLongPress({key.toStdString()});
  }
  Q_INVOKABLE void layoutModeChanged(const QString& mode, double size) {
    if (emitter_) emitter_->onLayoutMode({mode.toStdString(), size});
  }
  /// Icon URIs go through the same resolver as Image, so a prefetched icon
  /// comes from the shared decoded cache.
  Q_INVOKABLE QString resolveImage(const QString& uri) {
    auto* item = qobject_cast<QQuickItem*>(parent());
    auto* engine = item != nullptr ? qmlEngine(item) : nullptr;
    return engine != nullptr ? views::resolveImageUri(*engine, uri) : uri;
  }

 private:
  std::shared_ptr<const rn::RNQnxTabBarEventEmitter> emitter_;
};

bool applyTabBar(QQuickItem* item, const rn::ShadowView& view) {
  const auto* props = dynamic_cast<const rn::RNQnxTabBarProps*>(view.props.get());
  if (props == nullptr) return false;
  auto* bridge = bridgeFor<TabBarBridge>(item);
  bridge->setEmitter(std::dynamic_pointer_cast<const rn::RNQnxTabBarEventEmitter>(view.eventEmitter));

  QByteArray signature;
  QVariantList tabs;
  for (const auto& tab : props->tabs) {
    QVariantMap entry;
    entry.insert(QStringLiteral("key"), QString::fromStdString(tab.key));
    entry.insert(QStringLiteral("title"), QString::fromStdString(tab.title));
    entry.insert(QStringLiteral("icon"), QString::fromStdString(tab.icon));
    entry.insert(QStringLiteral("badge"), QString::fromStdString(tab.badge));
    entry.insert(QStringLiteral("accessibilityLabel"), QString::fromStdString(tab.accessibilityLabel));
    tabs.append(entry);
    for (const auto* text : {&tab.key, &tab.title, &tab.icon, &tab.badge, &tab.accessibilityLabel}) {
      signature.append(text->data(), static_cast<qsizetype>(text->size()));
      signature.append('\x1f');
    }
    signature.append('\x1e');
  }
  if (signature != bridge->tabsSignature || item->property("tabs").isNull()) {
    bridge->tabsSignature = signature;
    item->setProperty("tabs", tabs);
  }

  item->setProperty("layout", QString::fromUtf8(rn::toString(props->layout)));
  item->setProperty("sideBreakpoint", props->sideBreakpoint);
  item->setProperty("railWidth", props->railWidth);
  item->setProperty("barHeight", props->barHeight);
  item->setProperty("availableWidth", props->availableWidth);
  setMixedColorProp(item, "tintActive", props->tintActive);
  setMixedColorProp(item, "tintInactive", props->tintInactive);
  setMixedColorProp(item, "badgeColor", props->badgeColor);
  setMixedColorProp(item, "badgeTextColor", props->badgeTextColor);
  setMixedColorProp(item, "dividerColor", props->dividerColor);
  item->setProperty("hidden", props->hidden);
  item->setProperty("activeKey", QString::fromStdString(props->activeKey));
  return true;
}

#endif  // RNQNX_ENABLE_QT_NATIVE_UI


}  // namespace

#if RNQNX_ENABLE_QT_NATIVE_UI

std::vector<std::string> nativeUiComponentNames() {
  return {std::string(rn::RNQnxDrawerComponentDescriptor::ConcreteShadowNode::Name()),
          std::string(rn::RNQnxTabBarComponentDescriptor::ConcreteShadowNode::Name())};
}

void addNativeUiComponentDescriptors(rn::ComponentDescriptorProviderRegistry& registry) {
  registry.add(rn::concreteComponentDescriptorProvider<rn::RNQnxDrawerComponentDescriptor>());
  registry.add(rn::concreteComponentDescriptorProvider<rn::RNQnxTabBarComponentDescriptor>());
}

bool applyNativeUiProps(QQuickItem* item, const rn::ShadowView& view) {
  const std::string name(view.componentName != nullptr ? view.componentName : "");
  if (name == rn::RNQnxDrawerComponentDescriptor::ConcreteShadowNode::Name()) {
    return applyDrawer(item, view);
  }
  if (name == rn::RNQnxTabBarComponentDescriptor::ConcreteShadowNode::Name()) {
    return applyTabBar(item, view);
  }
  return false;
}

#endif


}  // namespace rnqnx::fabric

#include "QtNativeUiBridge.moc"
