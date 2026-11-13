#include "react-native-qnx/views/QtQuickItemFactory.h"
#include "react-native-qnx/views/DecodedImageCache.h"
#include "react-native-qnx/views/QtExtraItems.h"

#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QUrl>

#include <utility>

namespace rnqnx::views {
namespace {

// Inline type sources carry no file, so QQmlComponent::setData still needs a
// base URL for diagnostics and relative resolution. Nothing is read from disk.
QUrl syntheticUrlFor(const QString& componentName) {
  return QUrl(QStringLiteral("qrc:/react-native-qnx/%1.qml").arg(componentName));
}

}  // namespace

QtQuickItemFactory::QtQuickItemFactory(QQmlEngine& engine) : engine_(engine) {
  // The engine takes ownership. Registering here means every item this factory
  // creates can be given an image:// source without further setup.
  imageProvider_ = new QnxImageProvider();
  engine_.addImageProvider(
      QLatin1String(QnxImageProvider::providerId()), imageProvider_);
  registerExtraQmlTypes(*this);
}

QString QtQuickItemFactory::resolveImageSource(const QString& uri) {
  return resolveImageUri(engine_, uri);
}

QtQuickItemFactory::~QtQuickItemFactory() {
  for (auto* component : std::as_const(components_)) {
    delete component;
  }
}

void QtQuickItemFactory::registerType(
    const QString& componentName,
    QByteArray qmlSource) {
  sources_.insert(componentName, std::move(qmlSource));

  // Re-registering a type invalidates any component compiled from the previous
  // source, otherwise create() would keep returning the stale type.
  if (auto* stale = components_.take(componentName)) {
    delete stale;
  }
}

bool QtQuickItemFactory::isCompiled(const QString& componentName) const {
  return components_.contains(componentName);
}

QString QtQuickItemFactory::lastError() const {
  return lastError_;
}

QQmlComponent* QtQuickItemFactory::componentFor(const QString& componentName) {
  if (auto* cached = components_.value(componentName, nullptr)) {
    return cached;
  }

  const auto source = sources_.constFind(componentName);
  if (source == sources_.constEnd()) {
    lastError_ = QStringLiteral("No QML type source registered for '%1'")
                     .arg(componentName);
    return nullptr;
  }

  auto* component = new QQmlComponent(&engine_);
  component->setData(*source, syntheticUrlFor(componentName));
  if (component->isError()) {
    lastError_ = component->errorString();
    delete component;
    return nullptr;
  }

  components_.insert(componentName, component);
  return component;
}

QQuickItem* QtQuickItemFactory::create(
    const QString& componentName,
    QQuickItem* parent) {
  if (QQuickItem* extra = createExtraItem(componentName, parent)) {
    lastError_.clear();
    return extra;
  }
  auto* component = componentFor(componentName);
  if (component == nullptr) {
    return nullptr;
  }

  QObject* object = component->create();
  if (object == nullptr) {
    lastError_ = component->errorString();
    return nullptr;
  }

  auto* item = qobject_cast<QQuickItem*>(object);
  if (item == nullptr) {
    lastError_ = QStringLiteral("QML type '%1' is not a QQuickItem")
                     .arg(componentName);
    delete object;
    return nullptr;
  }

  // C++-constructed objects already default to CppOwnership, but the mounting
  // layer deletes these items explicitly, so the lifetime rule is stated rather
  // than assumed. Seam: lib/QtQml.framework/Headers/qjsengine.h:310-311.
  QQmlEngine::setObjectOwnership(item, QQmlEngine::CppOwnership);

  item->setParentItem(parent);
  lastError_.clear();
  return item;
}

}  // namespace rnqnx::views
