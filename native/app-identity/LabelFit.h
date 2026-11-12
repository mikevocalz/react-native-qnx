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
};

struct LabelMeasurement {
  qreal widthPx{0};       // Widest single-line advance over all faces.
  qreal boxWidthPx{0};
  QString widestFace;
  int lineCount{1};
  bool rightToLeft{false};
  // Code points the launcher faces do not contain. Their width comes from the
  // host's font fallback, which may differ from the device's.
  QList<char32_t> fallbackCodePoints;
  // Longest whole-word prefix that fits, or empty when none does.
  QString suggestion;

  bool fits() const { return widthPx <= boxWidthPx; }
  qreal overflowPx() const { return fits() ? 0 : widthPx - boxWidthPx; }
};

// Lays the text out on one line with Qt's text engine (shaping, bidi and font
// fallback included) and compares the result with the box. Throws
// std::invalid_argument for an empty label, a box without fonts, or a
// nonpositive size.
LabelMeasurement measureLabel(const QString& text, const LabelBox& box);

// Registers font files with the application font database and returns their
// faces in file order. Throws std::runtime_error naming the file that cannot
// be loaded.
QList<LauncherFace> registerFontFiles(const QStringList& files);

} // namespace rnqnx::identity
