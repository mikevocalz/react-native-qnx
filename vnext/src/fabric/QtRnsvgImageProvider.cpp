#include "react-native-qnx/fabric/QtRnsvgImageProvider.h"

#include "react-native-qnx/views/DecodedImageCache.h"

#include <image/ImageDecoding.h>

#include <QCoreApplication>
#include <QPointer>
#include <QThread>

#include <utility>

namespace rnqnx::fabric {

using namespace views::rnsvg;

struct QtRnsvgImageProvider::Waiter {
  Completion completion;
  bool cancelled{false};
};

// Destroying it cancels the waiter. It holds the provider weakly: a view tree
// can outlive the host.
class QtRnsvgImageProvider::Handle final : public ImageRequest {
 public:
  Handle(QtRnsvgImageProvider* provider, QString uri, std::shared_ptr<Waiter> waiter)
      : provider_(provider), uri_(std::move(uri)), waiter_(std::move(waiter)) {}
  ~Handle() override {
    waiter_->cancelled = true;
    if (provider_) provider_->forget(uri_, waiter_);
  }

 private:
  QPointer<QtRnsvgImageProvider> provider_;
  QString uri_;
  std::shared_ptr<Waiter> waiter_;
};

namespace {

ImageLoadFailure failureFor(const QString& uri, const QString& error) {
  // QtImageCache cannot tell why a decode failed. For an SVG the usual reason
  // is a Qt install without the qsvg plugin, so say so.
  if (uriNamesSvg(uri) && !svgDecodingAvailable()) return ImageLoadFailure{svgUnavailableMessage()};
  return ImageLoadFailure{error};
}

}  // namespace

QtRnsvgImageProvider& QtRnsvgImageProvider::shared() {
  auto* app = QCoreApplication::instance();
  Q_ASSERT(app != nullptr);
  // The provider and its cache are children of the application object, so they
  // go away with it, before the event loop their network manager needs is gone.
  // The QPointer clears itself when that happens.
  static QPointer<QtRnsvgImageProvider> instance;
  if (!instance) {
    auto* cache = new runtime::QtImageCache();
    cache->setParent(app);
    instance = new QtRnsvgImageProvider(*cache, app);
  }
  return *instance;
}

QtRnsvgImageProvider::QtRnsvgImageProvider(runtime::QtImageCache& cache, QObject* parent)
    : QObject(parent), cache_(cache) {}

// Waiters are dropped without a call. Their handles hold the provider weakly.
QtRnsvgImageProvider::~QtRnsvgImageProvider() = default;

std::unique_ptr<ImageRequest> QtRnsvgImageProvider::load(const QString& uri, Completion completion) {
  Q_ASSERT(QThread::currentThread() == thread());
  if (uri.startsWith(QStringLiteral("data:"), Qt::CaseInsensitive)) {
    completion(decodeDataUri(uri));
    return nullptr;
  }
  // A prefetched or already-loaded URI: no request at all.
  if (QImage cached = views::DecodedImageCache::shared().image(views::DecodedImageCache::key(uri)); !cached.isNull()) {
    completion(std::move(cached));
    return nullptr;
  }
  auto waiter = std::make_shared<Waiter>(Waiter{std::move(completion), false});
  // An entry exists exactly while a fetch for the URI is in flight, so a second
  // element asking for the same URI joins it instead of starting another.
  const auto [entry, startsFetch] = fetches_.try_emplace(uri);
  entry->second.push_back(waiter);
  if (startsFetch) {
    // QtImageCache never calls back after it is destroyed. The guard covers the
    // other order, a provider deleted while the cache lives on.
    QPointer<QtRnsvgImageProvider> self(this);
    cache_.load(uri, {}, [self, uri](QSize, QString error) {
      if (self) self->finish(uri, error);
    });
  }
  return std::make_unique<Handle>(this, uri, std::move(waiter));
}

void QtRnsvgImageProvider::finish(const QString& uri, const QString& error) {
  const auto found = fetches_.find(uri);
  if (found == fetches_.end()) return;
  // Take the list out first: a completion may start or cancel other loads.
  std::vector<std::shared_ptr<Waiter>> waiters = std::move(found->second);
  fetches_.erase(found);

  ImageLoadResult result = ImageLoadFailure{error};
  if (error.isEmpty()) {
    QImage image = views::DecodedImageCache::shared().image(views::DecodedImageCache::key(uri));
    // QtImageCache keeps a response out of the cache when it is marked
    // private or no-store, or when it is larger than the cache budget, and
    // reports only the size to its caller. There is then no image to hand over.
    result = image.isNull()
        ? ImageLoadResult{ImageLoadFailure{QStringLiteral(
              "The image loaded but the decoded-image cache did not keep it "
              "(a no-store or private response, or larger than the cache budget)")}}
        : ImageLoadResult{std::move(image)};
  } else {
    result = failureFor(uri, error);
  }
  for (const auto& waiter : waiters) {
    if (waiter->cancelled) continue;
    waiter->cancelled = true;  // delivered: a late cancel from a handle is a no-op
    Completion completion = std::move(waiter->completion);
    completion(result);
  }
}

void QtRnsvgImageProvider::forget(const QString& uri, const std::shared_ptr<Waiter>& waiter) {
  const auto found = fetches_.find(uri);
  if (found == fetches_.end()) return;
  auto& waiting = found->second;
  for (auto it = waiting.begin(); it != waiting.end(); ++it) {
    if (*it == waiter) {
      waiting.erase(it);
      break;
    }
  }
  // The fetch itself keeps running: its result lands in the shared cache for
  // the next request. The entry stays, empty, until it finishes, so a later
  // request for the same URI joins it.
}

}  // namespace rnqnx::fabric
