// Host test for the ExpoImage native module on real Hermes: the module that
// @rn-qnx/expo-image-qnx registers is installed under `globalThis.expo`, the
// QNX Expo polyfill is evaluated on top of it, and JavaScript calls what
// expo-image's public API calls. The renderer's ImageLoader TurboModule is the
// real one, so a call goes JavaScript -> module -> TurboModule -> Qt loader,
// disk cache and decoded cache, and the test reads those caches back.
// Host evidence only: no QNX, no device.
#include "support/ExpoImageTestSupport.h"

#include "ExpoQnxInstaller.hpp"
#include "react-native-qnx/fabric/QtQuickMountingLayer.h"
#include "react-native-qnx/fabric/ShadowViewMutationAdapter.h"
#include "react-native-qnx/runtime/QtReactHost.h"
#include "react-native-qnx/views/DecodedImageCache.h"
#include "react-native-qnx/views/QtQuickItemFactory.h"
#include "react-native-qnx/views/image/BlurHashEncoder.h"
#include "react-native-qnx/views/image/ExpoImageItem.h"
#include "react-native-qnx/views/image/ImageDiskCache.h"
#include "react-native-qnx/views/image/ImageRefRegistry.h"
#include "react-native-qnx/views/image/ThumbHash.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QGuiApplication>
#include <QImageReader>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QTemporaryDir>

#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>

namespace rnqnx::expo::image {
void registerModule();
}

namespace {

using expo_image_test::check;
using expo_image_test::dataUri;
using expo_image_test::fileUri;
using expo_image_test::pngBytes;
using expo_image_test::PngServer;
using rnqnx::views::ExpoImageItem;

constexpr int kSurface = 31;

std::string readFile(const char* path) {
  std::ifstream file(path);
  check(file.good(), std::string("cannot read ") + path);
  return std::string((std::istreambuf_iterator<char>(file)), {});
}

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
    window_.resize(300, 200);
    window_.show();
    host_.startEmptySurface(kSurface, 300.0F, 200.0F);
    // The module registers itself the way a consumer app's generated
    // installExpoModules does, then the polyfill (a Metro bundle entry in an
    // app) adds the JavaScript classes, with a stand-in for `require`.
    rnqnx::expo::image::registerModule();
    rnqnx::expo::installExpoModulesQnx(host_.runtime());
    eval("globalThis.__renderer = { dispatchCommand: (ref, name, args) => nativeFabricUIManager.dispatchCommand(ref.__node, name, args) };"
         "globalThis.__polyfill = (function () {"
         "  var module = { exports: {} }; var exports = module.exports;"
         "  var require = function (name) {"
         "    if (name === 'react-native/Libraries/ReactNative/RendererProxy') return __renderer;"
         "    throw new Error('unexpected require ' + name);"
         "  };\n" + readFile(RNQNX_EXPO_POLYFILL_PATH) + "\n  return module.exports;"
         "})();");
  }
  ~Rig() { host_.stopSurface(kSurface); }

  void eval(const std::string& script) {
    if (!host_.evaluate(script, "expo-image-module-test.js")) throw std::runtime_error(host_.lastError().toStdString());
  }
  // Runs an async script and waits for it; returns the failures it recorded.
  std::string runAsync(const std::string& body, int timeoutMs = 20000) {
    eval("globalThis.__done = false; globalThis.__failures = [];"
         "globalThis.expect = (ok, message) => { if (!ok) __failures.push(message); };"
         "(async () => {" + body + "})().catch((e) => { __failures.push('threw: ' + (e && e.stack || e)); })"
         ".then(() => { globalThis.__done = true; });");
    auto& rt = host_.runtime();
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeoutMs && !rt.global().getProperty(rt, "__done").getBool()) {
      rt.drainMicrotasks();
      host_.pumpEvents();
    }
    check(rt.global().getProperty(rt, "__done").getBool(), "the script did not finish: " + failures());
    return failures();
  }
  std::string failures() {
    eval("globalThis.__failureText = __failures.join('\\n');");
    auto& rt = host_.runtime();
    return rt.global().getProperty(rt, "__failureText").asString(rt).utf8(rt);
  }
  double numberOf(const std::string& expression) {
    eval("globalThis.__n = " + expression + ";");
    return number("__n");
  }
  double number(const char* name) {
    auto& rt = host_.runtime();
    return rt.global().getProperty(rt, name).asNumber();
  }
  std::string text(const char* name) {
    auto& rt = host_.runtime();
    return rt.global().getProperty(rt, name).asString(rt).utf8(rt);
  }
  ExpoImageItem* image(int tag) { return qobject_cast<ExpoImageItem*>(layer_.itemForTag(tag)); }
  void settle(const std::function<bool()>& ready, const char* what) {
    QElapsedTimer timer;
    timer.start();
    do {
      host_.runtime().drainMicrotasks();
      host_.pumpEvents();
      if (ready()) return;
    } while (timer.elapsed() < 5000);
    throw std::runtime_error(std::string("Timed out waiting for ") + what);
  }
  rnqnx::runtime::QtReactHost& host() { return host_; }

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
  QCoreApplication::setApplicationName(QStringLiteral("rnqnx-expo-image-host-test"));
  try {
    QTemporaryDir dir;
    check(dir.isValid(), "temp dir");
    auto& disk = rnqnx::views::ImageDiskCache::shared();
    disk.clear();
    rnqnx::views::DecodedImageCache::shared().clear();

    const QByteArray png = pngBytes(QSize(10, 6), QColor(250, 20, 20));
    const QString pngPath = dir.filePath("source.png");
    expo_image_test::writeFile(pngPath, png);
    PngServer server(pngBytes(QSize(64, 32), QColor(20, 200, 20)));

    Rig rig;

    // ---- the module's surface ---------------------------------------------
    std::string failures = rig.runAsync(R"JS(
      const mod = expo.modules.ExpoImage;
      for (const name of ['prefetch', 'clearMemoryCache', 'clearDiskCache', 'getCachePathAsync', 'configureCache',
                          'loadAsync', 'generateBlurhashAsync', 'generateThumbhashAsync', 'writeToCacheAsync',
                          'readFromCacheAsync']) {
        expect(typeof mod[name] === 'function', name + ' is a function');
      }
      expect(typeof mod.Image === 'function', 'Image (the ImageRef class) exists');
      const view = mod.ViewPrototypes.ExpoImage;
      for (const name of ['startAnimating', 'stopAnimating', 'lockResourceAsync', 'unlockResourceAsync', 'reloadAsync']) {
        expect(typeof view[name] === 'function', 'ViewPrototypes.ExpoImage.' + name);
      }
      expect(mod instanceof expo.NativeModule, 'the polyfill gave the module the NativeModule prototype');
    )JS");
    check(failures.empty(), "module surface:\n" + failures);

    // ---- prefetch, cache clearing and the cache path ------------------------
    const std::string url = server.url().toStdString();
    failures = rig.runAsync(R"JS(
      const mod = expo.modules.ExpoImage;
      const loader = nativeModuleProxy.ImageLoader;
      const url = ')JS" + url + R"JS(';
      const data = ')JS" + dataUri(png) + R"JS(';
      expect(await mod.getCachePathAsync(url) === null, 'nothing cached for the URL before the prefetch');

      // prefetch resolves true once every URL is fetched and cached; the
      // headers it is given go to the server.
      expect(await mod.prefetch([url + '?h=1'], 'memory', { 'X-Prefetch': 'yes' }) === true, 'prefetch with headers');
      expect(await mod.prefetch([url], 'disk') === true, 'prefetch resolved true');
      const path = await mod.getCachePathAsync(url);
      expect(typeof path === 'string' && path.length > 0, 'prefetch with cachePolicy disk filled the disk cache: ' + path);

      // memory-disk fills the decoded cache the view reads.
      expect(await mod.prefetch([data], 'memory-disk') === true, 'prefetch of a data URI');
      expect((await loader.queryCache([data]))[data] === 'memory', 'prefetch put the picture in the decoded cache');

      // A failing URL resolves false, never rejects, even beside a good one.
      expect(await mod.prefetch(['file:///definitely/missing.png'], 'memory-disk') === false, 'a missing file resolves false');
      expect(await mod.prefetch([data, 'file:///definitely/missing.png'], 'memory-disk') === false, 'one failure resolves false');
      expect(await mod.prefetch([], 'memory-disk') === true, 'an empty list resolves true');
      expect(await mod.prefetch('not-an-array') === false, 'a non-array resolves false');

      expect(await mod.clearMemoryCache() === true, 'clearMemoryCache resolved true');
      expect(Object.keys(await loader.queryCache([data])).length === 0, 'clearMemoryCache emptied the decoded cache');
      expect(typeof await mod.getCachePathAsync(url) === 'string', 'clearMemoryCache left the disk cache alone');
      expect(await mod.clearDiskCache() === true, 'clearDiskCache resolved true');
      expect(await mod.getCachePathAsync(url) === null, 'clearDiskCache emptied the disk cache');
    )JS");
    check(failures.empty(), "prefetch and caches:\n" + failures);
    {
      bool sawHeader = false;
      for (const QString& request : server.requests()) sawHeader = sawHeader || request.contains("X-Prefetch: yes");
      check(sawHeader, "prefetch sent the headers it was given");
    }

    // ---- loadAsync and ImageRef ----------------------------------------------
    failures = rig.runAsync(R"JS(
      const mod = expo.modules.ExpoImage;
      const data = ')JS" + dataUri(png) + R"JS(';
      // expo-observe subscribes to the module's imageLoaded event.
      globalThis.__loadedEvents = [];
      mod.addListener('imageLoaded', (event) => __loadedEvents.push(event));
      const ref = await mod.loadAsync({ uri: data });
      expect(ref instanceof expo.SharedRef, 'an ImageRef is a SharedRef');
      expect(ref instanceof expo.SharedObject, 'an ImageRef is a SharedObject');
      expect(ref instanceof mod.Image, 'an ImageRef is an instance of Image.Image');
      expect(ref.nativeRefType === 'image', 'nativeRefType is image');
      expect(ref.width === 10 && ref.height === 6, 'size ' + ref.width + 'x' + ref.height);
      expect(ref.scale === 1, 'scale is 1');
      expect(ref.mediaType === 'image/png', 'mediaType ' + ref.mediaType);
      expect(ref.isAnimated === false, 'isAnimated is false');
      expect(typeof ref.__expo_shared_object_id__ === 'number', 'the shared object id the view receives');
      globalThis.__refId = ref.__expo_shared_object_id__;

      const limited = await mod.loadAsync({ uri: ')JS" + url + R"JS(' }, { maxWidth: 16 });
      expect(limited.width === 16 && limited.height === 8, 'maxWidth keeps the aspect ratio: ' + limited.width + 'x' + limited.height);
      const both = await mod.loadAsync({ uri: ')JS" + url + R"JS(' }, { maxWidth: 40, maxHeight: 4 });
      expect(both.width === 8 && both.height === 4, 'maxHeight wins when it binds: ' + both.width + 'x' + both.height);
      const untouched = await mod.loadAsync({ uri: ')JS" + url + R"JS(' }, { maxWidth: 500 });
      expect(untouched.width === 64, 'a limit above the size does not upscale');
      globalThis.__tintId = (await mod.loadAsync({ uri: data }, { tintColor: 0xFF0000FF | 0 })).__expo_shared_object_id__;

      let rejected = false;
      try { await mod.loadAsync({ uri: 'file:///definitely/missing.png' }); } catch (e) { rejected = e instanceof Error; }
      expect(rejected, 'loadAsync rejects for a missing file');
      rejected = false;
      try { await mod.loadAsync({}); } catch (e) { rejected = true; }
      expect(rejected, 'loadAsync rejects a source without uri');

      // A blurhash source decodes without a network.
      const hashed = await mod.loadAsync({ uri: 'blurhash:/LEHV6nWB2yk8pyo0adR*.7kCMdnj', width: 20, height: 12 });
      expect(hashed.width === 20 && hashed.height === 12, 'blurhash source size');

      ref.release();
    )JS");
    check(failures.empty(), "loadAsync:\n" + failures);
    // loadAsync emitted the module's imageLoaded event for the first load.
    rig.settle([&] { return rig.numberOf("__loadedEvents.length") >= 1; }, "the imageLoaded event");
    check(rig.numberOf("__loadedEvents[0].width") == 10 && rig.numberOf("__loadedEvents[0].height") == 6 &&
              rig.numberOf("__loadedEvents[0].url.startsWith('data:image/png') ? 1 : 0") == 1,
          "imageLoaded carries url, width and height");
    {
      auto& registry = rnqnx::views::ImageRefRegistry::shared();
      check(registry.find(static_cast<std::uint32_t>(rig.number("__refId"))).image.isNull(), "release() freed the picture");
      const auto tinted = registry.find(static_cast<std::uint32_t>(rig.number("__tintId")));
      check(!tinted.image.isNull(), "the tinted ImageRef holds a picture");
      const QColor pixel = tinted.image.pixelColor(3, 3);
      check(pixel.blue() > 240 && pixel.red() < 10 && pixel.green() < 10, "tintColor replaced the picture's colour");
    }

    // ---- generateBlurhashAsync / generateThumbhashAsync ------------------------
    failures = rig.runAsync(R"JS(
      const mod = expo.modules.ExpoImage;
      const ref = await mod.loadAsync({ uri: ')JS" + dataUri(pngBytes(QSize(16, 12), QColor(40, 90, 200))) + R"JS(' });
      globalThis.__blurRef = await mod.generateBlurhashAsync(ref, [4, 3]);
      globalThis.__blurObject = await mod.generateBlurhashAsync(ref, { width: 3, height: 3 });
      globalThis.__blurUrl = await mod.generateBlurhashAsync(')JS" + url + R"JS(', [2, 2]);
      globalThis.__blurDefault = await mod.generateBlurhashAsync(ref);
      expect(await mod.generateBlurhashAsync(ref, [0, 3]) === null, 'components outside 1 to 9 resolve null');
      expect(await mod.generateBlurhashAsync(ref, [10, 3]) === null, 'components above 9 resolve null');
      globalThis.__thumbRef = await mod.generateThumbhashAsync(ref);
      globalThis.__thumbUrl = await mod.generateThumbhashAsync(')JS" + url + R"JS(');
      let rejected = false;
      try { await mod.generateThumbhashAsync('file:///definitely/missing.png'); } catch (e) { rejected = true; }
      expect(rejected, 'generateThumbhashAsync rejects for a missing file');
    )JS");
    check(failures.empty(), "hashes:\n" + failures);
    {
      QImage solid(16, 12, QImage::Format_RGBA8888);
      solid.fill(QColor(40, 90, 200));
      check(QString::fromStdString(rig.text("__blurRef")) == rnqnx::views::encodeBlurHash(solid, 4, 3),
            "generateBlurhashAsync matches the encoder for [4, 3]");
      check(QString::fromStdString(rig.text("__blurObject")) == rnqnx::views::encodeBlurHash(solid, 3, 3),
            "numberOfComponents accepts { width, height }");
      check(rig.text("__blurDefault").size() == 28, "numberOfComponents defaults to [4, 3]");
      check(rig.text("__blurUrl").size() == 4 + 2 * 2 * 2, "a URL source is fetched and encoded");
      const QImage decoded = rnqnx::views::decodeThumbHash(QString::fromStdString(rig.text("__thumbRef")));
      check(!decoded.isNull() && decoded.width() > decoded.height(), "the thumbhash decodes to a landscape picture");
      const QColor mid = decoded.pixelColor(decoded.width() / 2, decoded.height() / 2);
      check(mid.blue() > 150 && mid.red() < 90, "the thumbhash keeps the picture's colour");
      check(!rig.text("__thumbUrl").empty(), "a URL source gets a thumbhash");
    }

    // ---- writeToCacheAsync / readFromCacheAsync ------------------------------
    failures = rig.runAsync(R"JS(
      const mod = expo.modules.ExpoImage;
      expect(await mod.readFromCacheAsync('no-such-key') === null, 'an unknown key reads as null');
      expect(await mod.writeToCacheAsync(')JS" + fileUri(pngPath) + R"JS(', 'file-key') === undefined, 'writeToCacheAsync resolves');
      const stored = await mod.getCachePathAsync('file-key');
      expect(typeof stored === 'string', 'the written key has a cache path');
      const back = await mod.readFromCacheAsync('file-key');
      expect(back instanceof expo.SharedRef && back.width === 10 && back.height === 6, 'readFromCacheAsync returns the picture');

      // An ImageRef is stored as a PNG under its key.
      const ref = await mod.loadAsync({ uri: ')JS" + dataUri(pngBytes(QSize(7, 5), QColor(1, 2, 3))) + R"JS(' });
      await mod.writeToCacheAsync(ref, 'ref-key');
      const refBack = await mod.readFromCacheAsync('ref-key');
      expect(refBack && refBack.width === 7 && refBack.height === 5, 'an ImageRef written to the cache reads back');

      // A source naming the key is served from the cache with no network: the
      // URL below refuses connections.
      const served = await mod.loadAsync({ uri: 'http://127.0.0.1:1/never.png', cacheKey: 'file-key' });
      expect(served.width === 10 && served.height === 6, 'a source with the cacheKey loads from the cache');
      let rejected = false;
      try { await mod.loadAsync({ uri: 'http://127.0.0.1:1/never.png' }); } catch (e) { rejected = true; }
      expect(rejected, 'the same URL without the key has nothing to load');

      rejected = false;
      try { await mod.writeToCacheAsync('file:///definitely/missing.png', 'x'); } catch (e) { rejected = true; }
      expect(rejected, 'writeToCacheAsync rejects for a missing file');
      rejected = false;
      try { await mod.writeToCacheAsync(ref, ''); } catch (e) { rejected = true; }
      expect(rejected, 'writeToCacheAsync rejects an empty key');
      ref.release();
      let released = false;
      try { await mod.writeToCacheAsync(ref, 'again'); } catch (e) { released = true; }
      expect(released, 'a released ImageRef cannot be written');
    )JS");
    check(failures.empty(), "cache read and write:\n" + failures);
    check(QFile::exists(disk.pathFor(QStringLiteral("key:file-key"))), "the disk cache file for the key exists");

    // ---- configureCache -----------------------------------------------------
    failures = rig.runAsync(R"JS(
      const mod = expo.modules.ExpoImage;
      const loader = nativeModuleProxy.ImageLoader;
      mod.configureCache({ maxDiskSize: 123456, maxMemoryCost: 2 * 1024 * 1024, maxMemoryCount: 1 });
      const a = 'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR4nGMwTpv5HwAENAIyWy0K4AAAAABJRU5ErkJggg==';
      const b = ')JS" + dataUri(png) + R"JS(';
      const ra = await mod.prefetch([a], 'memory');
      const afterA = Object.keys(await loader.queryCache([a, b])).length;
      const rb = await mod.prefetch([b], 'memory');
      expect(ra === true && rb === true && afterA === 1, 'prefetch results ' + ra + ' ' + rb + ' afterA ' + afterA);
      const cached = await loader.queryCache([a, b]);
      expect(Object.keys(cached).length === 1, 'maxMemoryCount 1 keeps one picture: ' + Object.keys(cached).length);
    )JS");
    check(failures.empty(), "configureCache:\n" + failures);
    check(disk.budgetBytes() == 123456, "maxDiskSize set the disk budget");
    check(rnqnx::views::DecodedImageCache::shared().maxCostKiB() == 2048, "maxMemoryCost set the memory ceiling");
    rig.eval("expo.modules.ExpoImage.configureCache({ maxDiskSize: 134217728, maxMemoryCost: 33554432, maxMemoryCount: 0 });");
    check(disk.budgetBytes() == 134217728, "the disk budget is restored");

    // ---- view functions through Fabric --------------------------------------------
    rig.eval(R"JS(
      globalThis.fabric = nativeFabricUIManager;
      globalThis.nodes = {};
      globalThis.handles = {};
      globalThis.show = () => {
        const set = fabric.createChildSet();
        for (const id of Object.keys(nodes)) fabric.appendChildToSet(set, nodes[id]);
        fabric.completeRoot()JS" + std::to_string(kSurface) + R"JS(, set);
      };
      globalThis.fakeComponent = (id) => ({ getNativeRef: () => ({ __node: nodes[id] }) });
    )JS");
    const QString gifPath = dir.filePath("anim.gif");
    expo_image_test::writeFile(gifPath, expo_image_test::animatedGif());
    QImageReader reader(gifPath);
    if (reader.supportsAnimation() && reader.imageCount() > 1) {
      rig.eval("handles[7] = { id: 7 }; nodes[7] = fabric.createNode(7, 'ViewManagerAdapter_ExpoImage', " + std::to_string(kSurface) +
               ", { width: 20, height: 20, collapsable: false, cachePolicy: 'none', source: [{ uri: '" + fileUri(gifPath) +
               "' }] }, handles[7]); show();");
      rig.settle([&] { return rig.image(7) != nullptr && rig.image(7)->animating(); }, "the GIF to play");
      // ref.stopAnimating() on the view component: the prototype function
      // dispatches the command through React Native's dispatchCommand.
      failures = rig.runAsync(R"JS(
        const view = expo.modules.ExpoImage.ViewPrototypes.ExpoImage;
        expect(view.stopAnimating.call(fakeComponent(7)) === undefined, 'stopAnimating returns undefined');
      )JS");
      check(failures.empty(), "stopAnimating:\n" + failures);
      rig.settle([&] { return !rig.image(7)->animating(); }, "stopAnimating to pause the GIF");
      failures = rig.runAsync("expo.modules.ExpoImage.ViewPrototypes.ExpoImage.startAnimating.call(fakeComponent(7));");
      check(failures.empty(), "startAnimating:\n" + failures);
      rig.settle([&] { return rig.image(7)->animating(); }, "startAnimating to resume the GIF");
    } else {
      std::cout << "RNQNX_EXPO_IMAGE_MODULE_SKIP animated GIF unsupported by this Qt build\n";
    }

    // lockResourceAsync, unlockResourceAsync and reloadAsync return Promises and
    // reach the item.
    rig.eval("handles[8] = { id: 8 }; nodes[8] = fabric.createNode(8, 'ViewManagerAdapter_ExpoImage', " + std::to_string(kSurface) +
             ", { width: 20, height: 20, collapsable: false, source: [{ uri: '" + fileUri(pngPath) +
             "' }] }, handles[8]); show();");
    const double eventsBefore = rig.numberOf("__loadedEvents.length");
    rig.settle([&] { return rig.image(8) != nullptr && rig.image(8)->displayed(); }, "the picture to display");
    // The view's own load reaches the same module event.
    rig.settle([&] { return rig.numberOf("__loadedEvents.length") > eventsBefore; }, "the view's imageLoaded event");
    check(rig.numberOf("__loadedEvents[__loadedEvents.length - 1].url.endsWith('source.png') ? 1 : 0") == 1,
          "the view's imageLoaded names the file");
    failures = rig.runAsync(R"JS(
      const view = expo.modules.ExpoImage.ViewPrototypes.ExpoImage;
      const lock = view.lockResourceAsync.call(fakeComponent(8));
      expect(lock && typeof lock.then === 'function', 'lockResourceAsync returns a Promise');
      await lock;
    )JS");
    check(failures.empty(), "lockResourceAsync:\n" + failures);
    check(rig.image(8)->resourceLocked(), "lockResourceAsync locked the item");
    failures = rig.runAsync("await expo.modules.ExpoImage.ViewPrototypes.ExpoImage.unlockResourceAsync.call(fakeComponent(8));");
    check(failures.empty(), "unlockResourceAsync:\n" + failures);
    check(!rig.image(8)->resourceLocked(), "unlockResourceAsync unlocked the item");
    failures = rig.runAsync(R"JS(
      const view = expo.modules.ExpoImage.ViewPrototypes.ExpoImage;
      await view.reloadAsync.call(fakeComponent(8));
      // A component with no mounted view has nothing to command.
      await view.reloadAsync.call({ getNativeRef: () => null });
      let threw = false;
      try { view.startAnimating.call(undefined); } catch (e) { threw = true; }
      expect(threw, 'calling a view function without a view throws');
    )JS");
    check(failures.empty(), "reloadAsync:\n" + failures);

    check(rig.host().javaScriptErrors().empty(), "no JavaScript errors were reported");
    std::cout << "RNQNX_EXPO_IMAGE_MODULE_OK surface prefetch caches loadAsync hashes cache-io configure view-commands\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
