#include "react-native-qnx/fabric/QtVideoBridge.h"

#include <QtVideoItem.h>
#include <react/renderer/components/RnqnxQtVideo/EventEmitters.h>
#include <react/renderer/components/RnqnxQtVideo/Props.h>

#include <utility>

namespace rnqnx::fabric {

QtVideoBridge::QtVideoBridge(views::QtVideoItem& item) : QObject(&item), item_(item) {
  using Emitter = facebook::react::QnxQtVideoEventEmitter;
  QObject::connect(&item, &views::QtVideoItem::videoLoaded, this,
      [this](double duration, int width, int height, const QString& device) {
        if (auto emitter = emitter_) emitter->onVideoLoad(Emitter::OnVideoLoad{duration, double(width), double(height), device.toStdString()});
      });
  QObject::connect(&item, &views::QtVideoItem::videoProgress, this,
      [this](double current, double playable) {
        if (auto emitter = emitter_) emitter->onVideoProgress(Emitter::OnVideoProgress{current, playable});
      });
  QObject::connect(&item, &views::QtVideoItem::videoEnded, this, [this]() {
    if (auto emitter = emitter_) emitter->onVideoEnd(Emitter::OnVideoEnd{true});
  });
  QObject::connect(&item, &views::QtVideoItem::videoFailed, this,
      [this](const QString& message, int code) {
        if (auto emitter = emitter_) emitter->onVideoError(Emitter::OnVideoError{message.toStdString(), code});
      });
  QObject::connect(&item, &views::QtVideoItem::videoBuffering, this, [this](bool buffering) {
    if (auto emitter = emitter_) emitter->onVideoBuffer(Emitter::OnVideoBuffer{buffering});
  });
  QObject::connect(&item, &views::QtVideoItem::videoSeeked, this,
      [this](double current, double target) {
        if (auto emitter = emitter_) emitter->onVideoSeek(Emitter::OnVideoSeek{current, target});
      });
  QObject::connect(&item, &views::QtVideoItem::videoFullscreenToggled, this, [this](bool on) {
    if (auto emitter = emitter_) emitter->onVideoFullscreen(Emitter::OnVideoFullscreen{on});
  });
}

void QtVideoBridge::apply(
    const facebook::react::QnxQtVideoProps& props,
    std::shared_ptr<const facebook::react::QnxQtVideoEventEmitter> emitter) {
  emitter_ = std::move(emitter);
  const Applied* prev = last_ ? &*last_ : nullptr;
  if (!prev || prev->source != props.source) item_.setSource(QString::fromStdString(props.source));
  if (!prev || prev->poster != props.poster) item_.setPoster(QString::fromStdString(props.poster));
  if (!prev || prev->audioDevice != props.audioDevice) item_.setAudioDevice(QString::fromStdString(props.audioDevice));
  if (!prev || prev->paused != props.paused) item_.setPaused(props.paused);
  if (!prev || prev->muted != props.muted) item_.setMuted(props.muted);
  if (!prev || prev->volume != props.volume) item_.setVolume(props.volume);
  if (!prev || prev->rate != props.rate) item_.setRate(props.rate);
  if (!prev || prev->repeat != props.repeat) item_.setRepeat(props.repeat);
  if (!prev || prev->controls != props.controls) item_.setControls(props.controls);
  if (!prev || prev->fullscreen != props.fullscreen) item_.setFullscreen(props.fullscreen);
  if (!prev || prev->resizeMode != static_cast<int>(props.resizeMode)) {
    using Mode = facebook::react::QnxQtVideoResizeMode;
    switch (props.resizeMode) {
      case Mode::Contain: item_.setResizeMode(views::VideoResizeMode::Contain); break;
      case Mode::Cover: item_.setResizeMode(views::VideoResizeMode::Cover); break;
      case Mode::Stretch: item_.setResizeMode(views::VideoResizeMode::Stretch); break;
    }
  }
  last_ = Applied{props.source, props.poster, props.audioDevice, props.paused, props.muted, props.repeat,
                  props.controls, props.fullscreen, props.volume, props.rate,
                  static_cast<int>(props.resizeMode)};
}

} // namespace rnqnx::fabric
