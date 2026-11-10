#include "react-native-qnx/runtime/QnxLinkingModule.h"

#include <react/bridging/Promise.h>
#include <folly/dynamic.h>
#include <QFileOpenEvent>
#include <QGuiApplication>
#include <QPointer>
#include <QRegularExpression>
#include <QSet>
#include <QThread>
#include <QUrl>
#include <cmath>
#include <stdexcept>

namespace rnqnx::runtime {
namespace jsi = facebook::jsi;
namespace react = facebook::react;
namespace {
constexpr qsizetype maximumURLBytes = 8192;
constexpr std::size_t maximumPendingEvents = 32;

std::optional<std::string> acceptedURL(const QUrl& url, const QSet<QString>& schemes) {
  if (!url.isValid() || url.isEmpty() || url.isRelative() ||
      !schemes.contains(url.scheme().toLower()) || !url.userInfo().isEmpty()) return std::nullopt;
  const auto encoded = url.toEncoded(QUrl::FullyEncoded);
  if (encoded.isEmpty() || encoded.size() > maximumURLBytes) return std::nullopt;
  return encoded.toStdString();
}
}  // namespace

struct QnxLinkingModule::Impl final : QObject, std::enable_shared_from_this<Impl> {
  explicit Impl(std::shared_ptr<react::CallInvoker> invoker)
      : invoker(std::move(invoker)) {}
  static std::weak_ptr<Impl> receiver;

  void requireOwner() const noexcept {
    if (QThread::currentThread() != thread()) std::terminate();
  }
  bool eventFilter(QObject* watched, QEvent* event) override {
    requireOwner();
    if (!active || !configured || watched != application || event->type() != QEvent::FileOpen) {
      return false;
    }
    const auto value = acceptedURL(static_cast<QFileOpenEvent*>(event)->url(), schemes);
    if (!value || pending >= maximumPendingEvents) return false;
    ++pending;
    const std::weak_ptr<Impl> weak = shared_from_this();
    invoker->invokeAsync([weak, url = *value](jsi::Runtime& runtime) {
      const auto state = weak.lock();
      if (!state || !state->active) return;
      state->requireOwner();
      --state->pending;
      // Ordinary RN non-iOS Linking uses the shared emitter without calling
      // native addListener. Do not gate delivery on a native subscription count.
      auto emitter = runtime.global().getProperty(runtime, "__rctDeviceEventEmitter");
      if (!emitter.isObject()) return;
      auto object = emitter.asObject(runtime);
      auto payload = jsi::Object(runtime);
      payload.setProperty(runtime, "url", jsi::String::createFromUtf8(runtime, url));
      object.getPropertyAsFunction(runtime, "emit").callWithThis(runtime, object,
          jsi::String::createFromAscii(runtime, "url"), std::move(payload));
    });
    event->accept();
    return true;
  }
  std::shared_ptr<react::CallInvoker> invoker;
  QPointer<QGuiApplication> application;
  QSet<QString> schemes;
  std::optional<std::string> initialURL;
  std::size_t pending{0};
  bool active{true};
  bool configured{false};
};
std::weak_ptr<QnxLinkingModule::Impl> QnxLinkingModule::Impl::receiver;

QnxLinkingModule::QnxLinkingModule(std::shared_ptr<react::CallInvoker> invoker)
    : react::NativeLinkingManagerCxxSpec<QnxLinkingModule>(invoker) {
  auto* app = qobject_cast<QGuiApplication*>(QCoreApplication::instance());
  if (!app || QThread::currentThread() != app->thread() || !invoker) {
    throw std::logic_error("Linking requires the QGuiApplication owner and runtime invoker");
  }
  impl_ = std::make_shared<Impl>(std::move(invoker));
}
QnxLinkingModule::~QnxLinkingModule() { invalidate(); }

void QnxLinkingModule::configure(const std::vector<std::string>& requested,
    const std::optional<std::string>& initialURL) {
  if (!impl_) throw std::logic_error("Linking observation is closed");
  impl_->requireOwner();
  if (impl_->configured) throw std::logic_error("Linking launch configuration is immutable");
  if (auto receiver = Impl::receiver.lock(); receiver && receiver.get() != impl_.get()) {
    throw std::logic_error("Another runtime owns this application's URL receiver");
  }
  if (requested.empty() || requested.size() > 16) {
    throw std::invalid_argument("Linking requires between one and sixteen custom app schemes");
  }
  const QRegularExpression syntax(QStringLiteral("^[a-z][a-z0-9+.-]*$"));
  const QSet<QString> reserved{QStringLiteral("http"), QStringLiteral("https"),
      QStringLiteral("file"), QStringLiteral("ftp"), QStringLiteral("data"),
      QStringLiteral("javascript"), QStringLiteral("about"), QStringLiteral("qrc"),
      QStringLiteral("mailto"), QStringLiteral("tel"), QStringLiteral("sms")};
  QSet<QString> schemes;
  for (const auto& requestedScheme : requested) {
    const auto scheme = QString::fromStdString(requestedScheme).toLower();
    if (scheme.size() > 64 || !syntax.match(scheme).hasMatch() || reserved.contains(scheme)) {
      throw std::invalid_argument("Linking requires a valid custom application scheme");
    }
    schemes.insert(scheme);
  }
  std::optional<std::string> launch;
  if (initialURL) {
    if (initialURL->size() > static_cast<std::size_t>(maximumURLBytes)) {
      throw std::invalid_argument("Initial URL exceeds the application URL limit");
    }
    launch = acceptedURL(QUrl::fromEncoded(QByteArray::fromStdString(*initialURL), QUrl::StrictMode), schemes);
    if (!launch) throw std::invalid_argument("Initial URL must match the application's custom schemes");
  }
  auto* app = qobject_cast<QGuiApplication*>(QCoreApplication::instance());
  if (!app) throw std::logic_error("Linking requires a live QGuiApplication");
  impl_->schemes = std::move(schemes);
  impl_->initialURL = std::move(launch);
  impl_->application = app;
  impl_->configured = true;
  Impl::receiver = impl_;
  app->installEventFilter(impl_.get());
}

void QnxLinkingModule::invalidate() noexcept {
  if (!impl_) return;
  impl_->requireOwner();
  if (impl_->application) impl_->application->removeEventFilter(impl_.get());
  impl_->active = false;
  if (const auto receiver = Impl::receiver.lock(); receiver.get() == impl_.get()) {
    Impl::receiver.reset();
  }
  // Destroy QObject on the Qt owner, even if the module is later GC-released
  // off-owner. Queued deliveries hold only weak state, never JSI values.
  impl_.reset();
}

jsi::Value QnxLinkingModule::getInitialURL(jsi::Runtime& runtime) {
  react::AsyncPromise<folly::dynamic> promise(runtime, jsInvoker_);
  auto result = promise.get(runtime);
  if (!impl_) {
    promise.reject("Linking observation is closed");
    return result;
  }
  impl_->requireOwner();
  promise.resolve(impl_->initialURL ? folly::dynamic(*impl_->initialURL) : folly::dynamic(nullptr));
  return result;
}
jsi::Value QnxLinkingModule::unavailable(jsi::Runtime& runtime, const char* operation) {
  react::AsyncPromise<folly::dynamic> promise(runtime, jsInvoker_);
  auto result = promise.get(runtime);
  promise.reject(std::string("Linking.") + operation +
      " is unavailable: no scoped platform URL/settings dispatcher is integrated");
  return result;
}
jsi::Value QnxLinkingModule::canOpenURL(jsi::Runtime& runtime, jsi::String) {
  return unavailable(runtime, "canOpenURL");
}
jsi::Value QnxLinkingModule::openURL(jsi::Runtime& runtime, jsi::String) {
  return unavailable(runtime, "openURL");
}
jsi::Value QnxLinkingModule::openSettings(jsi::Runtime& runtime) {
  return unavailable(runtime, "openSettings");
}
void QnxLinkingModule::addListener(jsi::Runtime& runtime, jsi::String event) {
  if (!impl_) throw jsi::JSError(runtime, "Linking observation is closed");
  impl_->requireOwner();
  if (event.utf8(runtime) != "url") throw jsi::JSError(runtime, "Linking supports only url events");
  // RN NativeEventEmitter owns subscription lifetime; this is its required
  // framework hook. The process URL receiver remains owned by the host.
}
void QnxLinkingModule::removeListeners(jsi::Runtime& runtime, double count) {
  if (!impl_) return;
  impl_->requireOwner();
  if (!std::isfinite(count) || count < 0 || std::floor(count) != count || count > 9007199254740991.0) {
    throw jsi::JSError(runtime, "Linking removal count must be a nonnegative safe integer");
  }
}
}  // namespace rnqnx::runtime
