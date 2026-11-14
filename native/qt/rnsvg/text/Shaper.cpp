#include "Shaper.h"

#include <QGlyphRun>
#include <QPainterPathStroker>
#include <QRawFont>
#include <QTextLayout>
#include <QTextLine>
#include <QTextOption>
#include <QTransform>
#include <algorithm>
#include <cmath>

namespace rnqnx::views::rnsvg::text {

namespace {

// A glyph as the run reports it, before marks are folded into their base.
struct RawGlyph {
  int stringIndex{0};
  double advance{0};  // probe pixels
  QPainterPath outline;
  QPointF position;  // line coordinates, baseline-relative y
  bool zeroAdvance{false};
};

// Advances come from the pen positions so kerning is included. A glyph that
// follows with zero advance (a mark) is skipped when looking for the next pen
// position; the mark's own offset is not a pen move.
std::vector<RawGlyph> collectRun(const QGlyphRun& run, double baselineY) {
  const QList<quint32> indexes = run.glyphIndexes();
  const QList<QPointF> positions = run.positions();
  const QList<qsizetype> strings = run.stringIndexes();
  const QRawFont raw = run.rawFont();
  const QList<QPointF> nominal = raw.advancesForGlyphIndexes(indexes);
  const int n = int(std::min({indexes.size(), positions.size(), strings.size(), nominal.size()}));
  const bool rtl = run.isRightToLeft();

  std::vector<RawGlyph> glyphs(n);
  for (int i = 0; i < n; ++i) {
    RawGlyph& g = glyphs[i];
    g.stringIndex = int(strings[i]);
    g.position = QPointF(positions[i].x(), positions[i].y() - baselineY);
    g.outline = raw.pathForGlyph(indexes[i]);
    g.zeroAdvance = nominal[i].x() == 0;
    g.advance = nominal[i].x();
    if (g.zeroAdvance) continue;
    // Nearest pen neighbour in reading direction: later glyphs for left to
    // right, earlier ones for right to left (their positions decrease).
    int j = rtl ? i - 1 : i + 1;
    while (j >= 0 && j < n && nominal[j].x() == 0) j += rtl ? -1 : 1;
    if (j >= 0 && j < n) {
      const double gap = positions[j].x() - positions[i].x();
      if (gap > 0) g.advance = gap;
    }
  }
  return glyphs;
}

bool isWordSeparator(QChar c) { return c == QLatin1Char(' ') || c == QChar(0x00A0); }

}  // namespace

std::vector<ShapedGlyph> shapeText(const QString& text, const Probe& probe) {
  std::vector<ShapedGlyph> out;
  if (text.isEmpty()) return out;

  QTextLayout layout(text, probe.font);
  QTextOption option;
  option.setTextDirection(Qt::LeftToRight);
  option.setWrapMode(QTextOption::NoWrap);
  layout.setTextOption(option);
  layout.beginLayout();
  QTextLine line = layout.createLine();
  line.setLineWidth(1e7);
  layout.endLayout();
  if (!line.isValid()) return out;

  const double baselineY = line.ascent();
  const QTextLayout::GlyphRunRetrievalFlags flags(QTextLayout::RetrieveGlyphIndexes |
                                                  QTextLayout::RetrieveGlyphPositions |
                                                  QTextLayout::RetrieveStringIndexes);
  struct Cluster {
    int stringIndex;
    double advance;
    QPainterPath outline;  // probe pixels, origin at the base glyph's pen point
    double penX;
  };
  std::vector<Cluster> clusters;  // visual order
  for (const QGlyphRun& run : line.glyphRuns(-1, -1, flags)) {
    std::vector<Cluster> runClusters;
    for (const RawGlyph& g : collectRun(run, baselineY)) {
      if (g.zeroAdvance && !runClusters.empty()) {
        // A mark folds into the previous glyph, offset from that glyph's pen point.
        Cluster& base = runClusters.back();
        base.outline.addPath(g.outline.translated(g.position.x() - base.penX, g.position.y()));
        continue;
      }
      Cluster c;
      c.stringIndex = g.stringIndex;
      c.advance = g.advance;
      c.penX = g.position.x();
      c.outline = g.outline.translated(0, g.position.y());
      runClusters.push_back(std::move(c));
    }
    if (run.isRightToLeft()) std::reverse(runClusters.begin(), runClusters.end());
    for (Cluster& c : runClusters) clusters.push_back(std::move(c));
  }

  std::vector<int> firsts;
  firsts.reserve(clusters.size());
  for (const Cluster& c : clusters) firsts.push_back(c.stringIndex);
  std::sort(firsts.begin(), firsts.end());
  firsts.erase(std::unique(firsts.begin(), firsts.end()), firsts.end());

  const double scale = 1.0 / kProbeSize;
  QTransform toEm;
  toEm.scale(scale * probe.stretchFix, scale);
  out.reserve(clusters.size());
  for (const Cluster& c : clusters) {
    ShapedGlyph g;
    g.firstChar = c.stringIndex;
    const auto next = std::upper_bound(firsts.begin(), firsts.end(), c.stringIndex);
    g.charCount = (next == firsts.end() ? int(text.size()) : *next) - c.stringIndex;
    g.advance = c.advance * scale * probe.stretchFix;
    QPainterPath outline = c.outline;
    if (probe.emboldenPx > 0 && !outline.isEmpty()) {
      QPainterPathStroker stroker;
      stroker.setWidth(probe.emboldenPx);
      stroker.setJoinStyle(Qt::MiterJoin);
      outline = outline.united(stroker.createStroke(outline));
    }
    if (probe.italicShear != 0) outline = QTransform(1, 0, -probe.italicShear, 1, 0, 0).map(outline);
    g.outline = toEm.map(outline);
    const QChar ch = text.at(c.stringIndex);
    g.space = ch.isSpace();
    g.wordSeparator = isWordSeparator(ch);
    out.push_back(std::move(g));
  }
  return out;
}

}  // namespace rnqnx::views::rnsvg::text
