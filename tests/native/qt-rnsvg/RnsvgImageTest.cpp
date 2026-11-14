// Image element loading: the load-state machine, the decoder and the binding
// to a real element, all against a fake ImageProvider. No network and no
// vnext code; the HTTP half lives in tests/native/qt-rnsvg-image-http.
//
// Run under ASan (RNQNX_QT_RNSVG_SANITIZERS) to check the lifetime tests: a
// completion that reaches a freed loader or element is a use-after-free there.
#include "RnsvgItems.h"
#include "image/ImageBinding.h"
#include "image/ImageDecoding.h"
#include "image/ImageElementProps.h"
#include "image/ImageLoader.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QQuickWindow>
#include <QTest>

#include <list>
#include <map>

using namespace rnqnx::views;
using namespace rnqnx::views::rnsvg;

namespace {

Length px(double v) { return Length{v, Length::Unit::Px, true}; }

QImage solid(int w, int h, const QColor& c) {
  QImage image(w, h, QImage::Format_ARGB32_Premultiplied);
  image.fill(c);
  return image;
}

// Left half red, right half blue: tells every preserveAspectRatio mode apart.
QImage splitImage(int w, int h) {
  QImage image(w, h, QImage::Format_ARGB32_Premultiplied);
  image.fill(Qt::blue);
  QPainter p(&image);
  p.fillRect(0, 0, w / 2, h, Qt::red);
  return image;
}

QByteArray encode(const QImage& image, const char* format = "PNG") {
  QByteArray bytes;
  QBuffer buffer(&bytes);
  buffer.open(QIODevice::WriteOnly);
  image.save(&buffer, format);
  return bytes;
}

QString dataUri(const QImage& image) {
  return QStringLiteral("data:image/png;base64,") + QString::fromLatin1(encode(image).toBase64());
}

// The provider the loader talks to. It records each request, completes
// immediately for URIs registered with `immediate`, and otherwise waits for
// the test to call complete(). A cancelled request never completes through
// complete(); forceComplete() ignores the cancellation to prove the loader
// does not trust the provider to honour it.
class FakeProvider final : public ImageProvider {
 public:
  struct Request {
    QString uri;
    Completion completion;
    bool cancelled{false};
    bool completed{false};
  };

  class Handle final : public ImageRequest {
   public:
    // Shares the request with the provider, so a handle that outlives the
    // provider (a root destroyed after the host) still has something to write to.
    explicit Handle(std::shared_ptr<Request> request) : request_(std::move(request)) {}
    ~Handle() override { request_->cancelled = true; }
    std::shared_ptr<Request> request_;
  };

  std::unique_ptr<ImageRequest> load(const QString& uri, Completion completion) override {
    auto request = std::make_shared<Request>(Request{uri, std::move(completion), false, false});
    requests.push_back(request);
    if (const auto it = immediate.find(uri); it != immediate.end()) {
      request->completed = true;
      request->completion(it->second);
      return nullptr;
    }
    return std::make_unique<Handle>(request);
  }

  Request& at(int index) {
    auto it = requests.begin();
    std::advance(it, index);
    return **it;
  }
  void complete(int index, ImageLoadResult result) {
    Request& request = at(index);
    if (request.cancelled || request.completed) return;
    request.completed = true;
    request.completion(std::move(result));
  }
  void forceComplete(int index, ImageLoadResult result) { at(index).completion(std::move(result)); }
  int count() const { return int(requests.size()); }

  std::map<QString, ImageLoadResult> immediate;
  std::list<std::shared_ptr<Request>> requests;
};

ImageLoadResult failure(const char* text) { return ImageLoadFailure{QString::fromLatin1(text)}; }

struct SettleLog {
  int count{0};
  ImageState lastState{ImageState::Idle};
  QString lastUri;
  QString lastError;
  QSize lastSize;
  ImageLoader::SettledHandler handler() {
    return [this](const ImageLoader& l) {
      ++count;
      lastState = l.state();
      lastUri = l.uri();
      lastError = l.error();
      lastSize = l.image().size();
    };
  }
};

// One <Svg> in a window, so the root's paint count reflects the repaints Qt
// actually scheduled: a frame renders a QQuickPaintedItem only if update() ran.
struct Scene {
  QQuickWindow window;
  RnsvgRootItem* root;
  RnsvgElementItem* image;
  FakeProvider provider;

  Scene() {
    window.resize(100, 100);
    root = new RnsvgRootItem(window.contentItem());
    root->setSize(QSizeF(100, 100));
    RootProps rp;
    rp.bbWidth = px(100);
    rp.bbHeight = px(100);
    root->setRootProps(rp);
    image = new RnsvgElementItem(Kind::Image, root);
    window.show();
    settle();
  }
  // Renders a frame; returns how many times the root painted during it.
  int settle() {
    const int before = root->paintCount();
    window.grabWindow();
    return root->paintCount() - before;
  }
  // The commit the Fabric bridge makes for a new href.
  void commit(const QString& uri, int meetOrSlice = 0, const QString& align = QStringLiteral("xMidYMid"),
              double w = 100, double h = 100) {
    ElementProps next;
    next.kind = Kind::Image;
    next.x = px(0);
    next.y = px(0);
    next.width = px(w);
    next.height = px(h);
    next.imageUri = uri;
    next.imageAlign = align;
    next.imageMeetOrSlice = meetOrSlice;
    carryImageLoadState(next, image->props());
    image->setProps(std::move(next));
    RnsvgImageBinding::forElement(*image, provider).setSource(uri);
  }
};

QImage render(RnsvgRootItem& root) {
  QImage image(QSize(qCeil(root.width()), qCeil(root.height())), QImage::Format_ARGB32_Premultiplied);
  image.fill(Qt::transparent);
  QPainter painter(&image);
  root.paint(&painter);
  painter.end();
  return image;
}

bool isColor(const QImage& img, int x, int y, const QColor& c, int tol = 4) {
  const QColor got = img.pixelColor(x, y);
  return std::abs(got.red() - c.red()) <= tol && std::abs(got.green() - c.green()) <= tol &&
         std::abs(got.blue() - c.blue()) <= tol && std::abs(got.alpha() - c.alpha()) <= tol;
}

#define VERIFY_COLOR(img, x, y, c) \
  QVERIFY2(isColor((img), x, y, c), qPrintable(QStringLiteral("(%1,%2) is %3").arg(x).arg(y).arg((img).pixelColor(x, y).name(QColor::HexArgb))))

}  // namespace

class RnsvgImageTest final : public QObject {
  Q_OBJECT

 private Q_SLOTS:
  // ---- loader state machine ------------------------------------------------

  void startsIdleAndEmptyUriStaysIdle() {
    FakeProvider provider;
    SettleLog log;
    ImageLoader loader(provider, log.handler());
    QCOMPARE(loader.state(), ImageState::Idle);
    loader.setSource(QString());
    QCOMPARE(provider.count(), 0);
    QCOMPARE(loader.state(), ImageState::Idle);
    QCOMPARE(log.count, 0);
  }

  void immediateSuccessSettlesOnceAndKeepsTheImage() {
    FakeProvider provider;
    provider.immediate[QStringLiteral("a")] = solid(3, 2, Qt::red);
    SettleLog log;
    ImageLoader loader(provider, log.handler());
    loader.setSource(QStringLiteral("a"));
    QCOMPARE(loader.state(), ImageState::Loaded);
    QCOMPARE(loader.image().size(), QSize(3, 2));
    QCOMPARE(log.count, 1);
    QCOMPARE(log.lastState, ImageState::Loaded);
    QCOMPARE(provider.count(), 1);
  }

  void deferredSuccessIsLoadingUntilTheProviderCompletes() {
    FakeProvider provider;
    SettleLog log;
    ImageLoader loader(provider, log.handler());
    loader.setSource(QStringLiteral("a"));
    QCOMPARE(loader.state(), ImageState::Loading);
    QCOMPARE(log.count, 0);
    QVERIFY(loader.image().isNull());
    provider.complete(0, solid(4, 4, Qt::green));
    QCOMPARE(loader.state(), ImageState::Loaded);
    QCOMPARE(log.count, 1);
    QCOMPARE(log.lastSize, QSize(4, 4));
  }

  void failureReportsTheProviderMessageAndIsNotRetried() {
    FakeProvider provider;
    SettleLog log;
    ImageLoader loader(provider, log.handler());
    loader.setSource(QStringLiteral("a"));
    provider.complete(0, failure("HTTP 404"));
    QCOMPARE(loader.state(), ImageState::Failed);
    QCOMPARE(loader.error(), QStringLiteral("HTTP 404"));
    QVERIFY(loader.image().isNull());
    QCOMPARE(log.count, 1);
    // The same URI on the next commit does not start another request.
    loader.setSource(QStringLiteral("a"));
    QCOMPARE(provider.count(), 1);
    QCOMPARE(log.count, 1);
  }

  void emptyFailureMessageGetsAGenericOne() {
    FakeProvider provider;
    ImageLoader loader(provider, nullptr);
    loader.setSource(QStringLiteral("a"));
    provider.complete(0, failure(""));
    QCOMPARE(loader.state(), ImageState::Failed);
    QVERIFY(!loader.error().isEmpty());
  }

  void sameUriWhileLoadingDoesNotStartASecondRequest() {
    FakeProvider provider;
    ImageLoader loader(provider, nullptr);
    loader.setSource(QStringLiteral("a"));
    loader.setSource(QStringLiteral("a"));
    QCOMPARE(provider.count(), 1);
    QCOMPARE(loader.generation(), std::uint64_t(1));
  }

  void clearingTheUriCancelsAndGoesIdle() {
    FakeProvider provider;
    SettleLog log;
    ImageLoader loader(provider, log.handler());
    loader.setSource(QStringLiteral("a"));
    loader.setSource(QString());
    QCOMPARE(loader.state(), ImageState::Idle);
    QVERIFY(provider.at(0).cancelled);
    provider.forceComplete(0, solid(2, 2, Qt::red));  // a provider that ignores cancellation
    QCOMPARE(loader.state(), ImageState::Idle);
    QCOMPARE(log.count, 0);
  }

  void changingTheUriCancelsTheOldRequestAndIgnoresItsLateCompletion() {
    FakeProvider provider;
    SettleLog log;
    ImageLoader loader(provider, log.handler());
    loader.setSource(QStringLiteral("a"));
    loader.setSource(QStringLiteral("b"));
    QCOMPARE(provider.count(), 2);
    QVERIFY(provider.at(0).cancelled);
    provider.forceComplete(0, solid(1, 1, Qt::red));  // stale
    QCOMPARE(loader.state(), ImageState::Loading);
    QCOMPARE(loader.uri(), QStringLiteral("b"));
    QCOMPARE(log.count, 0);
    provider.complete(1, solid(7, 7, Qt::blue));
    QCOMPARE(loader.state(), ImageState::Loaded);
    QCOMPARE(loader.image().size(), QSize(7, 7));
    QCOMPARE(log.count, 1);
  }

  void anOlderCompletionArrivingAfterANewerOneIsIgnored() {
    FakeProvider provider;
    ImageLoader loader(provider, nullptr);
    loader.setSource(QStringLiteral("a"));
    loader.setSource(QStringLiteral("b"));
    provider.complete(1, solid(7, 7, Qt::blue));
    provider.forceComplete(0, solid(1, 1, Qt::red));
    QCOMPARE(loader.uri(), QStringLiteral("b"));
    QCOMPARE(loader.image().size(), QSize(7, 7));
  }

  void aCompletionDeliveredTwiceSettlesOnce() {
    FakeProvider provider;
    SettleLog log;
    ImageLoader loader(provider, log.handler());
    loader.setSource(QStringLiteral("a"));
    provider.forceComplete(0, solid(2, 2, Qt::red));
    provider.forceComplete(0, solid(9, 9, Qt::red));
    QCOMPARE(log.count, 1);
    QCOMPARE(loader.image().size(), QSize(2, 2));
  }

  void destroyingTheLoaderMidFlightCancelsAndNeverCallsBack() {
    FakeProvider provider;
    SettleLog log;
    {
      ImageLoader loader(provider, log.handler());
      loader.setSource(QStringLiteral("a"));
    }
    QVERIFY(provider.at(0).cancelled);
    // The loader and its state are gone; ASan flags any touch of them.
    provider.forceComplete(0, solid(2, 2, Qt::red));
    QCOMPARE(log.count, 0);
  }

  void aSettledHandlerMayDestroyTheLoader() {
    FakeProvider provider;
    provider.immediate[QStringLiteral("a")] = solid(2, 2, Qt::red);
    std::unique_ptr<ImageLoader> loader;
    int calls = 0;
    loader = std::make_unique<ImageLoader>(provider, [&](const ImageLoader&) {
      ++calls;
      loader.reset();
    });
    loader->setSource(QStringLiteral("a"));
    QCOMPARE(calls, 1);
    QVERIFY(!loader);
  }

  void aSettledHandlerMayStartTheNextLoad() {
    FakeProvider provider;
    provider.immediate[QStringLiteral("a")] = solid(2, 2, Qt::red);
    provider.immediate[QStringLiteral("b")] = solid(5, 5, Qt::blue);
    ImageLoader* self = nullptr;
    int calls = 0;
    ImageLoader loader(provider, [&](const ImageLoader& l) {
      if (++calls == 1) self->setSource(QStringLiteral("b"));
      Q_UNUSED(l);
    });
    self = &loader;
    loader.setSource(QStringLiteral("a"));
    QCOMPARE(calls, 2);
    QCOMPARE(loader.uri(), QStringLiteral("b"));
    QCOMPARE(loader.image().size(), QSize(5, 5));
  }

  void oversizeAndEmptyResultsFail() {
    FakeProvider provider;
    ImageLoader loader(provider, nullptr);
    loader.setSource(QStringLiteral("wide"));
    provider.complete(0, QImage(kMaxImageDimension + 1, 1, QImage::Format_Alpha8));
    QCOMPARE(loader.state(), ImageState::Failed);
    QVERIFY(loader.error().contains(QStringLiteral("8192")));
    loader.setSource(QStringLiteral("null"));
    provider.complete(1, QImage());
    QCOMPARE(loader.state(), ImageState::Failed);
    loader.setSource(QStringLiteral("limit"));
    provider.complete(2, QImage(kMaxImageDimension, 1, QImage::Format_Alpha8));
    QCOMPARE(loader.state(), ImageState::Loaded);
  }

  // ---- decoding -------------------------------------------------------------

  void decodesBase64AndPercentEncodedDataUris() {
    const auto base64 = decodeDataUri(dataUri(solid(5, 3, Qt::red)));
    QVERIFY(std::holds_alternative<QImage>(base64));
    QCOMPARE(std::get<QImage>(base64).size(), QSize(5, 3));
    const QString percent = QStringLiteral("data:image/png,") + QString::fromLatin1(encode(solid(2, 2, Qt::red)).toPercentEncoding());
    const auto decoded = decodeDataUri(percent);
    QVERIFY(std::holds_alternative<QImage>(decoded));
    QCOMPARE(std::get<QImage>(decoded).size(), QSize(2, 2));
  }

  void rejectsMalformedHostileDataUris() {
    for (const QString& bad : {QStringLiteral("data:image/png;base64"), QStringLiteral("data:,"),
                               QStringLiteral("data:image/png;base64,"), QStringLiteral("data:image/png;base64,!!!!"),
                               QStringLiteral("data:image/png;base64,iVBORw0KGgo="), QStringLiteral("notdata:x,y"),
                               QStringLiteral("data:image/png,%00%00%00")}) {
      QVERIFY2(std::holds_alternative<ImageLoadFailure>(decodeDataUri(bad)), qPrintable(bad));
    }
    // Truncating a valid PNG must fail, not return a half-decoded image.
    const QByteArray png = encode(solid(64, 64, Qt::red));
    const auto truncated = decodeImageBytes(png.left(png.size() / 2));
    QVERIFY(std::holds_alternative<ImageLoadFailure>(truncated));
  }

  void dataUriOverTheCharacterBoundIsRejectedWithoutDecoding() {
    QString huge = QStringLiteral("data:image/png;base64,");
    huge.append(QString(kMaxDataUriChars, QLatin1Char('A')));
    const auto result = decodeDataUri(huge);
    QVERIFY(std::holds_alternative<ImageLoadFailure>(result));
    QVERIFY(std::get<ImageLoadFailure>(result).message.contains(QStringLiteral("too large")));
  }

  void rasterOverTheDimensionBoundFailsBeforeAllocation() {
    // 8193x1 Alpha8 is 8 KiB; the check reads the header, not the pixels.
    const QByteArray png = encode(QImage(kMaxImageDimension + 1, 1, QImage::Format_Grayscale8));
    const auto result = decodeImageBytes(png);
    QVERIFY(std::holds_alternative<ImageLoadFailure>(result));
    QVERIFY(std::get<ImageLoadFailure>(result).message.contains(QStringLiteral("8192")));
    QVERIFY(std::holds_alternative<QImage>(
        decodeImageBytes(encode(QImage(kMaxImageDimension, 1, QImage::Format_Grayscale8)))));
  }

  void garbageAndEmptyBytesFail() {
    QVERIFY(std::holds_alternative<ImageLoadFailure>(decodeImageBytes(QByteArray())));
    QVERIFY(std::holds_alternative<ImageLoadFailure>(decodeImageBytes(QByteArray("not an image at all"))));
    QVERIFY(std::holds_alternative<ImageLoadFailure>(decodeImageBytes(QByteArray(4096, '\xff'))));
  }

  void svgDecodesThroughThePluginOrNamesTheMissingPlugin() {
    const QString svg =
        QStringLiteral("data:image/svg+xml;utf8,<svg xmlns='http://www.w3.org/2000/svg' width='20' height='10' "
                       "viewBox='0 0 20 10'><rect width='20' height='10' fill='%23ff0000'/></svg>");
    const auto result = decodeDataUri(svg);
    if (svgDecodingAvailable()) {
      QVERIFY(std::holds_alternative<QImage>(result));
      const QImage& image = std::get<QImage>(result);
      QCOMPARE(image.size(), QSize(20, 10));
      QVERIFY(isColor(image, 10, 5, Qt::red));
    } else {
      QVERIFY(std::holds_alternative<ImageLoadFailure>(result));
      QCOMPARE(std::get<ImageLoadFailure>(result).message, svgUnavailableMessage());
    }
  }

  void svgBase64DataUriAndGiantSvgAreBounded() {
    if (!svgDecodingAvailable()) QSKIP("Qt's qsvg image-format plugin is not installed", SkipAll);
    const QByteArray giant =
        "<svg xmlns='http://www.w3.org/2000/svg' width='40000' height='20000' viewBox='0 0 40000 20000'>"
        "<rect width='40000' height='20000' fill='blue'/></svg>";
    const auto result = decodeDataUri(QStringLiteral("data:image/svg+xml;base64,") + QString::fromLatin1(giant.toBase64()));
    QVERIFY(std::holds_alternative<QImage>(result));
    const QImage& image = std::get<QImage>(result);
    QVERIFY(image.width() <= kMaxImageDimension && image.height() <= kMaxImageDimension);
    QCOMPARE(image.width(), kMaxImageDimension);  // scaled down, aspect ratio kept
    QCOMPARE(image.height(), kMaxImageDimension / 2);
  }

  void svgDetectionByMediaTypeExtensionAndContent() {
    QVERIFY(uriNamesSvg(QStringLiteral("data:image/svg+xml;utf8,<svg/>")));
    QVERIFY(uriNamesSvg(QStringLiteral("https://x.test/a/logo.SVG?v=2")));
    QVERIFY(uriNamesSvg(QStringLiteral("file:///tmp/a.svgz")));
    QVERIFY(!uriNamesSvg(QStringLiteral("https://x.test/a.png?name=a.svg")));
    QVERIFY(!uriNamesSvg(QStringLiteral("data:image/png;base64,AAAA")));
    QVERIFY(looksLikeSvg("  <?xml version='1.0'?><svg/>"));
    QVERIFY(looksLikeSvg("\xEF\xBB\xBF<svg/>"));
    QVERIFY(!looksLikeSvg("\x89PNG\r\n\x1a\n"));
  }

  // ---- props carry-over -------------------------------------------------------

  void sameUriKeepsTheLoadedImageAndANewOneClearsIt() {
    ElementProps previous;
    previous.imageUri = QStringLiteral("a");
    previous.image = solid(4, 4, Qt::red);
    previous.imageState = toInt(ImageState::Loaded);

    ElementProps same;
    same.imageUri = QStringLiteral("a");
    carryImageLoadState(same, previous);
    QCOMPARE(same.image.size(), QSize(4, 4));
    QCOMPARE(same.imageState, toInt(ImageState::Loaded));

    ElementProps changed;
    changed.imageUri = QStringLiteral("b");
    carryImageLoadState(changed, previous);
    QVERIFY(changed.image.isNull());
    QCOMPARE(changed.imageState, toInt(ImageState::Loading));

    ElementProps cleared;
    carryImageLoadState(cleared, previous);
    QVERIFY(cleared.image.isNull());
    QCOMPARE(cleared.imageState, toInt(ImageState::Idle));
  }

  // ---- binding to a real element ------------------------------------------------

  void bindingRepaintsOnCompletionOnlyAndPaintsTheImage() {
    Scene s;
    s.commit(QStringLiteral("a"));
    s.settle();  // the commit's own repaint
    QCOMPARE(s.settle(), 0);
    QCOMPARE(s.image->props().imageState, toInt(ImageState::Loading));
    QVERIFY(s.image->props().image.isNull());

    s.provider.complete(0, solid(10, 10, Qt::red));
    QCOMPARE(s.image->props().imageState, toInt(ImageState::Loaded));
    QCOMPARE(s.settle(), 1);
    QCOMPARE(s.settle(), 0);
    const QImage img = render(*s.root);
    VERIFY_COLOR(img, 50, 50, Qt::red);
  }

  void staleCompletionNeitherChangesPropsNorRepaints() {
    Scene s;
    s.commit(QStringLiteral("a"));
    s.commit(QStringLiteral("b"));
    s.settle();
    QCOMPARE(s.settle(), 0);
    s.provider.forceComplete(0, solid(10, 10, Qt::red));  // belongs to "a"
    QCOMPARE(s.image->props().imageUri, QStringLiteral("b"));
    QCOMPARE(s.image->props().imageState, toInt(ImageState::Loading));
    QCOMPARE(s.settle(), 0);
    s.provider.complete(1, solid(10, 10, Qt::blue));
    QCOMPARE(s.settle(), 1);
    VERIFY_COLOR(render(*s.root), 50, 50, Qt::blue);
  }

  void changingTheHrefDropsThePreviousImageImmediately() {
    Scene s;
    s.commit(QStringLiteral("a"));
    s.provider.complete(0, solid(10, 10, Qt::red));
    s.settle();
    VERIFY_COLOR(render(*s.root), 50, 50, Qt::red);
    s.commit(QStringLiteral("b"));
    QVERIFY(s.image->props().image.isNull());
    QCOMPARE(s.settle(), 1);
    QVERIFY(render(*s.root).pixelColor(50, 50).alpha() <= 4);
  }

  void aCommitThatKeepsTheHrefDoesNotReload() {
    Scene s;
    s.commit(QStringLiteral("a"));
    s.provider.complete(0, solid(10, 10, Qt::red));
    s.commit(QStringLiteral("a"), 0, QStringLiteral("xMidYMid"), 80, 80);
    QCOMPARE(s.provider.count(), 1);
    QCOMPARE(s.image->props().imageState, toInt(ImageState::Loaded));
    QVERIFY(!s.image->props().image.isNull());
  }

  void failedLoadLeavesTheElementEmptyAndSilent() {
    Scene s;
    int loaded = 0;
    s.commit(QStringLiteral("a"));
    RnsvgImageBinding::existing(*s.image)->setLoadedCallback([&](const ImageLoadedEvent&) { ++loaded; });
    s.provider.complete(0, failure("HTTP 500"));
    QCOMPARE(s.image->props().imageState, toInt(ImageState::Failed));
    QVERIFY(s.image->props().image.isNull());
    QCOMPARE(loaded, 0);
    QVERIFY(render(*s.root).pixelColor(50, 50).alpha() <= 4);
  }

  void loadedCallbackRunsOnceOnALaterEventLoopTurn() {
    Scene s;
    QList<ImageLoadedEvent> events;
    s.commit(QStringLiteral("a"));
    auto& binding = *RnsvgImageBinding::existing(*s.image);
    QCOMPARE(&RnsvgImageBinding::forElement(*s.image, s.provider), &binding);
    binding.setLoadedCallback([&](const ImageLoadedEvent& e) {
      // The element already holds the image when the event fires.
      QCOMPARE(s.image->props().imageState, toInt(ImageState::Loaded));
      events.append(e);
    });
    s.provider.complete(0, solid(6, 4, Qt::red));
    QCOMPARE(events.size(), 0);  // not inside the call that completed the load
    QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
    QCOMPARE(events.size(), 1);
    QCOMPARE(events[0].uri, QStringLiteral("a"));
    QCOMPARE(events[0].size, QSize(6, 4));
    QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
    QCOMPARE(events.size(), 1);
  }

  void immediateLoadDoesNotRunTheCallbackInsideTheCommit() {
    Scene s;
    const QString uri = dataUri(solid(2, 2, Qt::green));
    s.provider.immediate[uri] = solid(2, 2, Qt::green);
    int events = 0;
    s.commit(QStringLiteral("seed"));  // creates the binding
    RnsvgImageBinding::existing(*s.image)->setLoadedCallback([&](const ImageLoadedEvent&) { ++events; });
    s.commit(uri);
    QCOMPARE(s.image->props().imageState, toInt(ImageState::Loaded));
    QCOMPARE(events, 0);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
    QCOMPARE(events, 1);
  }

  void loadedCallbackIsDroppedWhenTheElementMovesOnOrIsDeleted() {
    Scene s;
    int events = 0;
    s.commit(QStringLiteral("a"));
    RnsvgImageBinding::existing(*s.image)->setLoadedCallback([&](const ImageLoadedEvent&) { ++events; });
    s.provider.complete(0, solid(2, 2, Qt::red));
    s.commit(QStringLiteral("b"));  // before the queued call runs
    QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
    QCOMPARE(events, 0);
    s.provider.complete(1, solid(2, 2, Qt::red));
    delete s.image;  // before the queued call runs
    s.image = nullptr;
    QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
    QCOMPARE(events, 0);
  }

  void immediateProviderLoadsInsideTheCommit() {
    Scene s;
    s.provider.immediate[dataUri(solid(2, 2, Qt::green))] = solid(2, 2, Qt::green);
    s.commit(dataUri(solid(2, 2, Qt::green)));
    QCOMPARE(s.image->props().imageState, toInt(ImageState::Loaded));
    s.settle();
    VERIFY_COLOR(render(*s.root), 50, 50, Qt::green);
  }

  void preserveAspectRatioMeetSliceAndNoneWithALoadedImage() {
    // A 40x20 image, red left half and blue right half, in a 100x100 box.
    const auto loadWith = [](int meetOrSlice, const QString& align) {
      auto s = std::make_unique<Scene>();
      s->commit(QStringLiteral("a"), meetOrSlice, align);
      s->provider.complete(0, splitImage(40, 20));
      s->settle();
      return s;
    };
    {  // meet, centered: 100x50 at y 25..75
      auto s = loadWith(0, QStringLiteral("xMidYMid"));
      const QImage img = render(*s->root);
      VERIFY_COLOR(img, 25, 50, Qt::red);
      VERIFY_COLOR(img, 75, 50, Qt::blue);
      QVERIFY(img.pixelColor(25, 10).alpha() <= 4);
      QVERIFY(img.pixelColor(25, 90).alpha() <= 4);
    }
    {  // meet, top left: 100x50 at y 0..50
      auto s = loadWith(0, QStringLiteral("xMinYMin"));
      const QImage img = render(*s->root);
      VERIFY_COLOR(img, 25, 25, Qt::red);
      QVERIFY(img.pixelColor(25, 75).alpha() <= 4);
    }
    {  // meet, bottom right: 100x50 at y 50..100
      auto s = loadWith(0, QStringLiteral("xMaxYMax"));
      const QImage img = render(*s->root);
      VERIFY_COLOR(img, 75, 75, Qt::blue);
      QVERIFY(img.pixelColor(75, 25).alpha() <= 4);
    }
    {  // slice fills the box and crops the sides, keeping the red/blue split at the middle
      auto s = loadWith(1, QStringLiteral("xMidYMid"));
      const QImage img = render(*s->root);
      VERIFY_COLOR(img, 25, 10, Qt::red);
      VERIFY_COLOR(img, 25, 90, Qt::red);
      VERIFY_COLOR(img, 75, 10, Qt::blue);
      VERIFY_COLOR(img, 75, 90, Qt::blue);
    }
    {  // none stretches to the box
      auto s = loadWith(2, QStringLiteral("none"));
      const QImage img = render(*s->root);
      VERIFY_COLOR(img, 10, 10, Qt::red);
      VERIFY_COLOR(img, 90, 90, Qt::blue);
    }
  }

  void destroyingTheElementMidFlightNeverTouchesItAgain() {
    {
      Scene s;
      s.commit(QStringLiteral("a"));
      // The scene's own provider cannot outlive the test body, so complete
      // into a copy of the completion after the element is gone.
      auto completion = s.provider.at(0).completion;
      delete s.image;
      s.image = nullptr;
      QVERIFY(s.provider.at(0).cancelled);
      completion(solid(3, 3, Qt::red));  // ASan: no write to the freed element
      s.window.grabWindow();
    }
    QVERIFY(true);
  }

  void destroyingTheRootMidFlightNeverTouchesTheTreeAgain() {
    FakeProvider provider;
    FakeProvider::Request* request = nullptr;
    ImageProvider::Completion completion;
    {
      QQuickWindow window;
      auto* root = new RnsvgRootItem(window.contentItem());
      auto* element = new RnsvgElementItem(Kind::Image, root);
      ElementProps next;
      next.kind = Kind::Image;
      next.imageUri = QStringLiteral("a");
      carryImageLoadState(next, element->props());
      element->setProps(std::move(next));
      RnsvgImageBinding::forElement(*element, provider).setSource(QStringLiteral("a"));
      request = &provider.at(0);
      completion = request->completion;
      delete root;  // takes the element, its binding and its loader with it
    }
    QVERIFY(request->cancelled);
    completion(solid(3, 3, Qt::red));
    QVERIFY(true);
  }

  void manyElementsLoadIndependently() {
    Scene s;
    constexpr int kCount = 50;
    std::vector<RnsvgElementItem*> elements;
    for (int i = 0; i < kCount; ++i) {
      auto* element = new RnsvgElementItem(Kind::Image, s.root);
      ElementProps next;
      next.kind = Kind::Image;
      next.imageUri = QStringLiteral("u%1").arg(i);
      next.x = px(0); next.y = px(0); next.width = px(10); next.height = px(10);
      carryImageLoadState(next, element->props());
      element->setProps(std::move(next));
      RnsvgImageBinding::forElement(*element, s.provider).setSource(QStringLiteral("u%1").arg(i));
      elements.push_back(element);
    }
    QCOMPARE(s.provider.count(), kCount);
    for (int i = kCount - 1; i >= 0; i -= 2) s.provider.complete(i, solid(2, 2, Qt::red));
    for (int i = 0; i < kCount; ++i) {
      const bool odd = (kCount - 1 - i) % 2 == 0;
      QCOMPARE(elements[std::size_t(i)]->props().imageState, toInt(odd ? ImageState::Loaded : ImageState::Loading));
    }
  }
};

QTEST_MAIN(RnsvgImageTest)
#include "RnsvgImageTest.moc"
