// Host test for the ExpoImage view through React Native's own Fabric path: a
// Hermes runtime drives `nativeFabricUIManager`, the shadow tree commits, and the
// ExpoImage item mounts on Qt Quick. It checks what the mounting layer does with
// `opacity`, `transform` and `onLayout`, that the bridge turns the item's image
// events into Fabric events, and that Fabric view commands reach the item.
// Host evidence only: no QNX, no device.
#include "support/ExpoImageTestSupport.h"

#include "react-native-qnx/fabric/QtQuickMountingLayer.h"
#include "react-native-qnx/fabric/ShadowViewMutationAdapter.h"
#include "react-native-qnx/runtime/QtReactHost.h"
#include "react-native-qnx/views/DecodedImageCache.h"
#include "react-native-qnx/views/QtQuickItemFactory.h"
#include "react-native-qnx/views/image/ExpoImageItem.h"
#include "react-native-qnx/views/image/ImageRefRegistry.h"

#include <QBuffer>
#include <QElapsedTimer>
#include <QFile>
#include <QGuiApplication>
#include <QHostAddress>
#include <QImage>
#include <QImageReader>
#include <QPointer>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QUrl>

#include <cmath>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using rnqnx::views::ExpoImageItem;

constexpr int kSurface = 21;

using expo_image_test::check;
using expo_image_test::fileUri;
using expo_image_test::pngBytes;
using expo_image_test::PngServer;

bool near(double a, double b, double tolerance = 0.01) { return std::fabs(a - b) <= tolerance; }

class Rig {
 public:
  Rig()
      : factory_(engine_), layer_(factory_, *window_.contentItem(), kSurface), adapter_(layer_), host_(adapter_) {
    factory_.registerType(QStringLiteral("View"), "import QtQuick\nItem { property color backgroundColor: 'transparent' }\n");
    adapter_.mapComponent(QStringLiteral("View"), QStringLiteral("View"));
    host_.installTurboModuleBinding();
    host_.installComponentProvider();
    host_.installPlatformConstants();
    host_.installTimers();
    check(host_.hasComponent("ViewManagerAdapter_ExpoImage"), "ExpoImage descriptor missing from the runtime");
    window_.resize(300, 200);
    window_.show();
    host_.startEmptySurface(kSurface, 300.0F, 200.0F);
    eval(R"JS(
      globalThis.events = [];
      globalThis.fabric = globalThis.nativeFabricUIManager;
      fabric.registerEventHandler((instance, type, payload) => {
        events.push({ id: instance && instance.id, type, payload });
      });
      globalThis.surfaceId = )JS" + std::to_string(kSurface) + R"JS(;
      globalThis.nodes = {};
      // Fabric holds a node's instance handle weakly; keep them alive.
      globalThis.handles = {};
      globalThis.show = (...ids) => {
        const set = fabric.createChildSet();
        for (const id of ids) fabric.appendChildToSet(set, nodes[id]);
        fabric.completeRoot(surfaceId, set);
      };
      globalThis.make = (id, props) => {
        handles[id] = { id };
        nodes[id] = fabric.createNode(id, 'ViewManagerAdapter_ExpoImage', surfaceId,
          Object.assign({ collapsable: false }, props), handles[id]);
        return nodes[id];
      };
      globalThis.update = (id, props) => { nodes[id] = fabric.cloneNodeWithNewProps(nodes[id], props); };
    )JS");
  }
  ~Rig() { host_.stopSurface(kSurface); }

  void eval(const std::string& script) {
    if (!host_.evaluate(script, "expo-image-fabric-test.js")) throw std::runtime_error(host_.lastError().toStdString());
  }
  void pump(int milliseconds) {
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < milliseconds) {
      host_.runtime().drainMicrotasks();
      host_.pumpEvents();
    }
  }
  void settle(const std::function<bool()>& ready, const char* what, int timeoutMs = 5000) {
    QElapsedTimer timer;
    timer.start();
    do {
      host_.runtime().drainMicrotasks();
      host_.pumpEvents();
      if (ready()) return;
    } while (timer.elapsed() < timeoutMs);
    throw std::runtime_error(std::string("Timed out waiting for ") + what);
  }
  ExpoImageItem* image(int tag) { return qobject_cast<ExpoImageItem*>(layer_.itemForTag(tag)); }
  int eventCount(const std::string& tag, const std::string& type) {
    auto& rt = host_.runtime();
    auto result = rt.global().getPropertyAsFunction(rt, "countEvents");
    return static_cast<int>(result.call(rt, facebook::jsi::String::createFromUtf8(rt, tag),
                                        facebook::jsi::String::createFromUtf8(rt, type)).asNumber());
  }
  std::string jsonOf(const std::string& expression) {
    eval("globalThis.__json = JSON.stringify(" + expression + ");");
    auto& rt = host_.runtime();
    return rt.global().getProperty(rt, "__json").asString(rt).utf8(rt);
  }
  rnqnx::runtime::QtReactHost& host() { return host_; }
  QQuickWindow& window() { return window_; }

 private:
  QQuickWindow window_;
  QQmlEngine engine_;
  rnqnx::views::QtQuickItemFactory factory_;
  rnqnx::fabric::QtQuickMountingLayer layer_;
  rnqnx::fabric::ShadowViewMutationAdapter adapter_;
  rnqnx::runtime::QtReactHost host_;
};

}  // namespace

int main(int argc, char** argv) {
  QGuiApplication application(argc, argv);
  QQuickWindow::setGraphicsApi(QSGRendererInterface::Software);
  try {
    QTemporaryDir dir;
    check(dir.isValid(), "temp dir");
    const QString red = dir.filePath("red.png");
    const QString blue = dir.filePath("blue.png");
    expo_image_test::writeFile(red, pngBytes(QSize(10, 6), Qt::red));
    expo_image_test::writeFile(blue, pngBytes(QSize(10, 6), Qt::blue));
    rnqnx::views::DecodedImageCache::shared().clear();

    Rig rig;
    rig.eval(R"JS(
      globalThis.countEvents = (id, type) => events.filter(e => String(e.id) === id && e.type === type).length;
    )JS");

    // ---- opacity, transform, layout -------------------------------------
    rig.eval("make(701, { width: 40, height: 30, opacity: 0.5, transform: [{ translateX: 7 }, { translateY: 3 }, { scale: 2 }, { rotate: '90deg' }],"
             " cachePolicy: 'none', source: [{ uri: '" + fileUri(red) + "' }] }); show(701);");
    rig.settle([&] { return rig.image(701) != nullptr; }, "the ExpoImage item to mount");
    ExpoImageItem* item = rig.image(701);
    check(near(item->width(), 40) && near(item->height(), 30), "Yoga size reached the item");
    check(near(item->opacity(), 0.5), "opacity 0.5 reached the item");
    check(near(item->scale(), 2.0), "transform scale reached the item");
    check(near(item->rotation(), 90.0), "transform rotate (90deg) reached the item");
    check(near(item->x(), 7) && near(item->y(), 3), "transform translate moved the item");

    // An update changes them in place, on the same item.
    rig.eval("update(701, { opacity: 0.25, transform: [{ translateX: -4 }, { scale: 0.5 }] }); show(701);");
    rig.settle([&] { return near(item->opacity(), 0.25); }, "the opacity update");
    check(rig.image(701) == item, "an update keeps the same item");
    check(near(item->scale(), 0.5) && near(item->rotation(), 0.0) && near(item->x(), -4) && near(item->y(), 0),
          "the transform update replaced scale, rotation and translation");

    // onLayout: Fabric's layout event carries the frame Yoga computed.
    rig.settle([&] { return rig.eventCount("701", "topLayout") >= 1; }, "the layout event");
    const std::string layout = rig.jsonOf("events.filter(e => e.id === 701 && e.type === 'topLayout')[0].payload.layout");
    check(layout.find("\"width\":40") != std::string::npos && layout.find("\"height\":30") != std::string::npos,
          "onLayout reported the 40x30 frame: " + layout);
    const int layoutEvents = rig.eventCount("701", "topLayout");
    rig.eval("update(701, { width: 55, height: 20 }); show(701);");
    rig.settle([&] { return near(item->width(), 55) && rig.eventCount("701", "topLayout") > layoutEvents; },
               "the resized layout event");
    const std::string resized = rig.jsonOf("events.filter(e => e.id === 701 && e.type === 'topLayout').pop().payload.layout");
    check(resized.find("\"width\":55") != std::string::npos && resized.find("\"height\":20") != std::string::npos,
          "onLayout reported the new 55x20 frame: " + resized);

    // ---- image events become Fabric events --------------------------------
    rig.settle([&] { return rig.eventCount("701", "topLoad") >= 1 && rig.eventCount("701", "topDisplay") >= 1; },
               "the load and display events");
    check(rig.eventCount("701", "topLoadStart") == 1, "onLoadStart fired once");
    const std::string load = rig.jsonOf("events.filter(e => e.id === 701 && e.type === 'topLoad')[0].payload");
    check(load.find("\"cacheType\":\"none\"") != std::string::npos, "onLoad carries cacheType none: " + load);
    check(load.find("\"width\":10") != std::string::npos && load.find("\"height\":6") != std::string::npos,
          "onLoad carries the picture size: " + load);
    check(load.find("image/png") != std::string::npos, "onLoad carries the media type: " + load);
    check(load.find("red.png") != std::string::npos, "onLoad carries the url: " + load);
    const std::string display = rig.jsonOf("events.filter(e => e.id === 701 && e.type === 'topDisplay')[0].payload.displayed");
    check(display == "true", "onDisplay carries displayed true: " + display);
    // Order: start before load before display.
    check(rig.jsonOf("events.filter(e => e.id === 701).map(e => e.type).filter(t => ['topLoadStart','topLoad','topDisplay'].includes(t))") ==
              "[\"topLoadStart\",\"topLoad\",\"topDisplay\"]",
          "image events arrive in the order start, load, display");

    // onError.
    rig.eval("make(702, { width: 20, height: 20, cachePolicy: 'none', source: [{ uri: 'file:///definitely/missing.png' }] }); show(701, 702);");
    rig.settle([&] { return rig.eventCount("702", "topError") >= 1; }, "the error event");
    const std::string error = rig.jsonOf("events.filter(e => e.id === 702 && e.type === 'topError')[0].payload");
    check(error.find("\"error\":\"") != std::string::npos && error.size() > 12, "onError carries a message: " + error);
    check(rig.eventCount("702", "topLoad") == 0, "a failed load sends no onLoad");

    // onProgress from a real HTTP download.
    PngServer server(pngBytes(QSize(64, 64), Qt::green));
    rig.eval("make(703, { width: 20, height: 20, cachePolicy: 'none', source: [{ uri: '" + server.url().toStdString() +
             "' }] }); show(701, 702, 703);");
    rig.settle([&] { return rig.eventCount("703", "topLoad") >= 1; }, "the network load");
    check(rig.eventCount("703", "topProgress") >= 1, "onProgress fired during the download");
    const std::string progress = rig.jsonOf("events.filter(e => e.id === 703 && e.type === 'topProgress').pop().payload");
    check(progress.find("\"loaded\":") != std::string::npos && progress.find("\"total\":") != std::string::npos,
          "onProgress carries loaded and total: " + progress);

    // The picture is on screen: a node whose source is a decoded ImageRef.
    QImage ref(8, 8, QImage::Format_ARGB32);
    ref.fill(QColor(0, 200, 0));
    rnqnx::views::ImageRefEntry entry;
    entry.image = ref;
    entry.mediaType = QStringLiteral("image/png");
    const auto refId = rnqnx::views::ImageRefRegistry::shared().add(entry);
    rig.eval("make(704, { width: 16, height: 16, source: " + std::to_string(refId) + " }); show(701, 702, 703, 704);");
    rig.settle([&] { return rig.image(704) != nullptr && rig.image(704)->displayed(); }, "the ImageRef source to display");
    const QImage shown = rig.image(704)->grabImage();
    check(shown.pixelColor(8, 8).green() > 150 && shown.pixelColor(8, 8).red() < 60, "the ImageRef picture was painted");
    rig.eval("update(704, { source: " + std::to_string(refId + 1000) + " }); show(701, 702, 703, 704);");
    rig.settle([&] { return rig.eventCount("704", "topError") >= 1; }, "the error for a released ImageRef");

    // ---- Fabric view commands ---------------------------------------------
    const QString gifPath = dir.filePath("anim.gif");
    expo_image_test::writeFile(gifPath, expo_image_test::animatedGif());
    QImageReader gifReader(gifPath);
    const bool gifSupported = gifReader.supportsAnimation() && gifReader.imageCount() > 1;
    if (gifSupported) {
      rig.eval("make(705, { width: 20, height: 20, cachePolicy: 'none', source: [{ uri: '" + fileUri(gifPath) +
               "' }] }); show(701, 702, 703, 704, 705);");
      rig.settle([&] { return rig.image(705) != nullptr && rig.image(705)->animating(); }, "the GIF to play");
      rig.eval("fabric.dispatchCommand(nodes[705], 'stopAnimating', []);");
      rig.settle([&] { return !rig.image(705)->animating(); }, "stopAnimating to pause the GIF");
      rig.eval("fabric.dispatchCommand(nodes[705], 'startAnimating', []);");
      rig.settle([&] { return rig.image(705)->animating(); }, "startAnimating to resume the GIF");
    } else {
      std::cout << "RNQNX_EXPO_IMAGE_FABRIC_SKIP animated GIF unsupported by this Qt build\n";
    }

    // lockResource holds the picture against a source change; unlockResource
    // applies the latest source; reload loads again whatever the lock says.
    const std::string redUri = fileUri(red);
    const std::string blueUri = fileUri(blue);
    rig.eval("make(706, { width: 20, height: 20, cachePolicy: 'none', source: [{ uri: '" + redUri + "' }] }); show(701, 702, 703, 704, 706);");
    rig.settle([&] { return rig.image(706) != nullptr && rig.image(706)->displayed(); }, "the first picture");
    ExpoImageItem* held = rig.image(706);
    check(held->shownUri().endsWith(QStringLiteral("red.png")), "the item shows red.png");
    rig.eval("fabric.dispatchCommand(nodes[706], 'lockResource', []);");
    check(held->resourceLocked(), "lockResource locked the item");
    rig.eval("update(706, { source: [{ uri: '" + blueUri + "' }] }); show(701, 702, 703, 704, 706);");
    rig.pump(300);
    check(held->shownUri().endsWith(QStringLiteral("red.png")), "a locked item keeps its picture when source changes");
    rig.eval("fabric.dispatchCommand(nodes[706], 'unlockResource', []);");
    rig.settle([&] { return held->shownUri().endsWith(QStringLiteral("blue.png")); }, "unlockResource to load the latest source");
    check(!held->resourceLocked(), "unlockResource unlocked the item");
    const int loadsBeforeReload = rig.eventCount("706", "topLoad");
    rig.eval("fabric.dispatchCommand(nodes[706], 'lockResource', []); fabric.dispatchCommand(nodes[706], 'reload', []);");
    rig.settle([&] { return rig.eventCount("706", "topLoad") > loadsBeforeReload; }, "reload to load again through the lock");
    rig.eval("fabric.dispatchCommand(nodes[706], 'bogus', []);");
    check(!rig.host().delegate().lastFailures().empty(), "an unknown command is reported");
    check(rig.host().javaScriptErrors().empty(), "no JavaScript errors");

    std::cout << "RNQNX_EXPO_IMAGE_FABRIC_OK opacity transform onLayout events imageRef commands\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
