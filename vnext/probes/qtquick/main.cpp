// Host probe for ADR-0002: measures the cached-QQmlComponent instantiation
// strategy (Option 1) for create, update, and destroy of many items.
//
// This runs on the macOS development host under the offscreen QPA plugin in
// CTest, and on a real display when invoked without `-platform offscreen`.
// Host numbers ground the design; ADR-0002 still requires the same benchmark
// on the Pi 4 before Option 1 is confirmed for the target.

#include "react-native-qnx/views/QtQuickItemFactory.h"

#include <QByteArray>
#include <QGuiApplication>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QString>

#include <chrono>
#include <cstdio>
#include <vector>

namespace {

constexpr int kItemCount = 1000;
constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 720;

// Type sources are the minimum shape each React Native component needs from
// Qt Quick. They are compiled once per type and then mutated from C++.
constexpr const char* kViewSource =
    "import QtQuick\nRectangle { visible: true }\n";

double elapsedMs(std::chrono::steady_clock::time_point start) {
  const auto delta = std::chrono::steady_clock::now() - start;
  return std::chrono::duration<double, std::milli>(delta).count();
}

}  // namespace

int main(int argc, char** argv) {
  QGuiApplication app(argc, argv);

  QQuickWindow window;
  window.setWidth(kWindowWidth);
  window.setHeight(kWindowHeight);
  window.setTitle(QStringLiteral("react-native-qnx Qt Quick host probe"));

  QQmlEngine engine;
  rnqnx::views::QtQuickItemFactory factory(engine);
  factory.registerType(QStringLiteral("View"), QByteArray(kViewSource));

  QQuickItem* root = window.contentItem();
  std::vector<QQuickItem*> items;
  items.reserve(kItemCount);

  const auto createStart = std::chrono::steady_clock::now();
  for (int i = 0; i < kItemCount; ++i) {
    QQuickItem* item = factory.create(QStringLiteral("View"), root);
    if (item == nullptr) {
      std::fprintf(
          stderr,
          "RNQNX_QTQUICK_HOST_FAIL create index=%d error=%s\n",
          i,
          factory.lastError().toUtf8().constData());
      return 1;
    }
    item->setX(static_cast<qreal>(i % 40) * 32.0);
    item->setY(static_cast<qreal>(i / 40) * 18.0);
    item->setWidth(30.0);
    item->setHeight(16.0);
    items.push_back(item);
  }
  const double createMs = elapsedMs(createStart);

  // Fabric updates arrive as property mutations on existing items, which is the
  // path that has to stay cheap once a real mounting layer drives it.
  const auto updateStart = std::chrono::steady_clock::now();
  for (int frame = 0; frame < 10; ++frame) {
    const qreal offset = static_cast<qreal>(frame);
    for (QQuickItem* item : items) {
      item->setX(item->x() + offset);
      item->setOpacity(frame % 2 == 0 ? 1.0 : 0.5);
    }
  }
  const double updateMs = elapsedMs(updateStart);

  const auto destroyStart = std::chrono::steady_clock::now();
  for (QQuickItem* item : items) {
    delete item;
  }
  items.clear();
  const double destroyMs = elapsedMs(destroyStart);

  if (root->childItems().size() != 0) {
    std::fprintf(
        stderr,
        "RNQNX_QTQUICK_HOST_FAIL residual_children=%lld\n",
        static_cast<long long>(root->childItems().size()));
    return 1;
  }

  if (!factory.isCompiled(QStringLiteral("View"))) {
    std::fprintf(stderr, "RNQNX_QTQUICK_HOST_FAIL type_not_cached\n");
    return 1;
  }

  // An unregistered type must fail loudly rather than returning a bare item.
  if (factory.create(QStringLiteral("Missing"), root) != nullptr) {
    std::fprintf(stderr, "RNQNX_QTQUICK_HOST_FAIL unregistered_type_created\n");
    return 1;
  }

  std::printf(
      "RNQNX_QTQUICK_HOST_OK items=%d create_ms=%.3f update10_ms=%.3f "
      "destroy_ms=%.3f platform=%s\n",
      kItemCount,
      createMs,
      updateMs,
      destroyMs,
      QGuiApplication::platformName().toUtf8().constData());
  return 0;
}
