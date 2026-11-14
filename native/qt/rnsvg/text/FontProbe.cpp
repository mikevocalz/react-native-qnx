#include "FontProbe.h"

#include <QFontMetricsF>
#include <QRawFont>
#include <algorithm>
#include <cmath>

namespace rnqnx::views::rnsvg::text {

namespace {

QFont buildFont(const Font& f, bool ligatures) {
  QFont font;
  if (!f.family.isEmpty()) font.setFamily(f.family);
  font.setPixelSize(int(kProbeSize));
  font.setWeight(QFont::Weight(std::clamp(f.weight, 1, 1000)));
  font.setItalic(f.italic);
  font.setHintingPreference(QFont::PreferNoHinting);
  font.setStretch(int(std::lround(std::clamp(f.stretchPercent, 1.0, 400.0))));
  // A kerning value replaces the font's kerning (SVG 1.1 `kerning`).
  font.setKerning(!f.kerning.has_value());
  if (!ligatures) {
    font.setFeature(QFont::Tag("liga"), 0);
    font.setFeature(QFont::Tag("clig"), 0);
  }
  for (const FontFeature& feature : f.features) {
    if (const auto tag = QFont::Tag::fromString(feature.tag)) font.setFeature(*tag, feature.value);
  }
  return font;
}

qint16 be16(const QByteArray& table, int offset) {
  if (offset < 0 || offset + 2 > table.size()) return 0;
  return qint16((quint8(table[offset]) << 8) | quint8(table[offset + 1]));
}

FontMetrics readMetrics(const QFont& font) {
  const QFontMetricsF fm(font);
  FontMetrics m;
  m.ascent = fm.ascent() / kProbeSize;
  m.descent = fm.descent() / kProbeSize;
  m.leading = fm.leading() / kProbeSize;
  m.xHeight = fm.xHeight() / kProbeSize;
  // Qt's own line metrics are the fallback for fonts without the tables.
  m.underlinePos = fm.underlinePos() / kProbeSize;
  m.lineThickness = std::max(fm.lineWidth(), 1.0) / kProbeSize;
  m.strikePos = fm.strikeOutPos() / kProbeSize;
  const QRawFont raw = QRawFont::fromFont(font);
  if (!raw.isValid()) return m;
  const QByteArray head = raw.fontTable("head");
  const double unitsPerEm = head.size() >= 20 ? std::abs(be16(head, 18)) : 0;
  if (unitsPerEm <= 0) return m;
  // OS/2 holds the designer's subscript and superscript offsets, which
  // react-native-svg's `baselineShift: sub | super` reads too, and the strike-through position.
  const QByteArray os2 = raw.fontTable("OS/2");
  if (os2.size() >= 30) {
    m.subscriptDrop = be16(os2, 16) / unitsPerEm;
    m.superscriptRise = be16(os2, 24) / unitsPerEm;
    m.strikePos = be16(os2, 28) / unitsPerEm;
  }
  const QByteArray post = raw.fontTable("post");
  if (post.size() >= 12) {
    m.underlinePos = -be16(post, 8) / unitsPerEm;
    m.lineThickness = std::max(be16(post, 10) / unitsPerEm, 0.0);
  }
  return m;
}

double advanceOfM(const QFont& font) { return QFontMetricsF(font).horizontalAdvance(QLatin1Char('M')); }

// Slant FreeType's synthetic oblique uses (12 degrees).
constexpr double kSyntheticShear = 0.2126;

double stemWidth(const QFont& font) {
  const QRawFont raw = QRawFont::fromFont(font);
  if (!raw.isValid()) return 0;
  const QList<quint32> glyphs = raw.glyphIndexesForString(QStringLiteral("l"));
  return glyphs.isEmpty() ? 0 : raw.pathForGlyph(glyphs.front()).boundingRect().width();
}

}  // namespace

const Probe& ProbeCache::get(const Font& font, bool ligatures) {
  QString featureKey;
  for (const FontFeature& f : font.features) featureKey += f.tag + QLatin1Char(':') + QString::number(f.value) + QLatin1Char(';');
  const Key key{font.family, font.weight, font.italic, int(std::lround(font.stretchPercent * 10)),
                font.kerning.has_value(), ligatures, featureKey};
  auto it = probes_.find(key);
  if (it != probes_.end()) return *it->second;

  auto probe = std::make_unique<Probe>();
  probe->font = buildFont(font, ligatures);
  probe->metrics = readMetrics(probe->font);

  // Qt applies font-stretch only when the family offers a width variant or its
  // engine scales the face; measure instead of assuming, and finish the job.
  if (std::abs(font.stretchPercent - 100) > 0.5) {
    Font plain = font;
    plain.stretchPercent = 100;
    const double reference = advanceOfM(buildFont(plain, ligatures));
    const double actual = advanceOfM(probe->font);
    if (reference > 0 && actual > 0) probe->stretchFix = (font.stretchPercent / 100) / (actual / reference);
  }
  // The same check for italic: a real italic face, or Qt's own synthetic
  // slant, widens the stem box of "l"; if it does not, slant the outlines here.
  if (font.italic) {
    Font upright = font;
    upright.italic = false;
    const double slanted = stemWidth(probe->font);
    const double plain = stemWidth(buildFont(upright, ligatures));
    if (slanted > 0 && plain > 0 && slanted < plain * 1.03) probe->italicShear = kSyntheticShear;
  }
  // Qt draws a bold request on a family without a bold face by emboldening at
  // raster time, which glyph outlines do not see. Spot that case (the stem is
  // as thin as at weight 400) and thicken the outlines the way FreeType does.
  if (font.weight >= 600) {
    Font regular = font;
    regular.weight = 400;
    const double boldStem = stemWidth(probe->font);
    const double regularStem = stemWidth(buildFont(regular, ligatures));
    if (boldStem > 0 && regularStem > 0 && boldStem < regularStem * 1.04) probe->emboldenPx = kProbeSize / 24;
  }
  return *probes_.emplace(key, std::move(probe)).first->second;
}

}  // namespace rnqnx::views::rnsvg::text
