#include "QtVideoItem.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QQmlComponent>
#include <QQmlEngine>

namespace rnqnx::views {
namespace {

// QnxVideo.qml is compiled into the binary (see CMakeLists.txt) so a deployed
// app needs no loose QML files, only the QtMultimedia QML module.
const char* const kQmlUrl = "qrc:/react-native-qnx/video/QnxVideo.qml";

}  // namespace

QtVideoItem::QtVideoItem(QQmlEngine& engine, QQuickItem* parent) : QQuickItem(parent) {
  setClip(true);
  QQmlComponent component(&engine, QUrl(QString::fromLatin1(kQmlUrl)));
  if (component.isError()) {
    fail(component.errorString());
    return;
  }
  QObject* object = component.create();
  inner_ = qobject_cast<QQuickItem*>(object);
  if (inner_ == nullptr) {
    error_ = component.errorString();
    delete object;
    fail(error_.isEmpty() ? QStringLiteral("QnxVideo.qml is not an Item") : error_);
    return;
  }
  QQmlEngine::setObjectOwnership(inner_, QQmlEngine::CppOwnership);
  inner_->setParentItem(this);
  inner_->setSize(size());

  connect(inner_, SIGNAL(loaded(double,int,int,QString)), this, SIGNAL(videoLoaded(double,int,int,QString)));
  connect(inner_, SIGNAL(progress(double,double)), this, SIGNAL(videoProgress(double,double)));
  connect(inner_, SIGNAL(ended()), this, SIGNAL(videoEnded()));
  connect(inner_, SIGNAL(failed(QString,int)), this, SIGNAL(videoFailed(QString,int)));
  connect(inner_, SIGNAL(buffering(bool)), this, SIGNAL(videoBuffering(bool)));
  connect(inner_, SIGNAL(seeked(double,double)), this, SIGNAL(videoSeeked(double,double)));
  connect(inner_, SIGNAL(fullscreenToggled(bool)), this, SIGNAL(videoFullscreenToggled(bool)));
}

QtVideoItem::~QtVideoItem() {
  // The inner item and its MediaPlayer are children of this item; stop
  // playback first so the audio sink closes before the engine goes away.
  if (inner_ != nullptr) {
    QMetaObject::invokeMethod(inner_, "pause");
    inner_->setProperty("source", QUrl());
  }
}

void QtVideoItem::fail(const QString& message) {
  error_ = message;
  Q_EMIT videoFailed(message, -1);
}

void QtVideoItem::geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) {
  QQuickItem::geometryChange(newGeometry, oldGeometry);
  if (inner_ != nullptr) inner_->setSize(newGeometry.size());
}

QUrl QtVideoItem::resolveSource(const QString& source, const QString& baseDirectory) {
  if (source.isEmpty()) return QUrl();
  const QUrl parsed(source);
  const QString scheme = parsed.scheme();
  if (scheme == QLatin1String("http") || scheme == QLatin1String("https") ||
      scheme == QLatin1String("file") || scheme == QLatin1String("qrc")) {
    return parsed;
  }
  if (QDir::isAbsolutePath(source)) return QUrl::fromLocalFile(source);
  // Bundled assets (`assets/media/clip.mp4`) live next to the executable.
  return QUrl::fromLocalFile(QDir(baseDirectory).absoluteFilePath(source));
}

void QtVideoItem::setSource(const QString& source) {
  if (inner_ == nullptr) return;
  if (baseDirectory_.isEmpty()) baseDirectory_ = QCoreApplication::applicationDirPath();
  inner_->setProperty("source", resolveSource(source, baseDirectory_));
}

void QtVideoItem::setPaused(bool paused) {
  paused_ = paused;
  if (inner_ != nullptr) inner_->setProperty("paused", paused);
}
void QtVideoItem::setMuted(bool muted) { if (inner_) inner_->setProperty("muted", muted); }
void QtVideoItem::setVolume(double volume) { if (inner_) inner_->setProperty("volume", qBound(0.0, volume, 1.0)); }
void QtVideoItem::setRate(double rate) { if (inner_) inner_->setProperty("rate", qBound(0.25, rate, 4.0)); }
void QtVideoItem::setRepeat(bool repeat) { if (inner_) inner_->setProperty("repeat", repeat); }
void QtVideoItem::setPoster(const QString& poster) {
  if (inner_ == nullptr) return;
  if (baseDirectory_.isEmpty()) baseDirectory_ = QCoreApplication::applicationDirPath();
  inner_->setProperty("poster", resolveSource(poster, baseDirectory_));
}
void QtVideoItem::setControls(bool controls) { if (inner_) inner_->setProperty("controls", controls); }
void QtVideoItem::setAudioDevice(const QString& device) { if (inner_) inner_->setProperty("audioDevice", device); }
void QtVideoItem::setFullscreen(bool fullscreen) { if (inner_) inner_->setProperty("fullscreen", fullscreen); }

void QtVideoItem::setResizeMode(VideoResizeMode mode) {
  if (inner_ == nullptr) return;
  // VideoOutput.fillMode: Stretch = 0, PreserveAspectFit = 1, PreserveAspectCrop = 2.
  const int fill = mode == VideoResizeMode::Cover ? 2 : mode == VideoResizeMode::Stretch ? 0 : 1;
  inner_->setProperty("fillMode", fill);
}

void QtVideoItem::seek(double seconds) {
  if (inner_ != nullptr) QMetaObject::invokeMethod(inner_, "seekTo", Q_ARG(QVariant, seconds));
}
void QtVideoItem::play() {
  if (inner_ == nullptr) return;
  paused_ = false;
  inner_->setProperty("paused", false);
  QMetaObject::invokeMethod(inner_, "play");
}
void QtVideoItem::pause() {
  if (inner_ == nullptr) return;
  paused_ = true;
  inner_->setProperty("paused", true);
  QMetaObject::invokeMethod(inner_, "pause");
}

}  // namespace rnqnx::views
