#include "react-native-qnx/views/QtExtraItems.h"

#include "react-native-qnx/views/DecodedImageCache.h"
#include "react-native-qnx/views/QnxImageProvider.h"
#include "react-native-qnx/views/QtQuickItemFactory.h"

#include <QByteArray>
#include <QQmlEngine>
#include <QQuickItem>

#if RNQNX_ENABLE_QT_SVG
#include <QtSvgItem.h>
#endif
#if RNQNX_ENABLE_QT_RNSVG
#include <RnsvgItems.h>
#endif
#if RNQNX_ENABLE_QT_NATIVE_UI || RNQNX_ENABLE_QT_EXPO_IMAGE
#include <QtNativeUiQml.h>
#endif
#if RNQNX_ENABLE_QT_EXPO_IMAGE
#include "react-native-qnx/views/image/ExpoImageItem.h"
#endif
#if RNQNX_ENABLE_QT_VIDEO
#include <QtVideoItem.h>
#endif

namespace rnqnx::views {

namespace {
QQmlEngine* g_extraItemEngine = nullptr;
}  // namespace

void setExtraItemEngine(QQmlEngine& engine) {
  g_extraItemEngine = &engine;
}

QQuickItem* createExtraItem(const QString& componentName, QQuickItem* parent) {
  QQuickItem* item = nullptr;
#if RNQNX_ENABLE_QT_SVG
  if (componentName == QStringLiteral("QnxSvgDocument")) {
    item = new QtSvgItem(parent);
  }
#endif
#if RNQNX_ENABLE_QT_VIDEO
  if (item == nullptr && componentName == QStringLiteral("QnxQtVideo") && g_extraItemEngine != nullptr) {
    item = new QtVideoItem(*g_extraItemEngine, parent);
  }
#endif
#if RNQNX_ENABLE_QT_RNSVG
  if (item == nullptr && componentName.startsWith(QStringLiteral("RNSVG"))) {
    item = rnsvg::createItemForComponent(componentName, parent);
  }
#endif
  if (item != nullptr) {
    // The mounting layer deletes these explicitly; say so to the QML engine.
    QQmlEngine::setObjectOwnership(item, QQmlEngine::CppOwnership);
  }
  (void)componentName;
  (void)parent;
  return item;
}

void registerExtraQmlTypes(QtQuickItemFactory& factory) {
  (void)factory;
#if RNQNX_ENABLE_QT_NATIVE_UI
  factory.registerType(QStringLiteral("RNQnxDrawer"), QByteArray(nativeui::kDrawerQml));
  factory.registerType(QStringLiteral("RNQnxTabBar"), QByteArray(nativeui::kTabBarQml));
#endif
#if RNQNX_ENABLE_QT_EXPO_IMAGE
  ExpoImageItem::registerQmlType();
  factory.registerType(
      QStringLiteral("ViewManagerAdapter_ExpoImage"), QByteArray(nativeui::kExpoImageQml));
#endif
}

QString resolveImageUri(QQmlEngine& engine, const QString& uri) {
  if (DecodedImageCache::shared().contains(uri)) {
    return QStringLiteral("image://%1/cache/%2")
        .arg(QLatin1String(QnxImageProvider::providerId()), DecodedImageCache::key(uri));
  }
  auto* provider = dynamic_cast<QnxImageProvider*>(
      engine.imageProvider(QLatin1String(QnxImageProvider::providerId())));
  if (provider != nullptr) {
    const QString provided = provider->registerDataUri(uri);
    if (!provided.isEmpty()) {
      return provided;
    }
  }
  return uri;
}

}  // namespace rnqnx::views
