#pragma once

#include <QString>

QT_BEGIN_NAMESPACE
class QQmlEngine;
class QQuickItem;
QT_END_NAMESPACE

namespace rnqnx::views {

class QtQuickItemFactory;

/// The item half of the extra-component hook. A feature that adds a Fabric
/// component supplies its Qt Quick item here, in one place, instead of an
/// `#if` block inside QtQuickItemFactory. The Fabric half (names, descriptors,
/// props) is `fabric/QtExtraComponents.h`.
///
/// Returns a new item for a component built in C++ (react-native-svg, the SVG
/// document), or nullptr when \a componentName is not one of them.
QQuickItem* createExtraItem(const QString& componentName, QQuickItem* parent);

/// Registers the QML-sourced extra components (drawer, tab bar, ExpoImage
/// view) on \a factory. Called once from the factory's constructor.
void registerExtraQmlTypes(QtQuickItemFactory& factory);

/// Resolves an image URI for a Qt Quick `Image`: a URI the decoded cache holds
/// becomes `image://rnqnx/cache/<key>` (no second download, no second cache), a
/// base64 data URI is decoded through the platform provider, and anything else
/// passes through. QtQuickItemFactory::resolveImageSource uses the same code.
QString resolveImageUri(QQmlEngine& engine, const QString& uri);

}  // namespace rnqnx::views
