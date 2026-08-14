#pragma once

#include <QByteArray>
#include <QHash>
#include <QString>

QT_BEGIN_NAMESPACE
class QQmlComponent;
class QQmlEngine;
class QQuickItem;
QT_END_NAMESPACE

namespace rnqnx::views {

/// Instantiates Qt Quick built-in items from cached, inline-declared QML types.
///
/// Qt Quick's built-in items (Rectangle, Text, Image) are private C++ classes
/// registered as QML types, so the public C++ surface is QQuickItem plus the
/// QML type system. ADR-0002 Option 1 therefore compiles one QQmlComponent per
/// React Native component type and mutates the resulting items imperatively.
/// QML is a type source here; it is never authored as UI.
///
/// Seams (installed Qt 6.11.1, macOS development host):
/// - lib/QtQml.framework/Headers/qqmlcomponent.h:46 QQmlComponent(QQmlEngine*,
///   QObject*), :94 setData(const QByteArray&, const QUrl&), :63 isError(),
///   :69 errorString(), :75 create(QQmlContext*)
/// - lib/QtQuick.framework/Headers/qquickitem.h:186 setParentItem(QQuickItem*)
class QtQuickItemFactory {
 public:
  explicit QtQuickItemFactory(QQmlEngine& engine);
  ~QtQuickItemFactory();

  QtQuickItemFactory(const QtQuickItemFactory&) = delete;
  QtQuickItemFactory& operator=(const QtQuickItemFactory&) = delete;
  QtQuickItemFactory(QtQuickItemFactory&&) = delete;
  QtQuickItemFactory& operator=(QtQuickItemFactory&&) = delete;

  /// Declares the QML type source backing one component type. Compilation is
  /// deferred to the first create() so that registration cannot fail.
  void registerType(const QString& componentName, QByteArray qmlSource);

  /// Returns a new item parented to \a parent, or nullptr. On failure
  /// lastError() carries the QML compilation or type error; no partially
  /// constructed object is returned and no object is leaked.
  QQuickItem* create(const QString& componentName, QQuickItem* parent);

  /// True once the component type has been compiled successfully.
  bool isCompiled(const QString& componentName) const;

  QString lastError() const;

 private:
  QQmlComponent* componentFor(const QString& componentName);

  QQmlEngine& engine_;
  QHash<QString, QByteArray> sources_;
  QHash<QString, QQmlComponent*> components_;
  QString lastError_;
};

}  // namespace rnqnx::views
