#include "LabelFit.h"

#include <QChar>
#include <QFont>
#include <QFontDatabase>
#include <QRawFont>
#include <QTextBoundaryFinder>
#include <QTextDocument>
#include <QTextLayout>
#include <QTextOption>
#include <algorithm>
#include <stdexcept>

namespace rnqnx::identity {
namespace {

QFont launcherFont(const LauncherFace& face, qreal pixelSize) {
  QFont font(face.family);
  font.setWeight(QFont::Weight(face.weight));
  font.setPixelSize(qRound(pixelSize));
  // Unhinted outlines give the same advances under CoreText and FreeType, so
  // a fit decision does not change with the machine running prepare-native.
  font.setHintingPreference(QFont::PreferNoHinting);
  return font;
}

struct SingleLine {
  qreal widthPx;
  int lineCount;
};

// QQuickText lays labels out with QTextLayout; use the same engine so shaping,
// kerning, bidi reordering and fallback match the launcher's text item.
SingleLine layOut(const QString& text, const QFont& font) {
  QTextLayout layout(text, font);
  QTextOption option;
  option.setWrapMode(QTextOption::NoWrap);
  layout.setTextOption(option);
  layout.beginLayout();
  int lines = 0;
  qreal width = 0;
  for (QTextLine line = layout.createLine(); line.isValid(); line = layout.createLine()) {
    line.setLineWidth(1.0e6);
    width = std::max(width, line.naturalTextWidth());
    ++lines;
  }
  layout.endLayout();
  return {width, lines};
}

bool needsGlyph(char32_t codePoint) {
  const auto category = QChar::category(codePoint);
  return category != QChar::Separator_Space && category != QChar::Other_Format &&
      category != QChar::Mark_NonSpacing && category != QChar::Mark_Enclosing &&
      !(codePoint >= 0xFE00 && codePoint <= 0xFE0F);
}

} // namespace

QList<LauncherFace> registerFontFiles(const QStringList& files) {
  QList<LauncherFace> faces;
  for (const auto& file : files) {
    const int id = QFontDatabase::addApplicationFont(file);
    const auto names = id < 0 ? QStringList{} : QFontDatabase::applicationFontFamilies(id);
    const QRawFont raw(file, 20);
    if (names.isEmpty() || !raw.isValid()) {
      throw std::runtime_error(("Cannot load launcher font " + file).toStdString());
    }
    faces.append({names.first(), raw.weight()});
  }
  return faces;
}

LabelMeasurement measureLabel(const QString& text, const LabelBox& box) {
  if (text.isEmpty()) throw std::invalid_argument("Label text is empty.");
  if (box.faces.isEmpty()) throw std::invalid_argument("Label box has no font.");
  if (!(box.pixelSize > 0) || !(box.widthPx > 0)) {
    throw std::invalid_argument("Label box size and width must be positive.");
  }
  if (box.marginPx < 0 || box.marginPx >= box.widthPx) {
    throw std::invalid_argument("Label margin must be at least 0 and smaller than the box.");
  }
  LabelMeasurement result;
  result.boxWidthPx = box.widthPx;
  result.marginPx = box.marginPx;
  result.mightBeRichText = Qt::mightBeRichText(text);
  result.rightToLeft = text.isRightToLeft();
  QList<QFont> fonts;
  for (const auto& face : box.faces) {
    const auto font = launcherFont(face, box.pixelSize);
    fonts.append(font);
    const auto line = layOut(text, font);
    result.lineCount = std::max(result.lineCount, line.lineCount);
    if (line.widthPx > result.widthPx || result.widestFace.isEmpty()) {
      result.widthPx = std::max(result.widthPx, line.widthPx);
      result.widestFace = face.name();
    }
  }
  for (const char32_t codePoint : text.toUcs4()) {
    if (!needsGlyph(codePoint) || result.fallbackCodePoints.contains(codePoint)) continue;
    for (const auto& font : fonts) {
      if (!QRawFont::fromFont(font).supportsCharacter(codePoint)) {
        result.fallbackCodePoints.append(codePoint);
        break;
      }
    }
  }
  if (!result.fits()) {
    QTextBoundaryFinder words(QTextBoundaryFinder::Word, text);
    for (auto end = words.toNextBoundary(); end > 0; end = words.toNextBoundary()) {
      if (!(words.boundaryReasons() & QTextBoundaryFinder::EndOfItem)) continue;
      const auto prefix = text.left(end).trimmed();
      if (prefix.isEmpty() || prefix == text) continue;
      // Never offer a markup fragment such as "<b>Notes" as a label.
      if (prefix.contains(QLatin1Char('<')) || prefix.contains(QLatin1Char('&'))) continue;
      qreal width = 0;
      for (const auto& font : fonts) width = std::max(width, layOut(prefix, font).widthPx);
      if (width > result.limitPx()) break;
      result.suggestion = prefix;
    }
  }
  return result;
}

} // namespace rnqnx::identity
