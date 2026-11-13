#pragma once

#include <QQuickItem>
#include <QString>
#include <QUrl>

class QQmlComponent;
class QQmlEngine;

namespace rnqnx::views {

enum class VideoResizeMode { Contain, Cover, Stretch };

// A video player in the existing Qt Quick scene graph. The item owns one QML
// child (QnxVideo.qml: MediaPlayer + AudioOutput + VideoOutput + control bar)
// and mirrors the React props onto it. Frames are presented by VideoOutput
// as a scene-graph texture, so the item composes with sibling React views,
// the window chrome and clipping like any other item.
// All methods require the Qt GUI thread. Playback needs the Qt Multimedia QML
// module at runtime; when it is missing the item stays empty and emits
// videoFailed once, instead of aborting the host.
class QtVideoItem final : public QQuickItem {
  Q_OBJECT

 public:
  explicit QtVideoItem(QQmlEngine& engine, QQuickItem* parent = nullptr);
  ~QtVideoItem() override;

  bool isReady() const { return inner_ != nullptr; }
  QString errorString() const { return error_; }

  void setSource(const QString& source);
  void setPaused(bool paused);
  void setMuted(bool muted);
  void setVolume(double volume);
  void setRate(double rate);
  void setRepeat(bool repeat);
  void setResizeMode(VideoResizeMode mode);
  void setPoster(const QString& poster);
  void setControls(bool controls);
  void setFullscreen(bool fullscreen);

  void seek(double seconds);
  void play();
  void pause();

  // Maps a React `source` string to a URL: http(s)/file/qrc URLs pass through,
  // absolute paths become file URLs, and anything else resolves under base.
  static QUrl resolveSource(const QString& source, const QString& baseDirectory);

 Q_SIGNALS:
  void videoLoaded(double duration, int width, int height);
  void videoProgress(double currentTime, double playableDuration);
  void videoEnded();
  void videoFailed(const QString& message, int code);
  void videoBuffering(bool isBuffering);
  void videoSeeked(double currentTime, double seekTime);
  void videoFullscreenToggled(bool fullscreen);

 protected:
  void geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) override;

 private:
  void fail(const QString& message);
  QQuickItem* inner_{nullptr};
  QString error_;
  QString baseDirectory_;
  bool paused_{false};
};

}  // namespace rnqnx::views
