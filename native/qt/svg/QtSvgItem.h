#pragma once

#include "SvgDocument.h"

#include <QByteArrayView>
#include <QQuickPaintedItem>
#include <QString>
#include <memory>
#include <optional>

namespace rnqnx::views {

// A bounded, static SVG document in the existing Qt Quick renderer.
// All methods except paint() require the Qt GUI owner thread. paint() is only
// for Qt's render callback while the GUI is blocked, or synchronous owner-thread
// host rendering; it is not a thread-safe arbitrary worker API. Parent ownership
// is conventional Qt ownership. No network, timers, worklets, or SVG DOM API.
class QtSvgItem final : public QQuickPaintedItem {
  Q_OBJECT

 public:
  explicit QtSvgItem(QQuickItem* parent = nullptr);
  ~QtSvgItem() override;

  // A failed replacement clears old pixels; an empty source is a clear.
  // Reassigning the same bounded source emits no duplicate completion.
  // Checks the byte budget before reading and copies accepted-size input into
  // owned storage before parsing; no caller memory is retained after the call.
  bool setDocument(QByteArrayView utf8Xml);
  void clearDocument();
  void setResizeMode(SvgResizeMode mode);
  SvgResizeMode resizeMode() const;
  SvgDocumentState documentState() const;
  std::optional<SvgDocumentInfo> documentInfo() const;
  QString errorString() const;
  void paint(QPainter* painter) override;

 Q_SIGNALS:
  void documentLoaded(QSizeF intrinsicSize, QRectF viewBox);
  void documentFailed(QString message);
  void documentCleared();

 protected:
  bool event(QEvent* event) override;
  void geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) override;
  void itemChange(ItemChange change, const ItemChangeData& data) override;

 private:
  struct State;
  void requireOwner() const;
  void updateTextureSize();
  QThread* const owner_;
  bool affinityLost_{false};
  std::unique_ptr<State> state_;
  SvgResizeMode resizeMode_{SvgResizeMode::Contain};
};

}  // namespace rnqnx::views
