#include "ImageRenderer.h"

#include "../RnsvgItems.h"
#include "Scene.h"

#include <QBuffer>
#include <QImageReader>
#include <QPainter>
#include <QUrl>

#include <algorithm>

namespace rnqnx::views::rnsvg {
namespace {

constexpr qsizetype kMaxDataUriBytes = 16 * 1024 * 1024;
constexpr int kMaxImageSide = 8192;

}  // namespace

QImage ImageRenderer::decodeLocal(const QString& href) {
  if (href.isEmpty()) return {};
  if (href.startsWith(QStringLiteral("data:"))) {
    const int comma = href.indexOf(QLatin1Char(','));
    if (comma < 0 || href.size() > kMaxDataUriBytes) return {};
    const QByteArray payload = href.mid(comma + 1).toUtf8();
    const bool base64 = href.left(comma).contains(QStringLiteral(";base64"));
    QBuffer buffer;
    buffer.setData(base64 ? QByteArray::fromBase64(payload) : QByteArray::fromPercentEncoding(payload));
    buffer.open(QIODevice::ReadOnly);
    QImageReader reader(&buffer);
    const QSize size = reader.size();
    if (size.isValid() && (size.width() > kMaxImageSide || size.height() > kMaxImageSide)) return {};
    return reader.read();
  }
  QString path;
  if (href.startsWith(QStringLiteral("file://"))) path = QUrl(href).toLocalFile();
  else if (href.startsWith(QLatin1Char('/'))) path = href;
  if (path.isEmpty()) return {};
  QImageReader reader(path);
  const QSize size = reader.size();
  if (size.isValid() && (size.width() > kMaxImageSide || size.height() > kMaxImageSide)) return {};
  return reader.read();
}

void ImageRenderer::draw(const RnsvgElementItem& e, const Frame& f, int depth) {
  const auto& p = e.props();
  if (p.image.isNull()) return;
  const Units u(f);
  const QRectF box(u.resolve(p.x, Axis::X), u.resolve(p.y, Axis::Y),
                   u.resolve(p.width, Axis::X, p.image.width()), u.resolve(p.height, Axis::Y, p.image.height()));
  if (!(box.width() > 0) || !(box.height() > 0)) return;
  // preserveAspectRatio for the image inside its box.
  const QSizeF src = p.image.size();
  QRectF dest = box;
  QRectF source(QPointF(0, 0), src);
  if (p.imageMeetOrSlice != 2 && p.imageAlign != QStringLiteral("none")) {
    const double sx = box.width() / src.width(), sy = box.height() / src.height();
    const double s = p.imageMeetOrSlice == 1 ? std::max(sx, sy) : std::min(sx, sy);
    const QSizeF scaled = src * s;
    const QString& a = p.imageAlign;
    const double fx = a.startsWith(QStringLiteral("xMin")) ? 0 : a.startsWith(QStringLiteral("xMax")) ? 1 : 0.5;
    const double fy = a.endsWith(QStringLiteral("YMin")) ? 0 : a.endsWith(QStringLiteral("YMax")) ? 1 : 0.5;
    const QRectF fitted(box.x() + (box.width() - scaled.width()) * fx,
                        box.y() + (box.height() - scaled.height()) * fy, scaled.width(), scaled.height());
    if (p.imageMeetOrSlice == 1) {
      source = QRectF((fitted.x() < box.x() ? (box.x() - fitted.x()) / s : 0),
                      (fitted.y() < box.y() ? (box.y() - fitted.y()) / s : 0), box.width() / s, box.height() / s);
    } else {
      dest = fitted;
    }
  }
  if (scene_.hitMode()) {
    QPainterPath path;
    path.addRect(dest);
    Frame hf = f;
    hf.style.fill = Brush{Brush::Type::Color, Qt::black, {}};
    scene_.hitTester().testPath(&e, path, hf, PointerMode::parse(p.pointerEvents));
    return;
  }
  EffectBoxes boxes;
  boxes.objectBox = [dest] { return dest; };
  boxes.paintBox = [dest] { return dest; };
  scene_.effects().paint(e, f, boxes, false, depth, [&] {
    scene_.painter()->drawImage(dest, p.image, source);
  });
}

}  // namespace rnqnx::views::rnsvg
