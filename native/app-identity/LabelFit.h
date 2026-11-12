#pragma once

#include <QList>
#include <QString>
#include <QStringList>

namespace rnqnx::identity {

// One registered launcher face: a family plus its weight, so two faces of the
// same family (Regular and Light) stay distinct.
struct LauncherFace {
  QString family;
  int weight{400};

  QString name() const { return family + QStringLiteral(" ") + QString::number(weight); }
};

// One single-line launcher label box. Widths and sizes are logical pixels.
struct LabelBox {
  QList<LauncherFace> faces;  // Every candidate launcher face; the widest wins.
  qreal pixelSize{0};
  qreal widthPx{0};
  // Kept free at the end of the box so device font rendering, which this host
  // measurement cannot see, has room. A label fits when width <= box - margin.
  qreal marginPx{0};
};

struct LabelMeasurement {
  qreal widthPx{0};       // Widest single-line advance over all faces.
  qreal boxWidthPx{0};
  qreal marginPx{0};
  QString widestFace;
  int lineCount{1};
  bool rightToLeft{false};
  // Qt::mightBeRichText: a QML Text with the default AutoText format would
  // render this label as markup, so its plain-text width is not what shows.
  bool mightBeRichText{false};
  // Code points the launcher faces do not contain. Their width comes from the
  // host's font fallback, which may differ from the device's.
  QList<char32_t> fallbackCodePoints;
  // Longest whole-word prefix that fits and contains no '<' or '&', or empty.
  QString suggestion;

  qreal limitPx() const { return boxWidthPx - marginPx; }
  bool fits() const { return widthPx <= limitPx(); }
  qreal overflowPx() const { return fits() ? 0 : widthPx - limitPx(); }
};

// Lays the text out as plain text on one line with Qt's text engine (shaping,
// bidi and font fallback included, hinting off so results match across hosts)
// and compares the result with the box minus its margin. Throws
// std::invalid_argument for an empty label, a box without fonts, or a
// nonpositive size, or a negative or box-sized margin.
LabelMeasurement measureLabel(const QString& text, const LabelBox& box);

// Registers font files with the application font database and returns their
// faces in file order. Throws std::runtime_error naming the file that cannot
// be loaded.
QList<LauncherFace> registerFontFiles(const QStringList& files);

} // namespace rnqnx::identity
